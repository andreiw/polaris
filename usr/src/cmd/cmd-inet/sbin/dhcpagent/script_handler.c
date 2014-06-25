/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)script_handler.c	1.3	05/06/08 SMI"

#include <time.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <dhcpmsg.h>
#include "script_handler.h"

/*
 * scripts are directly managed by a script helper process. dhcpagent creates
 * the helper process and it, in turn, creates a process to run the script
 * dhcpagent owns one end of a pipe and the helper process owns the other end
 * the helper process calls waitpid to wait for the script to exit. an alarm
 * is set for SCRIPT_TIMEOUT seconds. If the alarm fires, SIGTERM is sent to
 * the script process and a second alarm is set for SCRIPT_TIMEOUT_GRACE. if
 * the second alarm fires, SIGKILL is sent to forcefully kill the script. when
 * script exits, the helper process notifies dhcpagent by closing its end
 * of the pipe.
 */

unsigned int	script_count;

/*
 * the signal to send to the script process. it is a global variable
 * to this file as sigterm_handler needs it.
 */

static int	script_signal = SIGTERM;

/*
 * script's absolute timeout value. the first timeout is set to SCRIPT_TIMEOUT
 * seconds from the time it is started. SIGTERM is sent on the first timeout
 * the second timeout is set to SCRIPT_TIMEOUT_GRACE from the first timeout
 * and SIGKILL is sent on the second timeout.
 */
static time_t		timeout;

/*
 * sigalarm_handler(): signal handler for SIGARLM
 *
 *   input: int: signal the handler was called with
 *  output: void
 */

/* ARGSUSED */
static void
sigalarm_handler(int sig)
{
	time_t	now;

	/* set a another alarm if it fires too early */
	now = time(NULL);
	if (now < timeout)
		(void) alarm(timeout - now);
}

/*
 * sigterm_handler(): signal handler for SIGTERM, fired when dhcpagent wants
 *		      to stop the script
 *   input: int: signal the handler was called with
 *  output: void
 */

/* ARGSUSED */
static void
sigterm_handler(int sig)
{
	if (script_signal != SIGKILL) {
		/* send SIGKILL SCRIPT_TIMEOUT_GRACE seconds from now */
		script_signal = SIGKILL;
		timeout = time(NULL) + SCRIPT_TIMEOUT_GRACE;
		(void) alarm(SCRIPT_TIMEOUT_GRACE);
	}
}

/*
 * run_script(): it forks a process to execute the script
 *
 *   input: struct ifslist *: the interface
 *	    const char *: the event name
 *	    int: the pipe end owned by the script helper process
 *  output: void
 */
static void
run_script(struct ifslist *ifsp, const char *event, int fd)
{
	int		n;
	char		c;
	char		*name;
	pid_t		pid;
	time_t		now;
	extern int	errno;

	if ((pid = fork()) == -1) {
		return;
	}
	if (pid == 0) {
		name = strrchr(SCRIPT_PATH, '/') + 1;

		/* close all files */
		closefrom(0);

		/* redirect stdin, stdout and stderr to /dev/null */
		if ((n = open("/dev/null", O_RDWR)) < 0)
			exit(-1);

		(void) dup2(n, STDOUT_FILENO);
		(void) dup2(n, STDERR_FILENO);

		(void) execl(SCRIPT_PATH, name, ifsp->if_name, event, NULL);
		_exit(127);
	}

	/*
	 * the first timeout fires SCRIPT_TIMEOUT seconds from now.
	 */
	timeout = time(NULL) + SCRIPT_TIMEOUT;
	(void) sigset(SIGALRM, sigalarm_handler);
	(void) alarm(SCRIPT_TIMEOUT);

	/*
	 * pass script's pid to dhcpagent.
	 */
	(void) write(fd, &pid, sizeof (pid));

	for (;;) {
		if (waitpid(pid, NULL, 0) >= 0) {
			/* script has exited */
			c = SCRIPT_OK;
			break;
		}

		if (errno != EINTR) {
			return;
		}

		now = time(NULL);
		if (now >= timeout) {
			(void) kill(pid, script_signal);
			if (script_signal == SIGKILL) {
				c = SCRIPT_KILLED;
				break;
			}

			script_signal = SIGKILL;
			timeout = now + SCRIPT_TIMEOUT_GRACE;
			(void) alarm(SCRIPT_TIMEOUT_GRACE);
		}
	}

	(void) write(fd, &c, 1);
}

/*
 * script_cleanup(): cleanup helper function
 *
 *   input: struct ifslist *: the interface
 *  output: void
 */

static void
script_cleanup(struct ifslist *ifsp)
{
	ifsp->if_script_helper_pid = -1;
	ifsp->if_script_pid = -1;

	if (ifsp->if_script_fd != -1) {
		assert(ifsp->if_script_event_id != -1);
		assert(ifsp->if_script_callback != NULL);

		(void) iu_unregister_event(eh, ifsp->if_script_event_id, NULL);
		(void) close(ifsp->if_script_fd);
		ifsp->if_script_event_id = -1;
		ifsp->if_script_fd = -1;
		ifsp->if_script_callback(ifsp, ifsp->if_callback_msg);
		ifsp->if_script_callback = NULL;
		ifsp->if_script_event = NULL;
		ifsp->if_callback_msg = NULL;

		(void) release_ifs(ifsp);
		script_count--;
	}
}

/*
 * script_exit(): does cleanup and invokes callback when script exits
 *
 *   input: eh_t *: unused
 *	    int: the end of pipe owned by dhcpagent
 *	    short: unused
 *	    eh_event_id_t: unused
 *	    void *: the interface
 *  output: void
 */

/* ARGSUSED */
static void
script_exit(iu_eh_t *ehp, int fd, short events, iu_event_id_t id, void *arg)
{
	char		c;

	if (read(fd, &c, 1) <= 0) {
		c = SCRIPT_FAILED;
	}

	if (c == SCRIPT_OK) {
		dhcpmsg(MSG_DEBUG, "script ok");
	} else if (c == SCRIPT_KILLED) {
		dhcpmsg(MSG_DEBUG, "script killed");
	} else {
		dhcpmsg(MSG_DEBUG, "script failed");
	}

	script_cleanup(arg);
}

/*
 * script_start(): tries to run the script
 *
 *   input: struct ifslist *: the interface
 *	    const char *: the event name
 *	    script_callback_t: callback function
 *	    void *: data to the callback function
 *  output: int: 1 if script starts successfully
 *	    int *: the returned value of the callback function if script
 *		starts unsuccessfully
 */
int
script_start(struct ifslist *ifsp, const char *event,
		script_callback_t *callback, const char *msg, int *status)
{
	int		n;
	int		fds[2];
	pid_t		pid;
	iu_event_id_t	event_id;

	assert(callback != NULL);

	if (access(SCRIPT_PATH, X_OK) == -1) {
		/* script does not exist */
		goto out;
	}
	if (ifsp->if_script_pid != -1) {
		/* script is running, stop it */
		dhcpmsg(MSG_ERROR, "script_start: stop script");
		script_stop(ifsp);
	}

	/*
	 * dhcpagent owns one end of the pipe and script helper process
	 * owns the other end. dhcpagent reads on the pipe; and the helper
	 * process notifies it when the script exits.
	 */
	if (pipe(fds) < 0) {
		dhcpmsg(MSG_ERROR, "script_start: can't create pipe");
		goto out;
	}

	if ((pid = fork()) < 0) {
		dhcpmsg(MSG_ERROR, "script_start: can't fork");
		(void) close(fds[0]);
		(void) close(fds[1]);
		goto out;
	}

	if (pid == 0) {
		/*
		 * SIGCHLD is ignored in dhcpagent, the helper process
		 * needs it. it calls waitpid to wait for the script to exit.
		 */
		(void) close(fds[0]);
		(void) sigset(SIGCHLD, SIG_DFL);
		(void) sigset(SIGTERM, sigterm_handler);
		run_script(ifsp, event, fds[1]);
		exit(0);
	}

	(void) close(fds[1]);

	/* get the script's pid */
	if (read(fds[0], &ifsp->if_script_pid, sizeof (pid_t)) !=
	    sizeof (pid_t)) {
		(void) kill(pid, SIGKILL);
		ifsp->if_script_pid = -1;
		(void) close(fds[0]);
		goto out;
	}

	ifsp->if_script_helper_pid = pid;
	event_id = iu_register_event(eh, fds[0], POLLIN, script_exit, ifsp);
	if (event_id == -1) {
		(void) close(fds[0]);
		script_stop(ifsp);
		goto out;
	}

	script_count++;
	ifsp->if_script_event_id = event_id;
	ifsp->if_script_callback = callback;
	ifsp->if_script_event = event;
	ifsp->if_callback_msg = msg;
	ifsp->if_script_fd = fds[0];
	hold_ifs(ifsp);
	return (1);

out:
	/* callback won't be called in script_exit, so call it here */
	n = callback(ifsp, msg);
	if (status != NULL)
		*status = n;

	return (0);
}

/*
 * script_stop(): stops the script if it is running
 *
 *   input: struct ifslist *: the interface
 *  output: void
 */
void
script_stop(struct ifslist *ifsp)
{
	if (ifsp->if_script_pid != -1) {
		assert(ifsp->if_script_helper_pid != -1);

		/*
		 * sends SIGTERM to the script and asks the helper process
		 * to send SIGKILL if it does not exit after
		 * SCRIPT_TIMEOUT_GRACE seconds.
		 */
		(void) kill(ifsp->if_script_pid, SIGTERM);
		(void) kill(ifsp->if_script_helper_pid, SIGTERM);
	}

	script_cleanup(ifsp);
}

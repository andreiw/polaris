/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)spawn.c	1.5	06/04/23 SMI"

#include "lint.h"
#include "thr_uberdata.h"
#include <sys/procset.h>
#include <sys/rtpriocntl.h>
#include <sys/tspriocntl.h>
#include <sys/rt.h>
#include <sys/ts.h>
#include <alloca.h>
#include <spawn.h>
#include "rtsched.h"

#define	ALL_POSIX_SPAWN_FLAGS			\
		(POSIX_SPAWN_RESETIDS |		\
		POSIX_SPAWN_SETPGROUP |		\
		POSIX_SPAWN_SETSIGDEF |		\
		POSIX_SPAWN_SETSIGMASK |	\
		POSIX_SPAWN_SETSCHEDPARAM |	\
		POSIX_SPAWN_SETSCHEDULER)

typedef struct {
	short		sa_psflags;	/* POSIX_SPAWN_* flags */
	pri_t		sa_priority;
	int		sa_schedpolicy;
	pid_t		sa_pgroup;
	sigset_t	sa_sigdefault;
	sigset_t	sa_sigmask;
} spawn_attr_t;

typedef struct file_attr {
	struct file_attr *fa_next;	/* circular list of file actions */
	struct file_attr *fa_prev;
	enum {FA_OPEN, FA_CLOSE, FA_DUP2} fa_type;
	uint_t		fa_pathsize;	/* size of fa_path[] array */
	char		*fa_path;	/* copied pathname for open() */
	int		fa_oflag;	/* oflag for open() */
	mode_t		fa_mode;	/* mode for open() */
	int		fa_filedes;	/* file descriptor for open()/close() */
	int		fa_newfiledes;	/* new file descriptor for dup2() */
} file_attr_t;

extern struct pcclass ts_class, rt_class;

extern	pid_t	_vfork(void);
#pragma unknown_control_flow(_vfork)
extern	void	*_private_memset(void *, int, size_t);
extern	int	__lwp_sigmask(int, const sigset_t *, sigset_t *);
extern	int	__open(const char *, int, mode_t);
extern	int	__sigaction(int, const struct sigaction *, struct sigaction *);
extern	int	_private_close(int);
extern	int	_private_execve(const char *, char *const *, char *const *);
extern	int	_private_fcntl(int, int, intptr_t);
extern	int	_private_setgid(gid_t);
extern	int	_private_setpgid(pid_t, pid_t);
extern	int	_private_setuid(uid_t);
extern	int	_private_sigismember(sigset_t *, int);
extern	gid_t	_private_getgid(void);
extern	uid_t	_private_getuid(void);
extern	uid_t	_private_geteuid(void);
extern	void	_private_exit(int);

/*
 * We call this function rather than priocntl() because we must not call
 * any function that is exported from libc while in the child of vfork().
 * Also, we are not using PC_GETXPARMS or PC_SETXPARMS so we can use
 * the simple call to __priocntlset() rather than the varargs version.
 */
static long
_private_priocntl(idtype_t idtype, id_t id, int cmd, caddr_t arg)
{
	extern long _private__priocntlset(int, procset_t *, int, caddr_t, ...);
	procset_t procset;

	setprocset(&procset, POP_AND, idtype, id, P_ALL, 0);
	return (_private__priocntlset(PC_VERSION, &procset, cmd, arg, 0));
}

/*
 * The following two functions are blatently stolen from
 * sched_setscheduler() and sched_setparam() in librt.
 * This would be a lot easier if librt were folded into libc.
 */
static int
setscheduler(int policy, pri_t prio)
{
	pcparms_t	pcparm;
	tsinfo_t	*tsi;
	tsparms_t	*tsp;
	int		scale;

	switch (policy) {
	case SCHED_FIFO:
	case SCHED_RR:
		if (prio < rt_class.pcc_primin || prio > rt_class.pcc_primax) {
			errno = EINVAL;
			return (-1);
		}
		pcparm.pc_cid = rt_class.pcc_info.pc_cid;
		((rtparms_t *)pcparm.pc_clparms)->rt_pri = prio;
		((rtparms_t *)pcparm.pc_clparms)->rt_tqnsecs =
		    (policy == SCHED_RR ? RT_TQDEF : RT_TQINF);
		break;

	case SCHED_OTHER:
		pcparm.pc_cid = ts_class.pcc_info.pc_cid;
		tsi = (tsinfo_t *)ts_class.pcc_info.pc_clinfo;
		scale = tsi->ts_maxupri;
		tsp = (tsparms_t *)pcparm.pc_clparms;
		tsp->ts_uprilim = tsp->ts_upri = -(scale * prio) / 20;
		break;

	default:
		errno = EINVAL;
		return (-1);
	}

	return (_private_priocntl(P_PID, P_MYID,
	    PC_SETPARMS, (caddr_t)&pcparm));
}

static int
setparam(pcparms_t *pcparmp, pri_t prio)
{
	tsparms_t	*tsp;
	tsinfo_t	*tsi;
	int		scale;

	if (pcparmp->pc_cid == rt_class.pcc_info.pc_cid) {
		/* SCHED_FIFO or SCHED_RR policy */
		if (prio < rt_class.pcc_primin || prio > rt_class.pcc_primax) {
			errno = EINVAL;
			return (-1);
		}
		((rtparms_t *)pcparmp->pc_clparms)->rt_tqnsecs = RT_NOCHANGE;
		((rtparms_t *)pcparmp->pc_clparms)->rt_pri = prio;
	} else if (pcparmp->pc_cid == ts_class.pcc_info.pc_cid) {
		/* SCHED_OTHER policy */
		tsi = (tsinfo_t *)ts_class.pcc_info.pc_clinfo;
		scale = tsi->ts_maxupri;
		tsp = (tsparms_t *)pcparmp->pc_clparms;
		tsp->ts_uprilim = tsp->ts_upri = -(scale * prio) / 20;
	} else {
		errno = EINVAL;
		return (-1);
	}

	return (_private_priocntl(P_PID, P_MYID,
	    PC_SETPARMS, (caddr_t)pcparmp));
}

static void
perform_flag_actions(spawn_attr_t *sap)
{
	int sig;

	if (sap->sa_psflags & POSIX_SPAWN_SETSIGMASK) {
		(void) __lwp_sigmask(SIG_SETMASK, &sap->sa_sigmask, NULL);
	}

	if (sap->sa_psflags & POSIX_SPAWN_SETSIGDEF) {
		struct sigaction sigdfl;

		(void) _private_memset(&sigdfl, 0, sizeof (sigdfl));
		for (sig = 1; sig < NSIG; sig++) {
			if (_private_sigismember(&sap->sa_sigdefault, sig))
				(void) __sigaction(sig, &sigdfl, NULL);
		}
	}

	if (sap->sa_psflags & POSIX_SPAWN_RESETIDS) {
		if (_private_setgid(_private_getgid()) != 0 ||
		    _private_setuid(_private_getuid()) != 0)
			_private_exit(127);
	}

	if (sap->sa_psflags & POSIX_SPAWN_SETPGROUP) {
		if (_private_setpgid(0, sap->sa_pgroup) != 0)
			_private_exit(127);
	}

	if (sap->sa_psflags & POSIX_SPAWN_SETSCHEDULER) {
		if (setscheduler(sap->sa_schedpolicy, sap->sa_priority) != 0)
			_private_exit(127);
	} else if (sap->sa_psflags & POSIX_SPAWN_SETSCHEDPARAM) {
		/*
		 * Get the process's current scheduling parameters,
		 * then modify to set the new priority.
		 */
		pcparms_t pcparm;

		pcparm.pc_cid = PC_CLNULL;
		if (_private_priocntl(P_PID, P_MYID,
		    PC_GETPARMS, (caddr_t)&pcparm) == -1)
			_private_exit(127);
		if (setparam(&pcparm, sap->sa_priority) != 0)
			_private_exit(127);
	}
}

static void
perform_file_actions(file_attr_t *fap)
{
	file_attr_t *froot = fap;
	int fd;

	do {
		switch (fap->fa_type) {
		case FA_OPEN:
			fd = __open(fap->fa_path, fap->fa_oflag, fap->fa_mode);
			if (fd < 0)
				_private_exit(127);
			if (fd != fap->fa_filedes) {
				if (_private_fcntl(fd, F_DUP2FD,
				    fap->fa_filedes) < 0)
					_private_exit(127);
				(void) _private_close(fd);
			}
			break;
		case FA_CLOSE:
			if (_private_close(fap->fa_filedes) == -1)
				_private_exit(127);
			break;
		case FA_DUP2:
			fd = _private_fcntl(fap->fa_filedes, F_DUP2FD,
				fap->fa_newfiledes);
			if (fd < 0)
				_private_exit(127);
			break;
		}
	} while ((fap = fap->fa_next) != froot);
}

/*
 * For MT safety, do not invoke the dynamic linker after calling vfork().
 * If some other thread was in the dynamic linker when this thread's parent
 * called vfork() then the dynamic linker's lock would still be held here
 * (with a defunct owner) and we would deadlock ourself if we invoked it.
 *
 * Therefore, all of the functions we call here after returning from
 * _vfork() in the child are not and must never be exported from libc
 * as global symbols.  To do so would risk invoking the dynamic linker.
 */

#pragma weak posix_spawn = _posix_spawn
int
_posix_spawn(
	pid_t *pidp,
	const char *path,
	const posix_spawn_file_actions_t *file_actions,
	const posix_spawnattr_t *attrp,
	char *const argv[],
	char *const envp[])
{
	spawn_attr_t *sap = attrp? attrp->__spawn_attrp : NULL;
	file_attr_t *fap = file_actions? file_actions->__file_attrp : NULL;
	pid_t pid;

	if (attrp != NULL && sap == NULL)
		return (EINVAL);

	switch (pid = _vfork()) {
	case 0:			/* child */
		break;
	case -1:		/* parent, failure */
		return (errno);
	default:		/* parent, success */
		if (pidp != NULL)
			*pidp = pid;
		return (0);
	}

	if (sap != NULL)
		perform_flag_actions(sap);

	if (fap != NULL)
		perform_file_actions(fap);

	(void) _private_execve(path, argv, envp);
	_private_exit(127);
	return (0);	/* not reached */
}

/*
 * Much of posix_spawnp() blatently stolen from execvp() (port/gen/execvp.c).
 */

extern int __xpg4;	/* defined in xpg4.c; 0 if not xpg4-compiled program */

static const char *
execat(const char *s1, const char *s2, char *si)
{
	int cnt = PATH_MAX + 1;
	char *s;
	char c;

	for (s = si; (c = *s1) != '\0' && c != ':'; s1++) {
		if (cnt > 0) {
			*s++ = c;
			cnt--;
		}
	}
	if (si != s && cnt > 0) {
		*s++ = '/';
		cnt--;
	}
	for (; (c = *s2) != '\0' && cnt > 0; s2++) {
		*s++ = c;
		cnt--;
	}
	*s = '\0';
	return (*s1? ++s1: NULL);
}

#pragma weak posix_spawnp = _posix_spawnp
/* ARGSUSED */
int
_posix_spawnp(
	pid_t *pidp,
	const char *file,
	const posix_spawn_file_actions_t *file_actions,
	const posix_spawnattr_t *attrp,
	char *const argv[],
	char *const envp[])
{
	spawn_attr_t *sap = attrp? attrp->__spawn_attrp : NULL;
	file_attr_t *fap = file_actions? file_actions->__file_attrp : NULL;
	const char *pathstr = (strchr(file, '/') == NULL)? getenv("PATH") : "";
	int xpg4 = __xpg4;
	char path[PATH_MAX+4];
	const char *cp;
	pid_t pid;
	char **newargs;
	int argc;
	int i;
	static const char *sun_path = "/bin/sh";
	static const char *xpg4_path = "/usr/xpg4/bin/sh";
	static const char *shell = "sh";

	if (attrp != NULL && sap == NULL)
		return (EINVAL);

	/*
	 * We may need to invoke the shell with a slightly modified
	 * argv[] array.  To do this we need to preallocate the array.
	 * We must call alloca() before calling vfork() because doing
	 * it after vfork() (in the child) would corrupt the parent.
	 */
	for (argc = 0; argv[argc] != NULL; argc++)
		continue;
	newargs = alloca((argc + 2) * sizeof (char *));

	switch (pid = _vfork()) {
	case 0:			/* child */
		break;
	case -1:		/* parent, failure */
		return (errno);
	default:		/* parent, success */
		if (pidp != NULL)
			*pidp = pid;
		return (0);
	}

	if (*file == '\0')
		_private_exit(127);

	if (sap != NULL)
		perform_flag_actions(sap);

	if (fap != NULL)
		perform_file_actions(fap);

	if (pathstr == NULL) {
		/*
		 * XPG4:  pathstr is equivalent to _CS_PATH, except that
		 * :/usr/sbin is appended when root, and pathstr must end
		 * with a colon when not root.  Keep these paths in sync
		 * with _CS_PATH in confstr.c.  Note that pathstr must end
		 * with a colon when not root so that when file doesn't
		 * contain '/', the last call to execat() will result in an
		 * attempt to execv file from the current directory.
		 */
		if (_private_geteuid() == 0 || _private_getuid() == 0) {
			if (!xpg4)
				pathstr = "/usr/sbin:/usr/ccs/bin:/usr/bin";
			else
				pathstr = "/usr/xpg4/bin:/usr/ccs/bin:"
				    "/usr/bin:/opt/SUNWspro/bin:/usr/sbin";
		} else {
			if (!xpg4)
				pathstr = "/usr/ccs/bin:/usr/bin:";
			else
				pathstr = "/usr/xpg4/bin:/usr/ccs/bin:"
				    "/usr/bin:/opt/SUNWspro/bin:";
		}
	}

	cp = pathstr;
	do {
		cp = execat(cp, file, path);
		/*
		 * 4025035 and 4038378
		 * if a filename begins with a "-" prepend "./" so that
		 * the shell can't interpret it as an option
		 */
		if (*path == '-') {
			char *s;

			for (s = path; *s != '\0'; s++)
				continue;
			for (; s >= path; s--)
				*(s + 2) = *s;
			path[0] = '.';
			path[1] = '/';
		}
		(void) _private_execve(path, argv, envp);
		if (errno == ENOEXEC) {
			newargs[0] = (char *)shell;
			newargs[1] = path;
			for (i = 1; i <= argc; i++)
				newargs[i + 1] = argv[i];
			(void) _private_execve(xpg4? xpg4_path : sun_path,
				newargs, envp);
			_private_exit(127);
		}
	} while (cp);
	_private_exit(127);
	return (0);	/* not reached */
}

#pragma weak posix_spawn_file_actions_init = \
		_posix_spawn_file_actions_init
int
_posix_spawn_file_actions_init(
	posix_spawn_file_actions_t *file_actions)
{
	file_actions->__file_attrp = NULL;
	return (0);
}

#pragma weak posix_spawn_file_actions_destroy = \
		_posix_spawn_file_actions_destroy
int
_posix_spawn_file_actions_destroy(
	posix_spawn_file_actions_t *file_actions)
{
	file_attr_t *froot = file_actions->__file_attrp;
	file_attr_t *fap;
	file_attr_t *next;

	if ((fap = froot) != NULL) {
		do {
			next = fap->fa_next;
			if (fap-> fa_type == FA_OPEN)
				lfree(fap->fa_path, fap->fa_pathsize);
			lfree(fap, sizeof (*fap));
		} while ((fap = next) != froot);
	}
	file_actions->__file_attrp = NULL;
	return (0);
}

static void
add_file_attr(posix_spawn_file_actions_t *file_actions, file_attr_t *fap)
{
	file_attr_t *froot = file_actions->__file_attrp;

	if (froot == NULL) {
		fap->fa_next = fap->fa_prev = fap;
		file_actions->__file_attrp = fap;
	} else {
		fap->fa_next = froot;
		fap->fa_prev = froot->fa_prev;
		froot->fa_prev->fa_next = fap;
		froot->fa_prev = fap;
	}
}

#pragma weak posix_spawn_file_actions_addopen = \
		_posix_spawn_file_actions_addopen
int
_posix_spawn_file_actions_addopen(
	posix_spawn_file_actions_t *file_actions,
	int filedes,
	const char *path,
	int oflag,
	mode_t mode)
{
	file_attr_t *fap;

	if (filedes < 0)
		return (EBADF);
	if ((fap = lmalloc(sizeof (*fap))) == NULL)
		return (ENOMEM);

	fap->fa_pathsize = strlen(path) + 1;
	if ((fap->fa_path = lmalloc(fap->fa_pathsize)) == NULL) {
		lfree(fap, sizeof (*fap));
		return (ENOMEM);
	}
	(void) strcpy(fap->fa_path, path);

	fap->fa_type = FA_OPEN;
	fap->fa_oflag = oflag;
	fap->fa_mode = mode;
	fap->fa_filedes = filedes;
	add_file_attr(file_actions, fap);

	return (0);
}

#pragma weak posix_spawn_file_actions_addclose = \
		_posix_spawn_file_actions_addclose
int
_posix_spawn_file_actions_addclose(
	posix_spawn_file_actions_t *file_actions,
	int filedes)
{
	file_attr_t *fap;

	if (filedes < 0)
		return (EBADF);
	if ((fap = lmalloc(sizeof (*fap))) == NULL)
		return (ENOMEM);

	fap->fa_type = FA_CLOSE;
	fap->fa_filedes = filedes;
	add_file_attr(file_actions, fap);

	return (0);
}

#pragma weak posix_spawn_file_actions_adddup2 = \
		_posix_spawn_file_actions_adddup2
int
_posix_spawn_file_actions_adddup2(
	posix_spawn_file_actions_t *file_actions,
	int filedes,
	int newfiledes)
{
	file_attr_t *fap;

	if (filedes < 0 || newfiledes < 0)
		return (EBADF);
	if ((fap = lmalloc(sizeof (*fap))) == NULL)
		return (ENOMEM);

	fap->fa_type = FA_DUP2;
	fap->fa_filedes = filedes;
	fap->fa_newfiledes = newfiledes;
	add_file_attr(file_actions, fap);

	return (0);
}

#pragma weak posix_spawnattr_init = \
		_posix_spawnattr_init
int
_posix_spawnattr_init(
	posix_spawnattr_t *attr)
{
	spawn_attr_t *sap;

	if ((sap = lmalloc(sizeof (*sap))) == NULL)
		return (ENOMEM);

	/*
	 * Add default stuff here?
	 */
	attr->__spawn_attrp = sap;
	return (0);
}

#pragma weak posix_spawnattr_destroy = \
		_posix_spawnattr_destroy
int
_posix_spawnattr_destroy(
	posix_spawnattr_t *attr)
{
	spawn_attr_t *sap = attr->__spawn_attrp;

	if (sap == NULL)
		return (EINVAL);

	/*
	 * deallocate stuff here?
	 */
	lfree(sap, sizeof (*sap));
	attr->__spawn_attrp = NULL;
	return (0);
}

#pragma weak posix_spawnattr_setflags = \
		_posix_spawnattr_setflags
int
_posix_spawnattr_setflags(
	posix_spawnattr_t *attr,
	short flags)
{
	spawn_attr_t *sap = attr->__spawn_attrp;

	if (sap == NULL ||
	    (flags & ~ALL_POSIX_SPAWN_FLAGS))
		return (EINVAL);

	if (flags & (POSIX_SPAWN_SETSCHEDPARAM | POSIX_SPAWN_SETSCHEDULER)) {
		/*
		 * Populate ts_class and rt_class.
		 * We will need them in the child of vfork().
		 */
		if (rt_class.pcc_state == 0)
			(void) get_info_by_policy(SCHED_FIFO);
		if (ts_class.pcc_state == 0)
			(void) get_info_by_policy(SCHED_OTHER);
	}

	sap->sa_psflags = flags;
	return (0);
}

#pragma weak posix_spawnattr_getflags = \
		_posix_spawnattr_getflags
int
_posix_spawnattr_getflags(
	const posix_spawnattr_t *attr,
	short *flags)
{
	spawn_attr_t *sap = attr->__spawn_attrp;

	if (sap == NULL)
		return (EINVAL);

	*flags = sap->sa_psflags;
	return (0);
}

#pragma weak posix_spawnattr_setpgroup = \
		_posix_spawnattr_setpgroup
int
_posix_spawnattr_setpgroup(
	posix_spawnattr_t *attr,
	pid_t pgroup)
{
	spawn_attr_t *sap = attr->__spawn_attrp;

	if (sap == NULL)
		return (EINVAL);

	sap->sa_pgroup = pgroup;
	return (0);
}

#pragma weak posix_spawnattr_getpgroup = \
		_posix_spawnattr_getpgroup
int
_posix_spawnattr_getpgroup(
	const posix_spawnattr_t *attr,
	pid_t *pgroup)
{
	spawn_attr_t *sap = attr->__spawn_attrp;

	if (sap == NULL)
		return (EINVAL);

	*pgroup = sap->sa_pgroup;
	return (0);
}

#pragma weak posix_spawnattr_setschedparam = \
		_posix_spawnattr_setschedparam
int
_posix_spawnattr_setschedparam(
	posix_spawnattr_t *attr,
	const struct sched_param *schedparam)
{
	spawn_attr_t *sap = attr->__spawn_attrp;

	if (sap == NULL)
		return (EINVAL);

	/*
	 * Check validity?
	 */
	sap->sa_priority = schedparam->sched_priority;
	return (0);
}

#pragma weak posix_spawnattr_getschedparam = \
		_posix_spawnattr_getschedparam
int
_posix_spawnattr_getschedparam(
	const posix_spawnattr_t *attr,
	struct sched_param *schedparam)
{
	spawn_attr_t *sap = attr->__spawn_attrp;

	if (sap == NULL)
		return (EINVAL);

	schedparam->sched_priority = sap->sa_priority;
	return (0);
}

#pragma weak posix_spawnattr_setschedpolicy = \
		_posix_spawnattr_setschedpolicy
int
_posix_spawnattr_setschedpolicy(
	posix_spawnattr_t *attr,
	int schedpolicy)
{
	spawn_attr_t *sap = attr->__spawn_attrp;

	if (sap == NULL)
		return (EINVAL);

	switch (schedpolicy) {
	case SCHED_OTHER:
	case SCHED_FIFO:
	case SCHED_RR:
		break;
	default:
		return (EINVAL);
	}

	sap->sa_schedpolicy = schedpolicy;
	return (0);
}

#pragma weak posix_spawnattr_getschedpolicy = \
		_posix_spawnattr_getschedpolicy
int
_posix_spawnattr_getschedpolicy(
	const posix_spawnattr_t *attr,
	int *schedpolicy)
{
	spawn_attr_t *sap = attr->__spawn_attrp;

	if (sap == NULL)
		return (EINVAL);

	*schedpolicy = sap->sa_schedpolicy;
	return (0);
}

#pragma weak posix_spawnattr_setsigdefault = \
		_posix_spawnattr_setsigdefault
int
_posix_spawnattr_setsigdefault(
	posix_spawnattr_t *attr,
	const sigset_t *sigdefault)
{
	spawn_attr_t *sap = attr->__spawn_attrp;

	if (sap == NULL)
		return (EINVAL);

	sap->sa_sigdefault = *sigdefault;
	return (0);
}

#pragma weak posix_spawnattr_getsigdefault = \
		_posix_spawnattr_getsigdefault
int
_posix_spawnattr_getsigdefault(
	const posix_spawnattr_t *attr,
	sigset_t *sigdefault)
{
	spawn_attr_t *sap = attr->__spawn_attrp;

	if (sap == NULL)
		return (EINVAL);

	*sigdefault = sap->sa_sigdefault;
	return (0);
}

#pragma weak posix_spawnattr_setsigmask = \
		_posix_spawnattr_setsigmask
int
_posix_spawnattr_setsigmask(
	posix_spawnattr_t *attr,
	const sigset_t *sigmask)
{
	spawn_attr_t *sap = attr->__spawn_attrp;

	if (sap == NULL)
		return (EINVAL);

	sap->sa_sigmask = *sigmask;
	return (0);
}

#pragma weak posix_spawnattr_getsigmask = \
		_posix_spawnattr_getsigmask
int
_posix_spawnattr_getsigmask(
	const posix_spawnattr_t *attr,
	sigset_t *sigmask)
{
	spawn_attr_t *sap = attr->__spawn_attrp;

	if (sap == NULL)
		return (EINVAL);

	*sigmask = sap->sa_sigmask;
	return (0);
}

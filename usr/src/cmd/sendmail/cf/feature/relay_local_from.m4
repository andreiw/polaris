divert(-1)
#
# Copyright (c) 1998-1999, 2001 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#ident	"@(#)relay_local_from.m4	1.2	01/08/27 SMI"
#

divert(0)
VERSIONID(`$Id: relay_local_from.m4,v 8.6 2001/02/06 15:55:21 ca Exp $')
divert(-1)

define(`_RELAY_LOCAL_FROM_', 1)
errprint(`*** WARNING: FEATURE(`relay_local_from') may cause your system to act as open
	relay.  Use SMTP AUTH or STARTTLS instead. If you cannot use those,
	try FEATURE(`relay_mail_from').
')

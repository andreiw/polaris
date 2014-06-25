#ifndef KRB5_LIBINIT_H
#define KRB5_LIBINIT_H

#pragma ident	"@(#)krb5_libinit.h	1.1	05/09/26 SMI"

#include "krb5.h"

krb5_error_code krb5int_initialize_library (void);
void krb5int_cleanup_library (void);

#endif /* KRB5_LIBINIT_H */

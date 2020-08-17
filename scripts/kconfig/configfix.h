#ifndef CONFIGFIX_H
#define CONFIGFIX_H

/* make functions accessible from xconfig */
#ifdef __cplusplus
extern "C" {
#endif

/* include internal definitions */
#define LKC_DIRECT_LINK
#include "lkc.h"


/* include glib */
#include <glib.h>


/* include own definitions */
#include "cfdefs.h"


/* include other header files needed */
#include "picosat.h"
#include "cfconstraints.h"
#include "cffexpr.h"
#include "cfprint.h"
#include "cfrangefix.h"
#include "cfsatutils.h"
#include "cfutils.h"


/* external functions */
GArray * run_satconf(GArray *arr);

int apply_satfix(GArray *fix);

int run_satconf_cli(const char *Kconfig_file);


/* make functions accessible from xconfig */
#ifdef __cplusplus
}
#endif

#endif

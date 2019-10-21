#ifndef SATCONF_H
#define SATCONF_H

/* make functions accessible from xconfig */
#ifdef __cplusplus
extern "C" {
#endif

/* include internal definitions */
#define LKC_DIRECT_LINK
#include "../lkc.h"


/* include glib */
#include <glib.h>


/* include own definitions */
#include "defs.h"


/* include other header files needed */
#include "picosat.h"
#include "cnf.h"
#include "constraints.h"
#include "fexpr.h"
#include "print.h"
#include "rangefix.h"
#include "satutils.h"
#include "symbol_mod.h"
#include "utils.h"


/* external functions */
GArray * run_satconf(struct symbol_dvalue *sdv);

int run_satconf_cli(const char *Kconfig_file);

char * get_test_char(void);


/* make functions accessible from xconfig */
#ifdef __cplusplus
}
#endif

#endif

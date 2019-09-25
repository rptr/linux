#ifndef RANGEFIX_H
#define RANGEFIX_H

#include "../satconfig.h"
#include "picosat.h"

/* initialize RangeFix and return the diagnoses */
GArray * rangefix_init(PicoSAT *pico);

/* ask user which fix to apply */
GArray * choose_fix(GArray *diag);

/* aplly the fix */
void apply_fix(GArray *diag);

#endif

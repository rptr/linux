#ifndef SYMBOL_MOD_H
#define SYMBOL_MOD_H

#include "../lkc.h"

bool sym_set_tristate_value_mod(struct symbol *sym, tristate val);
bool sym_tristate_within_range_mod(struct symbol *sym, tristate val);

#endif

#ifndef UTILS_H
#define UTILS_H

#include "satconf.h"
#include "picosat.h"

void init_data(void);

void assign_sat_variables(void);
void create_constants(void);

struct fexpr * create_fexpr(int satval, enum fexpr_type type, char *name);

struct cnf_clause * build_cnf_clause(struct gstr *reason, int num, ...);
struct cnf_clause * create_cnf_clause_struct(void);

void add_literal_to_clause(struct cnf_clause *cl, int val);

char * get_tmp_var_as_char(int i);
char * tristate_get_char(tristate val);

bool can_evaluate_to_mod(struct k_expr *e);
struct k_expr * get_const_false_as_kexpr(void);
struct k_expr * get_const_true_as_kexpr(void);

struct fexpr * get_fexpr_from_satmap(int key);

struct k_expr * parse_expr(struct expr *e, struct k_expr *parent);

bool is_tristate_constant(struct symbol *sym);
bool sym_is_boolean(struct symbol *sym);
bool sym_is_bool_or_triconst(struct symbol *sym);
bool sym_is_nonboolean(struct symbol *sym);

void sym_add_constraint(struct symbol *sym, struct fexpr *constraint);
void sym_add_assumption(PicoSAT *pico, struct symbol *sym);
void sym_add_assumption_tri(PicoSAT *pico, struct symbol *sym, tristate tri_val);
void sym_warn_unmet_dep(struct symbol *sym);

#endif

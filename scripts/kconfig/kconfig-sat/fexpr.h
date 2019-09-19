#ifndef FEXPR_H
#define FEXPR_H

#include "../satconfig.h"

void sym_create_fexpr (struct symbol *sym);

struct fexpr * calculate_fexpr_both(struct k_expr *e);
struct fexpr * calculate_fexpr_y(struct k_expr *e);
struct fexpr * calculate_fexpr_m(struct k_expr *e);
struct fexpr * calculate_fexpr_y_and(struct k_expr *a, struct k_expr *b);
struct fexpr * calculate_fexpr_m_and(struct k_expr *a, struct k_expr *b);
struct fexpr * calculate_fexpr_y_or(struct k_expr *a, struct k_expr *b);
struct fexpr * calculate_fexpr_m_or(struct k_expr *a, struct k_expr *b);
struct fexpr * calculate_fexpr_y_not(struct k_expr *a);
struct fexpr * calculate_fexpr_m_not(struct k_expr *a);
struct fexpr * calculate_fexpr_y_equals(struct k_expr *a);
struct fexpr * calculate_fexpr_y_unequals(struct k_expr *a);

struct fexpr * fexpr_and(struct fexpr *a, struct fexpr *b);
struct fexpr * fexpr_or(struct fexpr *a, struct fexpr *b);
struct fexpr * fexpr_not(struct fexpr *a);
struct fexpr * sym_get_fexpr_both(struct symbol *sym);
struct fexpr * sym_get_nonbool_fexpr(struct symbol *sym, char *value);
struct fexpr * implies(struct fexpr *a, struct fexpr *b);

bool fexpr_is_symbol(struct fexpr *e);
bool fexpr_is_symbol_or_not(struct fexpr *e);

void convert_fexpr_to_nnf(struct fexpr *e);
void convert_fexpr_to_cnf(struct fexpr *e);

void unfold_cnf_clause(struct fexpr *e);

#endif

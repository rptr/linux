/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Patrick Franz <patfra71@gmail.com>
 */

#ifndef CF_EXPR_H
#define CF_EXPR_H

/* create a fexpr */
struct fexpr * fexpr_create(int satval, enum fexpr_type type, char *name);

/* create the fexpr for a symbol */
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

struct pexpr * calculate_pexpr_both(struct k_expr *e);
struct pexpr * calculate_pexpr_y(struct k_expr *e);
struct pexpr * calculate_pexpr_m(struct k_expr *e);
struct pexpr * calculate_pexpr_y_and(struct k_expr *a, struct k_expr *b);
struct pexpr * calculate_pexpr_m_and(struct k_expr *a, struct k_expr *b);
struct pexpr * calculate_pexpr_both_and(struct k_expr *a, struct k_expr *b);
struct pexpr * calculate_pexpr_y_or(struct k_expr *a, struct k_expr *b);
struct pexpr * calculate_pexpr_m_or(struct k_expr *a, struct k_expr *b);
struct pexpr * calculate_pexpr_both_or(struct k_expr *a, struct k_expr *b);
struct pexpr * calculate_pexpr_y_not(struct k_expr *a);
struct pexpr * calculate_pexpr_m_not(struct k_expr *a);
struct pexpr * calculate_pexpr_y_equals(struct k_expr *a);
struct pexpr * calculate_pexpr_y_unequals(struct k_expr *a);


/* macro to create a fexpr of type AND */
struct fexpr * fexpr_and(struct fexpr *a, struct fexpr *b);

/* macro to create a pexpr of type AND */
struct pexpr * pexpr_and(struct pexpr *a, struct pexpr *b);

/* macro to create a fexpr of type OR */
struct fexpr * fexpr_or(struct fexpr *a, struct fexpr *b);

/* macro to create a pexpr of type OR */
struct pexpr * pexpr_or(struct pexpr *a, struct pexpr *b);

/* macro to create a fexpr of type NOT */
struct fexpr * fexpr_not(struct fexpr *a);

/* macro to create a pexpr of type NOT */
struct pexpr * pexpr_not(struct pexpr *a);

/* check whether a fexpr is in CNF */
bool fexpr_is_cnf(struct fexpr *e);

/* check whether a pexpr is in CNF */
bool pexpr_is_cnf(struct pexpr *e);

/* return fexpr_both for a symbol */
struct fexpr * sym_get_fexpr_both(struct symbol *sym);

/* return fexpr_sel_y for a symbol */
struct fexpr * sym_get_fexpr_sel_y(struct symbol *sym);

/* return fexpr_sel_m for a symbol */
struct fexpr * sym_get_fexpr_sel_m(struct symbol *sym);

/* return fexpr_sel_both for a symbol */
struct fexpr * sym_get_fexpr_sel_both(struct symbol *sym);

/* create the fexpr of a non-boolean symbol for a specific value */
struct fexpr * sym_create_nonbool_fexpr(struct symbol *sym, char *value);

/* return the fexpr of a non-boolean symbol for a specific value, NULL if non-existent */
struct fexpr * sym_get_nonbool_fexpr(struct symbol *sym, char *value);

/*
 * return the fexpr of a non-boolean symbol for a specific value, if it exists
 * otherwise create it
 */
struct fexpr * sym_get_or_create_nonbool_fexpr(struct symbol *sym, char *value);

/* macro to construct a fexpr for "A implies B" */
struct fexpr * implies_fexpr(struct fexpr *a, struct fexpr *b);

/* macro to construct a pexpr for "A implies B" */
struct pexpr * pexpr_implies(struct pexpr *a, struct pexpr *b);

/* check, if the fexpr is a symbol, a True/False-constant, a literal symbolizing a non-boolean or a choice symbol */
bool fexpr_is_symbol(struct fexpr *e);

/* check, if the fexpr is a symbol, a True/False-constant, a literal symbolizing a non-boolean, a choice symbol or NOT */
bool fexpr_is_symbol_or_not(struct fexpr *e);

/* check, if a fexpr is a symbol, a True/False-constant, a literal symbolizing a non-boolean, a choice symbol or a negated "symbol" */
bool fexpr_is_symbol_or_neg_atom(struct fexpr *e);

/* check whether a pexpr is a symbol or a negated symbol */
bool pexpr_is_symbol(struct pexpr *e);

/* check whether the fexpr is a constant (true/false) */
bool fexpr_is_constant(struct fexpr *e);

/* print an fexpr */
void fexpr_print(char *tag, struct fexpr *e, int parent);

/* write an fexpr into a string (format needed for testing) */
void fexpr_as_char(struct fexpr *e, struct gstr *s, int parent);

/* write an fexpr into a string */
void fexpr_as_char_short(struct fexpr *e, struct gstr *s, int parent);

/* write pn pexpr into a string */
void pexpr_as_char_short(struct pexpr *e, struct gstr *s, int parent);

/* check whether 2 pexpr are equal */
bool pexpr_eq(struct pexpr *e1, struct pexpr *e2);

/* copy a pexpr */
struct pexpr * pexpr_copy(const struct pexpr *org);

/* free a pexpr */
void pexpr_free(struct pexpr *e);

/* print a pexpr  */
void pexpr_print(char *tag, struct pexpr *e, int prevtoken);

/* convert a fexpr to a pexpr */
struct pexpr * pexf(struct fexpr *fe);

/* eliminate duplicate and redundant operands */
struct pexpr * pexpr_eliminate_dups(struct pexpr *e);

#endif

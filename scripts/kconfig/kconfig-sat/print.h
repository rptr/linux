#ifndef PRINT_H
#define PRINT_H

#include "satconf.h"

#define OUTFILE_DIMACS "./scripts/kconfig/kconfig-sat/out_cnf.dimacs"
#define OUTFILE_FEXPR "./scripts/kconfig/kconfig-sat/out_constraints"

/* print all symbols */
void print_all_symbols(void);

/* print the symbol */
void print_symbol(struct symbol *sym);

/* print a default value for a property */
void print_default(struct property *p);

/* print a select statement for a property */
void print_select(struct property *p);

/* print an imply statement for a property */
void print_imply(struct property *p);

/* print an expr */
void print_expr(struct expr *e, int prevtoken);

/* print some debug info about the tree structure of k_expr */
void debug_print_kexpr(struct k_expr *e);

/* print a kexpr */
void print_kexpr(struct k_expr *e);

/* print an fexpr */
void print_fexpr(struct fexpr *e, int parent);


void kexpr_as_char(struct k_expr *e, struct gstr *s);

void fexpr_as_char(struct fexpr *e, struct gstr *s, int parent);

void fexpr_as_char_short(struct fexpr *e, struct gstr *s, int parent);

void print_cnf_clause(struct cnf_clause *cl);

void print_all_cnf_clauses(GArray *cnf_clauses);

void print_sym_constraint(struct symbol *sym);

void print_satmap(gpointer key, gpointer value, gpointer userData);

void write_cnf_to_file(GArray *cnf_clauses, int sat_variable_nr, int nr_of_clauses);

void write_constraints_to_file(void);

#endif

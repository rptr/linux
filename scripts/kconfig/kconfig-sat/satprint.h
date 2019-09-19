#ifndef SATPRINT_H
#define SATPRINT_H

#include "../satconfig.h"

#define OUTFILE_DIMACS "./scripts/kconfig/kconfig-sat/out_cnf.dimacs"
#define OUTFILE_FEXPR "./scripts/kconfig/kconfig-sat/out_constraints"

void print_all_symbols(void);

void print_symbol(struct symbol *sym);

void print_default(struct symbol *sym, struct property *p);

void print_select(struct symbol *sym, struct property *p);

void print_imply(struct symbol *sym, struct property *p);

void print_expr(struct expr *e, int prevtoken);

void debug_print_kexpr(struct k_expr *e);

void print_kexpr(struct k_expr *e);

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

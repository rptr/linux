#ifndef SATPRINT_H
#define SATPRINT_H

void print_symbol(struct symbol *sym);

void print_default(struct symbol *sym, struct property *p);

void print_select(struct symbol *sym, struct property *p);

void print_imply(struct symbol *sym, struct property *p);

void print_expr(struct expr *e, int prevtoken);

void print_cnf_clauses(struct cnf_clause *cnf_clauses);

#endif

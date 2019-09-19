#ifndef TSEYTIN_H
#define TSEYTIN_H

void construct_cnf_clauses(void);

void build_cnf(struct fexpr *e);

///////////////////////////////////////////////////////////

// void build_cnf_tmp_or(struct tmp_sat_variable *t, struct k_expr *e);
// void build_cnf_tmp_or_yes(struct tmp_sat_variable *t, struct k_expr *e);
// void build_cnf_tmp_or_mod(struct tmp_sat_variable *t, struct k_expr *e);
// 
// void build_cnf_tmp_and(struct symbol *sym, struct tmp_sat_variable *tvar, struct k_expr *e);
// 
// void build_cnf_tmp_not(struct symbol *sym, struct tmp_sat_variable *tvar, struct k_expr *e);

// struct tmp_sat_variable * create_tmp_sat_var(struct tmp_sat_variable *parent);
// void create_tmp_sat_var_yesmod(struct tmp_sat_variable *t, int create_clauses);

#endif

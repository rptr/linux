#ifndef CNF_H
#define CNF_H

/* construct the CNF-clauses from the constraints */
void construct_cnf_clauses(void);

/* check, if a CNF-clause is a tautology */
bool cnf_is_tautology(struct cnf_clause *cl);

///////////////////////////////////////////////////////////


// void build_cnf(struct fexpr *e);
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

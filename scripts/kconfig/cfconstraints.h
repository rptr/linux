#ifndef CONSTRAINTS_H
#define CONSTRAINTS_H

/* build the constraints for each symbol */
void get_constraints(void);

/* count the number of all constraints */
unsigned int count_counstraints(void);

/* add a constraint for a symbol */
void sym_add_constraint(struct symbol *sym, struct fexpr *constraint);

#endif

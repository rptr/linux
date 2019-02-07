#ifndef SATCONFIG_H
#define SATCONFIG_H

/* struct for a CNF-clause */
struct cnf_clause {
	/* first literal in the CNF clause */
	struct cnf_literal *lit;
	
	/* next clause - null if last */
	struct cnf_clause *next;
};

/* struct for a literal in a CNF-clause */
struct cnf_literal {
	/* integer value needed by the SAT-solver */
	int val;
	
	/* string representation for debugging */
	char sval[100];
	
	/* next literal in the clause - null if last */
	struct cnf_literal *next;
};

/* different types for pl_expr */
enum kexpr_type {
	KET_SYMBOL,
	KET_AND,
	KET_OR,
	KET_NOT
};

/* struct for an expression, built like a tree structure */
struct k_expr {
	struct k_expr *parent;
	enum kexpr_type type;
	
	union {
		/* symbol */
		struct symbol *sym;
		/* NOT */
		struct {
			struct k_expr *child;
		};
		/* AND, OR */
		struct {
			struct k_expr *left;
			struct k_expr *right;
		};
	};
	
};

#endif

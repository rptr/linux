#ifndef SATCONFIG_H
#define SATCONFIG_H
#define CNF_REASON_LENGTH 256
#define CNF_LITERAL_LENGTH 100
#define PRINT_CNF_REASONS true

/* struct for a CNF-clause */
struct cnf_clause {
	/* first literal in the CNF clause */
	struct cnf_literal *lit;

	/* reason - string refers to a specific boolean law that produced this clause */
	char reason[CNF_REASON_LENGTH];

	/* next clause - null if last */
	struct cnf_clause *next;
};

/* struct for a literal in a CNF-clause */
struct cnf_literal {
	/* integer value needed by the SAT-solver */
	int val;

	/* string representation for debugging */
	char sval[CNF_LITERAL_LENGTH];

	/* next literal in the clause - null if last */
	struct cnf_literal *next;
};

/* different types for k_expr */
enum kexpr_type {
	KE_SYMBOL,
	KE_AND,
	KE_OR,
	KE_NOT
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

#ifndef SATCONFIG_H
#define SATCONFIG_H

struct cnf_clause {
	/* first literal in the CNF clause */
	struct cnf_literal *lit;
	
	/* next clause for this symbol - null if last */
	struct cnf_clause *next;
};

struct cnf_literal {
	/* integer value needed by the SAT-solver */
	int val;
	
	/* string representation for debugging */
	char sval[100];
	
	/* next literal in the clause - null if last */
	struct cnf_literal *next;
};

#endif

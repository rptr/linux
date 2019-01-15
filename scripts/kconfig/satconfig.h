#ifndef SATCONFIG_H
#define SATCONFIG_H

struct cnf_clause {
	struct cnf_literal *lit;
	struct cnf_clause *next;
};

struct cnf_literal {
	int val;
	char sval[100];
	struct cnf_literal *next;
};

#endif

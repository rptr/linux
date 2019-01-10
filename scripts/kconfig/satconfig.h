#ifndef SATCONFIG_H
#define SATCONFIG_H

struct cnf_clause {
	char clause[100];
	struct cnf_clause *next;
};

#endif

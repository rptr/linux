#define _GNU_SOURCE
#include <assert.h>
#include <locale.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <glib.h>

#define LKC_DIRECT_LINK
#include "../lkc.h"

#include "satconf.h"
#include "picosat.h"
#include "fexpr.h"
#include "print.h"
#include "utils.h"
#include "cnf.h"
#include "constraints.h"
#include "rangefix.h"
#include "satutils.h"

unsigned int sat_variable_nr = 1;
unsigned int tmp_variable_nr = 1;

GHashTable *satmap = NULL; /* hash table with all SAT-variables and their fexpr */
GArray *cnf_clauses; /* array with all CNF-clauses */
struct tmp_sat_variable *tmp_sat_vars;

unsigned int nr_of_clauses = 0; /* number of CNF-clauses */

struct fexpr *const_false; /* constant False */
struct fexpr *const_true; /* constant True */

/* -------------------------------------- */

int run_satconf_cli(const char *Kconfig_file)
{
// 	printf("\nHello satconfig!\n\n");

	printf("Init...");
	/* measure time for constructing constraints and clauses */
	clock_t start, end;
	double time;
	start = clock();
	
	/* parse Kconfig-file and read .config */
	init_config(Kconfig_file);

	/* initialize satmap and cnf_clauses */
	init_data();
	
	/* creating constants */
	create_constants();
	
	/* assign SAT variables & create sat_map */
	assign_sat_variables();
	
	/* get the constraints */
	get_constraints();
	
	/* print all symbols and its constraints */
// 	print_all_symbols();
	
	/* construct the CNF clauses */
	construct_cnf_clauses();

	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	
	/* print all symbols and its constraints */
// 	print_all_symbols();
	
	/* write constraints to file */
	write_constraints_to_file();
	
	/* print all CNFs */
// 	printf("All CNFs:\n");
// 	print_all_cnf_clauses( cnf_clauses );
	
	printf("Generating constraints and clauses...done. (%.6f secs.)\n", time);
	
	/* print the satmap */
// 	g_hash_table_foreach(satmap, print_satmap, NULL);
	
	/* start PicoSAT */
	PicoSAT *pico = initialize_picosat();
	picosat_add_clauses(pico);
	picosat_solve(pico);

	/* 
	 * write CNF-clauses in DIMACS-format to file 
	 * NOTE: this output is without the unit-clauses
	 */
// 	write_cnf_to_file(cnf_clauses, sat_variable_nr, nr_of_clauses);
	
	return EXIT_SUCCESS;
}

GArray * run_satconf(struct symbol_dvalue *sdv)
{
	printf("\n");
	printf("Init...");
	/* measure time for constructing constraints and clauses */
	clock_t start, end;
	double time;
	start = clock();

	/* initialize satmap and cnf_clauses */
	init_data();
	
	/* creating constants */
	create_constants();
	
	/* assign SAT variables & create sat_map */
	assign_sat_variables();
	
	/* get the constraints */
	get_constraints();
	
	/* construct the CNF clauses */
	construct_cnf_clauses();
	
	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	
	printf("Generating constraints and clauses...done. (%.6f secs.)\n", time);
	
	/* start PicoSAT */
	PicoSAT *pico = initialize_picosat();
	picosat_add_clauses(pico);
	
	/* add unit clauses for symbol */
	if (sym_get_type(sdv->sym) == S_BOOLEAN) {
		switch (sdv->tri) {
		case yes:
			picosat_add_arg(pico, sdv->sym->fexpr_y->satval, 0);
			break;
		case no:
			picosat_add_arg(pico, -(sdv->sym->fexpr_y->satval), 0);
			break;
		case mod:
			perror("Should not happen.\n");
		}
	} else if (sym_get_type(sdv->sym) == S_TRISTATE) {
		switch (sdv->tri) {
		case yes:
			picosat_add_arg(pico, sdv->sym->fexpr_y->satval, 0);
			picosat_add_arg(pico, -(sdv->sym->fexpr_m->satval), 0);
			break;
		case mod:
			picosat_add_arg(pico, -(sdv->sym->fexpr_y->satval), 0);
			picosat_add_arg(pico, sdv->sym->fexpr_m->satval, 0);
			break;
		case no:
			picosat_add_arg(pico, -(sdv->sym->fexpr_y->satval), 0);
			picosat_add_arg(pico, -(sdv->sym->fexpr_m->satval), 0);
		}
	}
	
// 	picosat_solve(pico);
	printf("Solving SAT-problem...");
	start = clock();
	
	int res = picosat_sat(pico, -1);
	
	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("done. (%.6f secs.)\n\n", time);
	
	if (res == PICOSAT_SATISFIABLE) {
		printf("===> Problem is satisfiable <===\n");
		
		return g_array_new(false, false, sizeof(GArray *));
		
// 		struct symbol *sym;
// 		unsigned int i;
// 		bool found = false;
// 		
// 		/* check, if a symbol has been selected, but has unmet dependencies */
// 		for_all_symbols(i, sym) {
// 			if (sym->dir_dep.tri < sym->rev_dep.tri) {
// 				found = true;
// 				sym_warn_unmet_dep(sym);
// 				add_select_constraints(pico, sym);
// 			}
// 		}
// 		if (!found) return;
// 		printf("\n");
// 		
// 		/* add assumptions again */
// 		for_all_symbols(i, sym)
// 			sym_add_assumption(pico, sym);
// 		
// 		int res = picosat_sat(pico, -1);
// 		
// 		/* problem should be unsatisfiable now */
// 		if (res == PICOSAT_UNSATISFIABLE) {
// 			return rangefix_init(pico);
// 		} else {
// 			printf("SOMETHING IS A MISS. PROBLEM IS NOT UNSATISFIABLE.\n");
// 			return NULL;
// 		}
		
// 		printf("All CNFs:\n");
// 		print_all_cnf_clauses( cnf_clauses );
		
	} else if (res == PICOSAT_UNSATISFIABLE) {
		printf("===> PROBLEM IS UNSATISFIABLE <===\n");
		printf("\n");
		return rangefix_init(pico);
	}
	else {
		printf("Unknown if satisfiable.\n");
		return NULL;
	}
		
	return NULL;
}

/*
 * test function
 */
char * get_test_char(void)
{
	return "kconfig-sat";
}

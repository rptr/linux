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
#include "lkc.h"

#include "satconfig.h"
#include "kconfig-sat/picosat.h"
#include "kconfig-sat/fexpr.h"
#include "kconfig-sat/print.h"
#include "kconfig-sat/utils.h"
#include "kconfig-sat/cnf.h"
#include "kconfig-sat/constraints.h"
#include "kconfig-sat/rangefix.h"
#include "kconfig-sat/satutils.h"

unsigned int sat_variable_nr = 1;
unsigned int tmp_variable_nr = 1;

GHashTable *satmap = NULL; /* hash table with all SAT-variables and their fexpr */
GArray *cnf_clauses; /* array with all CNF-clauses */
struct tmp_sat_variable *tmp_sat_vars;

unsigned int nr_of_clauses = 0; /* number of CNF-clauses */

struct fexpr *const_false; /* constant False */
struct fexpr *const_true; /* constant True */

static PicoSAT *pico; /* SAT-solver */

static void init_config(const char *Kconfig_file);

/* -------------------------------------- */


int main(int argc, char *argv[])
{
	printf("\nHello satconfig!\n\n");

	printf("Init...");
	/* measure time for constructing constraints and clauses */
	clock_t start, end;
	double time;
	start = clock();
	
	/* parse Kconfig-file and read .config */
	init_config(argv[1]);

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
	pico = initialize_picosat();
	picosat_add_clauses(pico);
	picosat_solve(pico);

	/* 
	 * write CNF-clauses in DIMACS-format to file 
	 * NOTE: this output is without the unit-clauses
	 */
// 	write_cnf_to_file(cnf_clauses, sat_variable_nr, nr_of_clauses);
	
	return EXIT_SUCCESS;
}

/*
 * parse Kconfig-file and read .config
 */
static void init_config (const char *Kconfig_file)
{
	conf_parse(Kconfig_file);
	conf_read(NULL);
}

/*
 * test function
 */
char * get_test_char()
{
	return "kconfig-sat";
}

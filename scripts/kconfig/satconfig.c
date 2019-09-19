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
#include "kconfig-sat/fexpr.h"
#include "kconfig-sat/satprint.h"
#include "kconfig-sat/satutils.h"
#include "kconfig-sat/cnf.h"
#include "kconfig-sat/constraints.h"
#include "kconfig-sat/picosat.h"
#include "kconfig-sat/rangefix.h"

#define DEBUG_KEXPR false
#define TEST_OUTPUT true

unsigned int sat_variable_nr = 1;
unsigned int tmp_variable_nr = 1;

GHashTable *satmap = NULL; /* hash table with all SAT-variables and their fexpr */
GArray *cnf_clauses; /* array with all CNF-clauses */
struct tmp_sat_variable *tmp_sat_vars;

unsigned int nr_of_clauses = 0; /* number of CNF-clauses */

struct fexpr *const_false; /* constant False */
struct fexpr *const_true; /* constant True */

static PicoSAT *pico;
static int start_core;

static void initialize_picosat(void);
static void picosat_add_clauses(void);
static void picosat_solve(void);

static void run_unsat_problem(PicoSAT *pico);
static void add_select_constraints(struct symbol *sym);

/* -------------------------------------- */


const char *expr_type[] = {
	"E_NONE", "E_OR", "E_AND", "E_NOT",
	"E_EQUAL", "E_UNEQUAL", "E_LTH", "E_LEQ", "E_GTH", "E_GEQ",
	"E_LIST", "E_SYMBOL", "E_RANGE"
};
const char *kexpr_type[] = {
	"PLT_SYMBOL",
	"PLT_AND",
	"PLT_OR",
	"PLT_NOT"
};
const char *tristate_type[] = {"no", "mod", "yes"};


int main(int argc, char *argv[])
{
	printf("\nHello satconfig!\n\n");

	printf("Init...");
	/* measure time for constructing constraints and clauses */
	clock_t start, end;
	double time;
	start = clock();
	
	/* parse KConfig-file and read .config */
	const char *Kconfig_file = argv[1];
	conf_parse(Kconfig_file);
	conf_read(NULL);

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
	//g_hash_table_foreach(satmap, print_satmap, NULL);
	
	/* start PicoSAT */
	initialize_picosat();
	picosat_add_clauses();
	picosat_solve();

	/* 
	 * write CNF-clauses in DIMACS-format to file 
	 * NOTE: this output is without the unit-clauses
	 */
	write_cnf_to_file(cnf_clauses, sat_variable_nr, nr_of_clauses);
	
	return EXIT_SUCCESS;
}


/*
 * initialize the PicoSAT variable
 */
static void initialize_picosat()
{
	// TODO
	printf("\nInitializing PicoSAT...\n");
	pico = picosat_init();
	picosat_enable_trace_generation(pico);
}

/*
 * add clauses to the PicoSAT
 */
static void picosat_add_clauses()
{
	printf("Adding clauses...");
	clock_t start, end;
	double time;
	start = clock();

	/* add CNF-clauses to PicoSAT
	 * ignore tautologies */
	struct cnf_clause *cl;
	unsigned int i, j;
	for (i = 0; i < cnf_clauses->len; i++) {
		cl = g_array_index(cnf_clauses, struct cnf_clause *, i);
		if (cnf_is_tautology(cl)) {
			nr_of_clauses--;
			continue;
		}
		
		struct cnf_literal *lit;
		for (j = 0; j < cl->lits->len; j++) {
			lit = g_array_index(cl->lits, struct cnf_literal *, j);
			picosat_add(pico, lit->val);
			if (j == cl->lits->len - 1)
				picosat_add(pico, 0);
		}
	}
	
	/* need to add these 2 clauses after all others in order to avoid being flagged as tautologies */
	struct gstr tmp1 = str_new();
	str_append(&tmp1, "(#): False constant");
	build_cnf_clause(&tmp1, 1, -const_false->satval);
	picosat_add_arg(pico, -const_false->satval, 0);
	
	struct gstr tmp2 = str_new();
	str_append(&tmp2, "(#): True constant");
	build_cnf_clause(&tmp2, 1, const_true->satval);
	picosat_add_arg(pico, const_true->satval, 0);
	
	start_core = picosat_added_original_clauses(pico);
	
	/* add assumptions */
	struct symbol *sym;
	for_all_symbols(i, sym)
		sym_add_assumption(pico, sym);
	
	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("%d clauses added. (%.6f secs.)\n", nr_of_clauses, time);

	assert(nr_of_clauses == picosat_added_original_clauses(pico));
}

/*
 * start PicoSAT
 */
static void picosat_solve()
{
	printf("Solving SAT-problem...");
	clock_t start, end;
	double time;
	start = clock();
	
	int res = picosat_sat(pico, -1);
	
	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("done. (%.6f secs.)\n\n", time);
	
	if (res == PICOSAT_SATISFIABLE) {
		printf("===> Problem is satisfiable <===\n");
		
		struct symbol *sym;
		unsigned int i;
		bool found = false;
		
		for_all_symbols(i, sym) {
			if (sym->dir_dep.tri < sym->rev_dep.tri) {
				found = true;
				sym_warn_unmet_dep(sym);
				add_select_constraints(sym);
// 				printf("Symbol %s has unmet dependencies.\n", sym->name);
			}
		}
		if (!found) return;
		printf("\n");
		
		/* add assumptions again */
		for_all_symbols(i, sym)
			sym_add_assumption(pico, sym);
		
		int res = picosat_sat(pico, -1);
		
		/* problem should be unsatisfiable now */
		if (res == PICOSAT_UNSATISFIABLE) {
			run_unsat_problem(pico);
		} else {
			printf("SOMETHING IS A MISS. PROBLEM IS NOT UNSATISFIABLE.\n");
		}
		
// 		printf("All CNFs:\n");
// 		print_all_cnf_clauses( cnf_clauses );
		
	} else if (res == PICOSAT_UNSATISFIABLE) {
		printf("===> PROBLEM IS UNSATISFIABLE <===\n");
		printf("\n");
		run_unsat_problem(pico);
	}
	else
		printf("Unknown if satisfiable.\n");
}

static void run_unsat_problem(PicoSAT *pico)
{
	rangefix_init(pico);
}

static void add_select_constraints(struct symbol *sym)
{
	assert(sym->dir_dep.expr);
	
	struct k_expr *ke_dirdep = parse_expr(sym->dir_dep.expr, NULL);
	struct fexpr *dep_both = calculate_fexpr_both(ke_dirdep);
	int tmp_clauses = nr_of_clauses;
	
	if (sym_get_type(sym) == S_TRISTATE) {
		struct fexpr *dep_y = calculate_fexpr_y(ke_dirdep);
		struct fexpr *fe_y = implies(sym->fexpr_y, dep_y);
		convert_fexpr_to_nnf(fe_y);
		convert_fexpr_to_cnf(fe_y);
		unfold_cnf_clause(fe_y);
		
		struct fexpr *fe_both = implies(sym->fexpr_m, dep_both);
		convert_fexpr_to_nnf(fe_both);
		convert_fexpr_to_cnf(fe_both);
		unfold_cnf_clause(fe_both);
	} else if (sym_get_type(sym) == S_BOOLEAN) {
		struct fexpr *fe_both = implies(sym->fexpr_y, dep_both);
		convert_fexpr_to_nnf(fe_both);
		convert_fexpr_to_cnf(fe_both);
		unfold_cnf_clause(fe_both);
	}
	
	/* add CNF-clauses to PicoSAT
	 * ignore tautologies */
	struct cnf_clause *cl;
	unsigned int i, j;
	for (i = tmp_clauses; i < cnf_clauses->len; i++) {
		cl = g_array_index(cnf_clauses, struct cnf_clause *, i);
		if (cnf_is_tautology(cl)) {
			
			nr_of_clauses--;
			continue;
		}
		
		struct cnf_literal *lit;
		for (j = 0; j < cl->lits->len; j++) {
			lit = g_array_index(cl->lits, struct cnf_literal *, j);
			picosat_add(pico, lit->val);
			if (j == cl->lits->len - 1)
				picosat_add(pico, 0);
		}
	}
// 	printf("old %d, new %d\n", tmp_clauses, nr_of_clauses);
}

char * get_test_char()
{
	return "kconfig-sat";
}

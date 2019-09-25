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

#include "../satconfig.h"
#include "picosat.h"
#include "satutils.h"
#include "utils.h"
#include "rangefix.h"
#include "fexpr.h"
#include "print.h"

static void run_unsat_problem(PicoSAT *pico);
static void add_select_constraints(PicoSAT *pico, struct symbol *sym);

/* -------------------------------------- */

/*
 * initialize PicoSAT
 */
PicoSAT * initialize_picosat(void)
{
	// TODO
	printf("\nInitializing PicoSAT...\n");
	PicoSAT *pico = picosat_init();
	picosat_enable_trace_generation(pico);
	
	return pico;
}

/*
 * add clauses to the PicoSAT
 */
void picosat_add_clauses(PicoSAT *pico)
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
void picosat_solve(PicoSAT *pico)
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
				add_select_constraints(pico, sym);
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
	/* get the diagnoses from RangeFix */
	GArray *diagnoses = rangefix_init(pico);
	
	/* ask user for solution to apply */
	GArray *fix = choose_fix(diagnoses);
	
	/* user chose no action, so exit */
	if (fix == NULL) return;
	
	/* apply the fix */ 
	apply_fix(fix);
}

/*
 * add the sharpened dependency constraints in order to make it unsatisfiable 
 */
static void add_select_constraints(PicoSAT *pico, struct symbol *sym)
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
	
	/* add newly created CNF-clauses to PicoSAT, ignore tautologies */
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

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Patrick Franz <patfra71@gmail.com>
 */

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

#include "configfix.h"

static void unfold_cnf_clause(struct pexpr *e);
static void build_cnf_tseytin(struct pexpr *e, struct fexpr *t);
static void build_cnf_tseytin_and(struct pexpr *e, struct fexpr *t);
static void build_cnf_tseytin_or(struct pexpr *e, struct fexpr *t);
static void build_cnf_tseytin_not(struct pexpr *e, struct fexpr *t);

static void run_unsat_problem(PicoSAT *pico);

static PicoSAT *pico;

/* -------------------------------------- */

/*
 * initialize PicoSAT
 */
PicoSAT * initialize_picosat(void)
{
	// TODO
	printf("\nInitializing PicoSAT...");
	PicoSAT *pico = picosat_init();
	picosat_enable_trace_generation(pico);
	printf("done.\n");
	
	return pico;
}

/*
 * construct the CNF-clauses from the constraints
 */
void construct_cnf_clauses(PicoSAT *p)
{
	pico = p;
	unsigned int i;
	struct symbol *sym;
	
	/* adding unit-clauses for constants */
	sat_add_clause(2, pico, -(const_false->satval));
	sat_add_clause(2, pico, const_true->satval);
	
// 	printf("\n");

	for_all_symbols(i, sym) {
		if (sym->type == S_UNKNOWN) continue;
		
// 		print_sym_name(sym);
// 		print_sym_constraint(sym);
		
		struct pexpr_node *node;
		pexpr_list_for_each(node, sym->constraints) {
			if (pexpr_is_cnf(node->elem)) {
// 				pexpr_print("CNF:", e, -1);
// 				getchar();
				unfold_cnf_clause(node->elem);
			} else {
// 				pexpr_print("!Not CNF:", e, -1);
// 				getchar();
				build_cnf_tseytin(node->elem, create_tmpsatvar());
// 				return;
			}
		}
	}
}

/*
 * helper function to add an expression to a CNF-clause
 */
static void unfold_cnf_clause_util(GArray *arr, struct pexpr *e)
{
	switch (e->type) {
	case PE_SYMBOL:
		g_array_add_ints(2, arr, e->left.fexpr->satval);
		break;
	case PE_OR:
		unfold_cnf_clause_util(arr, e->left.pexpr);
		unfold_cnf_clause_util(arr, e->right.pexpr);
		break;
	case PE_NOT:
		g_array_add_ints(2, arr, -(e->left.pexpr->left.fexpr->satval));
		break;
	default:
		perror("Not in CNF, FE_EQUALS.");
	}
}

/*
 * extract the variables from a fexpr in CNF
 */
static void unfold_cnf_clause(struct pexpr *e)
{
	assert(pexpr_is_cnf(e));

	GArray *arr = g_array_new(false, false, sizeof(int *));
	
	unfold_cnf_clause_util(arr, e);

	sat_add_clause_garray(pico, arr);
}

/*
 * build CNF-clauses for a fexpr not in CNF
 */
static void build_cnf_tseytin(struct pexpr *e, struct fexpr *t)
{
// 	pexpr_print("::", e, -1);
// 	assert(!pexpr_is_cnf(e));
	
// 	print_fexpr("e:", e, -1);

	switch (e->type) {
	case PE_AND:
		build_cnf_tseytin_and(e, t);
		break;
	case PE_OR:
		build_cnf_tseytin_or(e, t);
		break;
	case PE_NOT:
		pexpr_print("!Not NNF:",e, -1);
		perror("Should ne in NNF.");
// 		build_cnf_tseytin_not(e);
		break;
	default:
		perror("Expression not a propositional logic formula. root.");
	}
}

/*
 * return the SAT-variable for a pexpr that is a symbol
 */
static int pexpr_satval(struct pexpr *e)
{
	assert(pexpr_is_symbol(e));
	
	switch (e->type) {
	case PE_SYMBOL:
		return e->left.fexpr->satval;
	case PE_NOT:
		return -(e->left.pexpr->left.fexpr->satval);
	default:
		perror("Not a symbol.");
	}
	
	return -1;
}


/*
 * build CNF-clauses for a fexpr of type AND
 */
static void build_cnf_tseytin_and(struct pexpr *e, struct fexpr *t)
{
	struct fexpr *t1, *t2;
	int a, b, c;
	
	/* set left side */
	if (pexpr_is_symbol(e->left.pexpr)) {
		a = pexpr_satval(e->left.pexpr);
	} else {
		t1 = create_tmpsatvar();
		a = t1->satval;
// 		sat_add_clause();
	}
	
	/* set right side */
	if (pexpr_is_symbol(e->right.pexpr)) {
		b = pexpr_satval(e->right.pexpr);
	} else {
		t2 = create_tmpsatvar();
		b = t2->satval;
	}
	
	c = t->satval;
	
	/* A ^ B */
// 	sat_add_clause(2, pico, a);
// 	sat_add_clause(2, pico, b);
	
	/* -A v -B v C */
	sat_add_clause(4, pico, -a, -b, c);
	/* A v -C */
	sat_add_clause(3, pico, a, -c);
	/* B v -C */
	sat_add_clause(3, pico, b, -c);
	
	/* traverse down the tree to build more constraints if needed */
	if (!pexpr_is_symbol(e->left.pexpr)) {
		build_cnf_tseytin(e->left.pexpr, t1);
	}
	if (!pexpr_is_symbol(e->right.pexpr)) {
		build_cnf_tseytin(e->right.pexpr, t2);
	}
}



/*
 * build CNF-clauses for a fexpr of type OR
 */
static void build_cnf_tseytin_or(struct pexpr *e, struct fexpr *t)
{
	struct fexpr *t1, *t2;
	int a, b, c;
	
	/* set left side */
	if (pexpr_is_symbol(e->left.pexpr)) {
		a = pexpr_satval(e->left.pexpr);
	} else {
		t1 = create_tmpsatvar();
		a = t1->satval;
	}
	
	/* set right side */
	if (pexpr_is_symbol(e->right.pexpr)) {
		b = pexpr_satval(e->right.pexpr);
	} else {
		t2 = create_tmpsatvar();
		b = t2->satval;
	}

	c = t->satval;
	
	/* A v B */
// 	sat_add_clause(3, pico, a, b);
	
	/* A v B v -C */
	sat_add_clause(4, pico, a, b, -c);
	/* -A v C */;
	sat_add_clause(3, pico, -a, c);
	/* -B v C */
	sat_add_clause(3, pico, -b, c);
	
	/* traverse down the tree to build more constraints if needed */
	if (!pexpr_is_symbol(e->left.pexpr)) {
		build_cnf_tseytin(e->left.pexpr, t1);
	}
	if (!pexpr_is_symbol(e->right.pexpr)) {
		build_cnf_tseytin(e->right.pexpr, t2);
	}
}

/*
 * build CNF-clauses for a fexpr of type OR
 */
static void build_cnf_tseytin_not(struct pexpr *e, struct fexpr *t)
{
	struct fexpr *t1;
	int a, c;
	
	/* set left side */
	if (pexpr_is_symbol(e->left.pexpr)) {
		a = pexpr_satval(e->left.pexpr);
	} else {
		t1 = create_tmpsatvar();
		a = t1->satval;
	}
	
	c = t->satval;
	
	/* -A v -C */
	sat_add_clause(3, pico, -a, -c);
	/* A v C */
	sat_add_clause(3, pico, a, c);
	
	printf("\nWARNING!!\nCNF-CLAUSE JUST NOT!!\n");
}

/*
 * add a clause to PicoSAT
 * First argument is the SAT solver
 */
void sat_add_clause(int num, ...)
{
	if (num <= 1) return;
	
	va_list valist;
	int i, *lit;
	PicoSAT *pico;
	GArray *arr = g_array_new(false, false, sizeof(int *));
	
	/* initialize valist for num number of arguments */
	va_start(valist, num);
	
	pico = va_arg(valist, PicoSAT *);
	
	/* access all the arguments assigned to valist */
	for (i = 1; i < num; i++) {
		lit = malloc(sizeof(int));
		*lit = va_arg(valist, int);
		picosat_add(pico, *lit);
		g_array_append_val(arr, lit);
	}
	picosat_add(pico, 0);
// 	id = malloc(sizeof(int));
// 	*id = picosat_add(pico, 0);
// 	*id = g_hash_table_size(cnf_clauses);
	
	/* add clause to hashmap */
// 	g_hash_table_insert(cnf_clauses_map, id, arr);
	
// 	printf("Clause added, id %d\n", id);
	
	/* clean memory reserved for valist */
	va_end(valist);
}

/* 
 * add a clause from a GArray to PicoSAT 
 */
void sat_add_clause_garray(PicoSAT *pico, GArray *arr)
{
	int i, *lit;

	for (i = 0; i < arr->len; i++) {
		lit = g_array_index(arr, int *, i);
		picosat_add(pico, *lit);
	}
	picosat_add(pico, 0);
// 	id = malloc(sizeof(int));
// 	*id = picosat_add(pico, 0);
	
	/* add clause to hashmap if not done yet */
// 	if (!g_hash_table_contains(cnf_clauses_map, id))
// 		g_hash_table_insert(cnf_clauses_map, id, arr);
}

/*
 * add clauses to the PicoSAT
 */
// void picosat_add_clauses(PicoSAT *pico)
// {
// 	printf("Adding clauses...");
// 	
// 	int tmp_clauses = nr_of_clauses;
// 	
// 	clock_t start, end;
// 	double time;
// 	start = clock();
// 
// 	/* add CNF-clauses to PicoSAT
// 	 * ignore tautologies */
// 	struct cnf_clause *cl;
// 	unsigned int i, j;
// 	for (i = 0; i < cnf_clauses->len; i++) {
// 		cl = g_array_index(cnf_clauses, struct cnf_clause *, i);
// 		if (cnf_is_tautology(cl)) {
// 			tmp_clauses--;
// 			continue;
// 		}
// 		
// 		struct cnf_literal *lit;
// 		for (j = 0; j < cl->lits->len; j++) {
// 			lit = g_array_index(cl->lits, struct cnf_literal *, j);
// 			picosat_add(pico, lit->val);
// 			if (j == cl->lits->len - 1)
// 				picosat_add(pico, 0);
// 		}
// 	}
// 	
// 	end = clock();
// 	time = ((double) (end - start)) / CLOCKS_PER_SEC;
// 	printf("%d clauses added. (%.6f secs.)\n", tmp_clauses, time);
// 
// 	assert(tmp_clauses == picosat_added_original_clauses(pico));
// }

/*
 * start PicoSAT
 */
void picosat_solve(PicoSAT *pico)
{
	printf("Solving SAT-problem...");
	
	/* add assumptions */
	struct symbol *sym;
	unsigned int i;
	for_all_symbols(i, sym) {
		if (sym->type == S_UNKNOWN) continue;
		
		sym_add_assumption(pico, sym);
	}
	
	clock_t start, end;
	double time;
	start = clock();
	
	int res = picosat_sat(pico, -1);
	
	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("done. (%.6f secs.)\n\n", time);
	
	if (res == PICOSAT_SATISFIABLE) {
		printf("===> PROBLEM IS SATISFIABLE <===\n");
		
// 		struct symbol *sym;
// 		bool found = false;
// 		
// 		/* check, if a symbol has been selected, but has unmet dependencies */
// 		for_all_symbols(i, sym) {
// 			if (sym->dir_dep.tri < sym->rev_dep.tri) {
// 				found = true;
// 				sym_warn_unmet_dep(sym);
// 				add_select_constraints(pico, sym);
// 				printf("Symbol %s has unmet dependencies.\n", sym->name);
// 			}
// 		}
// 		if (!found) return;
// 		printf("\n");
// 		
// 		/* add assumptions again */
// 		for_all_symbols(i, sym)
// 			sym_add_assumption(pico, sym);
		
// 		int res = picosat_sat(pico, -1);
// 		
// 		/* problem should be unsatisfiable now */
// 		if (res == PICOSAT_UNSATISFIABLE) {
// 			run_unsat_problem(pico);
// 		} else {
// 			printf("SOMETHING IS A MISS. PROBLEM IS NOT UNSATISFIABLE.\n");
// 		}
		
// 		printf("All CNFs:\n");
// 		print_all_cnf_clauses( cnf_clauses );
		
	} else if (res == PICOSAT_UNSATISFIABLE) {
		printf("===> PROBLEM IS UNSATISFIABLE <===\n");
		
		/* print unsat core */
		printf("\nPrinting unsatisfiable core:\n");
		struct fexpr *e;
		for (i = 1; i < sat_variable_nr; i++) {
			if (picosat_failed_assumption(pico, i)) {
				int *index = malloc(sizeof(*index));
				*index = i;
				e = g_hash_table_lookup(satmap, index);
				printf("%s <%d>\n", str_get(&e->name), e->assumption);
			}
			
		}
// 		printf("\n");
		
		return;
		printf("\n");
		run_unsat_problem(pico);
	}
	else
		printf("Unknown if satisfiable.\n");
}

static void run_unsat_problem(PicoSAT *pico)
{
	/* get the diagnoses from RangeFix */
	struct sfl_list *diagnoses = rangefix_run(pico);
	
	/* ask user for solution to apply */
	struct sfix_list *fix = choose_fix(diagnoses);
	
	/* user chose no action, so exit */
	if (fix == NULL) return;
	
	/* apply the fix */ 
	apply_fix(fix);
}

/*
 * add assumption for a symbol to the SAT-solver
 */
void sym_add_assumption(PicoSAT *pico, struct symbol *sym)
{
	/*
	* TODO
	* Decide if we want the value from .config or the actual value,
	* which might differ because of prompt conditions.
	*/

	if (sym_is_boolean(sym)) {
// 		int tri_val = sym->def[S_DEF_USER].tri;
		int tri_val = sym_get_tristate_value(sym);
		
		sym_add_assumption_tri(pico, sym, tri_val);
		
		return;
	}
	
	if (sym_is_nonboolean(sym)) {
		struct fexpr *e = sym->nb_vals->head->elem;

		struct fexpr_node *node;
		
		/* symbol does not have a value */
		if (!sym_has_value(sym)) {
			/* set value for sym=n */
			picosat_assume(pico, e->satval);
			e->assumption = true;
			
			struct fexpr_node *node;
			fexpr_list_for_each(node, sym->nb_vals) {
				if (node->prev == NULL) continue;
				
				picosat_assume(pico, -(node->elem->satval));
				node->elem->assumption = false;
			}
			
			return;
		}
		
		/* symbol does have a value set */
		
		/* set value for sym=n */
		picosat_assume(pico, -(e->satval));
		e->assumption = false;
		
		const char *string_val = sym_get_string_value(sym);
		
		/* set value for all other fexpr */
		fexpr_list_for_each(node, sym->nb_vals) {
			if (node->prev == NULL) continue;
			
			if (strcmp(str_get(&node->elem->nb_val), string_val) == 0) {
				picosat_assume(pico, node->elem->satval);
				node->elem->assumption = true;
			} else {
				picosat_assume(pico, -(node->elem->satval));
				node->elem->assumption = false;
			}
		}
	}
}

/*
 * add assumption for a boolean symbol to the SAT-solver
 */
void sym_add_assumption_tri(PicoSAT *pico, struct symbol *sym, tristate tri_val)
{
	if (sym->type == S_BOOLEAN) {
		int a = sym->fexpr_y->satval;
		switch (tri_val) {
		case no:
			picosat_assume(pico, -a);
			sym->fexpr_y->assumption = false;
			break;
		case mod:
			perror("Should not happen. Boolean symbol is set to mod.\n");
			break;
		case yes:
			picosat_assume(pico, a);
			sym->fexpr_y->assumption = true;
			break;
		}
	}
	if (sym->type == S_TRISTATE) {
		int a = sym->fexpr_y->satval;
		int a_m = sym->fexpr_m->satval;
		switch (tri_val) {
		case no:
			picosat_assume(pico, -a);
			picosat_assume(pico, -a_m);
			sym->fexpr_y->assumption = false;
			sym->fexpr_m->assumption = false;
			break;
		case mod:
			picosat_assume(pico, -a);
			picosat_assume(pico, a_m);
			sym->fexpr_y->assumption = false;
			sym->fexpr_m->assumption = true;
			break;
		case yes:
			picosat_assume(pico, a);
			picosat_assume(pico, -a_m);
			sym->fexpr_y->assumption = true;
			sym->fexpr_m->assumption = false;
			break;
		}
	}
}

/* 
 * add assumptions for the symbols to be changed to the SAT solver
 */
void sym_add_assumption_sdv(PicoSAT *pico, GArray *arr)
{
	unsigned int i;
	struct symbol_dvalue *sdv;
	for (i = 0; i < arr->len; i++) {
		sdv = g_array_index(arr, struct symbol_dvalue *, i);
		
		int lit_y = sdv->sym->fexpr_y->satval;
		
		if (sdv->sym->type == S_BOOLEAN) {
			switch (sdv->tri) {
			case yes:
				picosat_assume(pico, lit_y);
				break;
			case no:
				picosat_assume(pico, -lit_y);
				break;
			case mod:
				perror("Should not happen.\n");
			}
		} else if (sdv->sym->type == S_TRISTATE) {
			int lit_m = sdv->sym->fexpr_m->satval;
			switch (sdv->tri) {
			case yes:
				picosat_assume(pico, lit_y);
				picosat_assume(pico, -lit_m);
				break;
			case mod:
				picosat_assume(pico, -lit_y);
				picosat_assume(pico, lit_m);
				break;
			case no:
				picosat_assume(pico, -lit_y);
				picosat_assume(pico, -lit_m);
			}
		}
	}
}

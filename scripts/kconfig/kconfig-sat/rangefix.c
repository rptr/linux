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
#include "rangefix.h"
#include "utils.h"

static GArray *diagnoses, *diagnoses_symbol;

static GArray * generate_diagnoses(PicoSAT *pico);

static void add_fexpr_to_constraint_set(gpointer key, gpointer value, gpointer userData);
static void set_assumptions(PicoSAT *pico, GArray *c);
static void fexpr_add_assumption(PicoSAT *pico, struct fexpr *e);
static GArray * get_unsat_core(PicoSAT *pico, GArray *c);


static GArray * get_difference(GArray *C, GArray *E0);
static bool has_intersection(GArray *e, GArray *X);
static GArray * get_union(GArray *A, GArray *B);
static GArray * clone_array(GArray *arr);
static bool is_subset_of(GArray *A, GArray *B);
static void print_array(char *title, GArray *arr);
static void print_array_symbol(char *title, GArray *arr);
static bool diagnosis_contains_fexpr(GArray *diagnosis, struct fexpr *e);
static bool diagnosis_contains_symbol(GArray *diagnosis, struct symbol *sym);

static void print_diagnoses(GArray *diag);
static void print_diagnoses_symbol(GArray *diag_sym);

static GArray * convert_diagnoses(GArray *diagnoses);
static struct symbol_fix * symbol_fix_create(struct fexpr *e, enum symbolfix_type type, GArray *diagnosis);

static tristate calculate_new_tri_val(struct fexpr *e, GArray *diagnosis);
static const char * calculate_new_string_value(struct fexpr *e, GArray *diagnosis);

/* -------------------------------------- */

GArray * rangefix_init(PicoSAT *pico)
{
	printf("Starting RangeFix...\n");
	printf("Generating diagnoses...");
	clock_t start, end;
	double time;
	start = clock();
	
	/* generate the diagnoses */
	diagnoses = generate_diagnoses(pico);

	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("done. (%.6f secs.)\n", time);
	
	printf("\n");
	
	/* convert diagnoses of fexpr to diagnoses of symbols */
	diagnoses_symbol = convert_diagnoses(diagnoses);
	
	return diagnoses_symbol;
}

/*
 * generate the diagnoses
 */
static GArray * generate_diagnoses(PicoSAT *pico)
{
	GArray *C = g_array_new(false, false, sizeof(struct fexpr *));
	GArray *E = g_array_new(false, false, sizeof(GArray *));
	GArray *R = g_array_new(false, false, sizeof(GArray *));
	GArray *X, *e, *x_set, *E1, *E_R_Union, *E2;
	struct fexpr *x;
	unsigned int i, j, k, diagnosis_index;
	
// 	unsigned int t;
// 	GArray *tmp;
	
	/* create constraint set C */
	g_hash_table_foreach(satmap, add_fexpr_to_constraint_set, C);
	
	/* init E with an empty diagnosis */
	GArray *empty_diagnosis = g_array_new(false, false, sizeof(struct fexpr *));
	g_array_append_val(E, empty_diagnosis);
	
	//num = (rand() % (upper – lower + 1)) + lower
	while (E->len > 0) {
// 		printf("start while, E.length: %d\n", E->len);
		/* get random diagnosis */
		diagnosis_index = rand() % (E->len);
		GArray *E0 = g_array_index(E, GArray *, diagnosis_index);
// 		print_array("Select partial diagnosis", E0);
		
		GArray *c = get_difference(C, E0);
// 		print_array("Soft constraints", c);
		
		
		set_assumptions(pico, c);
		
		int res = picosat_sat(pico, -1);
		if (res == PICOSAT_SATISFIABLE) {
// 			printf("SATISFIABLE\n");
// 			print_array("Found diagnosis", E0);
// 			printf("size %d\n", E0->len);
			E = g_array_remove_index(E, diagnosis_index);
			R = g_array_append_val(R, E0);
			
			continue;
			
		} else if (res == PICOSAT_UNSATISFIABLE) {
// 			printf("UNSATISFIABLE\n");
		} else if (res == PICOSAT_UNKNOWN) {
			printf("UNKNOWN\n");
		} else {
			perror("Doh brother.");
		}
		
		
		
		X = get_unsat_core(pico, c);
// 		print_array("Unsat core", X);
		// TODO: possibly minimise the unsat core here, but not necessary
		
		for (i = 0; i < E->len; i++) {
// 		i = 0;
// 		while (i < E->len) {
// 			printf("size of E: %d, i = %d\n", E->len, i);
// 			getchar();
			/* get partial diagnosis */
			e = g_array_index(E, GArray *, i);
// 			print_array("Look at partial diagnosis", e);
			
			/* check, if there is an intersection between e and X
			 * if there is, go to the next partial diagnosis */
			if (has_intersection(e, X)) {
// 				printf("Intersection with core.\n");
// 				i++;
				continue;
			}
			
			/* for each fexpr in the core */
			for (j = 0; j < X->len; j++) {
				x = g_array_index(X, struct fexpr *, j);
// 				printf("x = %s\n", str_get(&x->name));
				
				
				/* create {x} */
				x_set = g_array_new(false, false, sizeof(struct fexpr *));
				g_array_append_val(x_set, x);
// 				print_array("{x}", x_set);
				
				
				
				/* create E' = e ∪ {x} */
				E1 = get_union(e, x_set);
// 				print_array("E1", E1);
				
				

				
				/* create (E\e) ∪ R */
				E_R_Union = clone_array(E);
				g_array_remove_index(E_R_Union, i);
				E_R_Union = get_union(E_R_Union, R);
// 				for (t = 0; t < E_R_Union->len; t++) {
// 					tmp = g_array_index(E_R_Union, GArray *, t);
// 					print_array("E_R_Union", tmp);
// 				}
// 				
// 				getchar();
				
				
				bool E2_subset_of_E1 = false;
				
				/* E" ∈ (E\e) ∪ R */
				for (k = 0; k < E_R_Union->len; k++) {
					E2 = g_array_index(E_R_Union, GArray *, k);
// 					print_array("E2", E2);
// 					getchar();
					
					/* E" ⊆ E' ? */
					if (is_subset_of(E2, E1)) {
						E2_subset_of_E1 = true;
// 						print_array("E\"", E2);
// 						print_array("E'", E1);
						break;
					}
				}
				
				/* ∄ E" ⊆ E' */
				if (!E2_subset_of_E1) {
					E = g_array_append_val(E, E1);
// 					print_array("Add partial diagnosis", E1);
				}
				
// 				for (t = 0; t < E->len; t++) {
// 					tmp = g_array_index(E, GArray *, t);
// 					print_array("E", tmp);
// 				}
				
				
// 				getchar();
				
			}
			
// 			print_array("Remove partial diagnosis", e);
			g_array_remove_index(E, i);
// 			for (t = 0; t < E->len; t++) {
// 				tmp = g_array_index(E, GArray *, t);
// 				print_array("E", tmp);
// 			}
// 			printf("E.length: %d\n", E->len);
// 			
// 			getchar();
			
			i--;
			
		}
		
// 		printf("E.length: %d, i = %d\n", E->len, i);
		
	}
	
// 	for (t = 0; t < R->len; t++) {
// 		tmp = g_array_index(R, GArray *, t);
// 		print_array("R", tmp);
// 	}
	
	return R;
}

/*
 * add a fexpr to our constraint set C 
 */
static void add_fexpr_to_constraint_set(gpointer key, gpointer value, gpointer userData)
{
	int realKey = *( (int*)key );
	struct fexpr *e = (struct fexpr *) value;
	assert(realKey == e->satval);
	GArray *C = (GArray *) userData;
	
	if (e->type != FE_SYMBOL && e->type != FE_NONBOOL) return;
	
	g_array_append_val(C, e);
}

/*
 * set the assumptions for the next run of Picosat
 */
static void set_assumptions(PicoSAT *pico, GArray *c)
{
	struct fexpr *e;
	unsigned int i;
	for (i = 0; i < c->len; i++) {
		e = g_array_index(c, struct fexpr *, i);
		fexpr_add_assumption(pico, e);
	}
}

static void fexpr_add_assumption(PicoSAT *pico, struct fexpr *e)
{
	struct symbol *sym = e->sym;
	
	if (sym_get_type(sym) == S_BOOLEAN) {
		int tri_val = sym->def[S_DEF_USER].tri;
		tri_val = sym_get_tristate_value(sym);
	
		if (tri_val == yes) {
			picosat_assume(pico, e->satval);
			e->assumption = true;
		} else {
			picosat_assume(pico, -(e->satval));
			e->assumption = false;
		}
	}
	
	if (sym_get_type(sym) == S_TRISTATE) {
		int tri_val = sym->def[S_DEF_USER].tri;
		tri_val = sym_get_tristate_value(sym);
		
		if (e->tristate == yes) {
			if (tri_val == yes) {
				picosat_assume(pico, e->satval);
				e->assumption = true;
			} else {
				picosat_assume(pico, -(e->satval));
				e->assumption = false;
			}
		} else if (e->tristate == mod) {
			if (tri_val == mod) {
				picosat_assume(pico, e->satval);
				e->assumption = true;
			} else {
				picosat_assume(pico, -(e->satval));
				e->assumption = false;
			}
		}
	}
	
	if (sym_get_type(sym) == S_INT) {
		const char *string_val = sym_get_string_value(sym);
	
		if (strcmp(str_get(&e->nb_val), string_val) == 0) {
			picosat_assume(pico, e->satval);
			e->assumption = true;
		} else {
			picosat_assume(pico, -(e->satval));
			e->assumption = false;
		}
	}
}

/*
 * get the unsat core from the last run of Picosat
 */
static GArray * get_unsat_core(PicoSAT *pico, GArray *c)
{
	GArray *ret = g_array_new(false, false, sizeof(struct fexpr *));
	struct fexpr *e;
	unsigned int i;
	
	for (i = 0; i < c->len; i++) {
		e = g_array_index(c, struct fexpr *, i);
		if (picosat_failed_assumption(pico, e->satval))
			g_array_append_val(ret, e);
	}
	
	return ret;
}

/*
 * Calculate C\E0
 */
static GArray * get_difference(GArray *C, GArray *E0)
{
	GArray *ret = g_array_new(false, false, sizeof(struct fexpr *));
	struct fexpr *e1, *e2;
	unsigned int i, j;
	bool found;
	
	for (i = 0; i < C->len; i++) {
		e1 = g_array_index(C, struct fexpr *, i);
		found = false;
		for (j = 0; j < E0->len; j++) {
			e2 = g_array_index(E0, struct fexpr *, j);
			if (e1 == e2) {
				found = true;
				break;
			}
		}
		if (!found)
			g_array_append_val(ret, e1);
	}
	
	return ret;
}

/*
 * check, if there is an intersection between e and X
 */
static bool has_intersection(GArray *e, GArray *X)
{
	struct fexpr *e1, *e2;
	unsigned int i, j;
	
	for (i = 0; i < e->len; i++) {
		e1 = g_array_index(e, struct fexpr *, i);
		for (j = 0; j < X->len; j++) {
			e2 = g_array_index(X, struct fexpr *, j);
			if (e1 == e2)
				return true;
		}
	}
	return false;
}

/*
 * get the union of 2 GArrays with fexpr
 */
static GArray * get_union(GArray *A, GArray *B)
{
	GArray *ret = g_array_new(false, false, sizeof(void *));
	struct fexpr *e1, *e2;
	unsigned int i, j;
	bool found = false;
	
	for (i = 0; i < A->len; i++) {
		e1 = g_array_index(A, void *, i);
		g_array_append_val(ret, e1);
	}
	
	for (i = 0; i < B->len; i++) {
		e1 = g_array_index(B, void *, i);
		for (j = 0; j < A->len; j++) {
			e2 = g_array_index(A, void *, j);
			if (e1 == e2) {
				found = true;
				break;
			}
		}
		if (!found)
			g_array_append_val(ret, e1);
	}
	
	return ret;
}

/*
 * clone a single GArray
 */
static GArray * clone_array(GArray *arr)
{
	GArray *ret = g_array_new(false, false, sizeof(void *));
	struct fexpr *e;
	unsigned int i;

	for (i = 0; i < arr->len; i++) {
		e = g_array_index(arr, void *, i);
		g_array_append_val(ret, e);
	}
	
	return ret;
}

/*
 * check, if A is a subset of B
 */
static bool is_subset_of(GArray *A, GArray *B)
{
	struct fexpr *e1, *e2;
	unsigned int i, j;
	bool found;
	
	for (i = 0; i < A->len; i++) {
		e1 = g_array_index(A, struct fexpr *, i);
		found = false;
		for (j = 0; j < B->len; j++) {
			e2 = g_array_index(B, struct fexpr *, j);
			if (e1 == e2) {
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}
	
	return true;
}

/*
 * print a GArray with fexpr
 */
static void print_array(char *title, GArray *arr)
{
	struct fexpr *e;
	unsigned int i;
	printf("%s: [", title);
	
	for (i = 0; i < arr->len; i++) {
		e = g_array_index(arr, struct fexpr *, i);
		printf("%s", str_get(&e->name));
		if (i != arr->len - 1)
			printf(", ");
	}
	
	printf("]\n");
}

/*
 * print a GArray with symbol_fix
 */
static void print_array_symbol(char *title, GArray *arr)
{
	struct symbol_fix *fix;
	unsigned int i;
	printf("%s: [", title);
	
	for (i = 0; i < arr->len; i++) {
		fix = g_array_index(arr, struct symbol_fix *, i);
		printf("%s", fix->sym->name);
		if (i != arr->len - 1)
			printf(", ");
	}
	
	printf("]\n");
}

/*
 * check if a diagnosis contains a fexpr
 */
static bool diagnosis_contains_fexpr(GArray *diagnosis, struct fexpr *e)
{
	struct fexpr *e2;
	unsigned int i;
	for (i = 0; i < diagnosis->len; i++) {
		e2 = g_array_index(diagnosis, struct fexpr *, i);
		if (e == e2)
			return true;
	}
	
	return false;
}

/*
 * check if a diagnosis contains a symbol
 */
static bool diagnosis_contains_symbol(GArray *diagnosis, struct symbol *sym)
{
	struct symbol_fix *sym_fix;
	unsigned int i;
	
	for (i = 0; i < diagnosis->len; i++) {
		sym_fix = g_array_index(diagnosis, struct symbol_fix *, i);
		if (sym == sym_fix->sym)
			return true;
	}
	
	return false;
}

/*
 * print the diagnoses of type fexpr
 */
static void print_diagnoses(GArray *diag)
{
	GArray *arr;
	struct fexpr *e;
	unsigned int i, j;

	for (i = 0; i < diag->len; i++) {
		arr = g_array_index(diag, GArray *, i);
		printf("%d: [", i+1);
		for (j = 0; j < arr->len; j++) {
			e = g_array_index(arr, struct fexpr *, j);
			char *new_val = e->assumption ? "false" : "true";
			printf("%s => %s", str_get(&e->name), new_val);
			if (j != arr->len - 1)
				printf(", ");
		}
		printf("]\n");
	}
}

/*
 * print the diagnoses of type symbol_fix
 */
static void print_diagnoses_symbol(GArray *diag_sym)
{
	GArray *arr;
	struct symbol_fix *fix;
	unsigned int i, j;

	for (i = 0; i < diag_sym->len; i++) {
		arr = g_array_index(diag_sym, GArray *, i);
		printf("%d: [", i+1);
		for (j = 0; j < arr->len; j++) {
			fix = g_array_index(arr, struct symbol_fix *, j);
			
			if (fix->type == SF_BOOLEAN)
				printf("%s => %s", fix->sym->name, tristate_get_char(fix->tri));
			else if (fix->type == SF_NONBOOLEAN)
				printf("%s => %s", fix->sym->name, str_get(&fix->nb_val));
			else
				perror("NB not yet implemented.");
			
			if (j != arr->len - 1)
				printf(", ");
		}
		printf("]\n");
	}
}


/*
 * convert the diagnoses of fexpr into diagnoses of symbols
 * it is easier to handle symbols when applying fixes
 */
static GArray * convert_diagnoses(GArray *diag_arr)
{
	diagnoses_symbol = g_array_new(false, false, sizeof(GArray *));
	
	GArray *diagnosis, *diagnosis_symbol;
	struct fexpr *e;
	struct symbol_fix *fix;
	unsigned i, j;
	
	for (i = 0; i < diag_arr->len; i++) {
		diagnosis = g_array_index(diag_arr, GArray *, i);
		diagnosis_symbol = g_array_new(false, false, sizeof(struct symbol_fix *));
		for (j = 0; j < diagnosis->len; j++) {
			e = g_array_index(diagnosis, struct fexpr *, j);
			
			/* diagnosis contains symbol, so continue */
			if (diagnosis_contains_symbol(diagnosis_symbol, e->sym)) continue;
			
			// TODO for disallowed
			enum symbolfix_type type;
			if (sym_is_boolean(e->sym))
				type = SF_BOOLEAN;
			else if (sym_is_nonboolean(e->sym))
				type = SF_NONBOOLEAN;
			else
				type = SF_DISALLOWED;
			fix = symbol_fix_create(e, type, diagnosis);
			
			g_array_append_val(diagnosis_symbol, fix);
		}
		g_array_append_val(diagnoses_symbol, diagnosis_symbol);
	}
	
	return diagnoses_symbol;
}

/* 
 * create a symbol_fix given a fexpr
 */
static struct symbol_fix * symbol_fix_create(struct fexpr *e, enum symbolfix_type type, GArray *diagnosis)
{
	struct symbol_fix *fix = malloc(sizeof(struct symbol_fix));
	fix->sym = e->sym;
	fix->type = type;
	
	switch(type) {
	case SF_BOOLEAN:
		fix->tri = calculate_new_tri_val(e, diagnosis);
		break;
	case SF_NONBOOLEAN:
		fix->nb_val = str_new();
		str_append(&fix->nb_val, calculate_new_string_value(e, diagnosis));
		break;
	default:
		perror("Illegal symbolfix_type.\n");
	}

	return fix;
}

/*
 * list the diagnoses and let user choose a diagnosis to be applied
 */
GArray * choose_fix(GArray *diag)
{
	printf("=== GENERATED DIAGNOSES ===\n");
	printf("0: No changes wanted\n");
	print_diagnoses_symbol(diag);
	
	int choice;
	printf("\n> Choose option: ");
	scanf("%d", &choice);
	
	/* no changes wanted */
	if (choice == 0) return NULL;
	
	return g_array_index(diag, GArray *, choice - 1);
}

/*
 * apply the fixes from a diagnosis
 */
void apply_fix(GArray *diag)
{
	struct symbol_fix *fix;
	unsigned int i;

	// TODO
	// order is important
	// either sort it or forcewrite it into config
	// or do with a fixpoint, i.e. repeat until everything is set
	for (i = 0; i < diag->len; i++) {
		fix = g_array_index(diag, struct symbol_fix *, i);
		if (fix->type == SF_BOOLEAN)
			sym_set_tristate_value(fix->sym, fix->tri);
		else if (fix->type == SF_NONBOOLEAN)
			sym_set_string_value(fix->sym, str_get(&fix->nb_val));
		
		conf_write(NULL);
	}
// 	conf_write(NULL);
}

/*
 * calculate the new value for a boolean symbol given a diagnosis and an fexpr
 */
static tristate calculate_new_tri_val(struct fexpr *e, GArray *diagnosis)
{
	assert(sym_is_boolean(e->sym));
	
	/* return the opposite of the last assumption for booleans */
	if (sym_get_type(e->sym) == S_BOOLEAN)
		return e->assumption ? no : yes;
	
	/* new values for tristate must be deduced from the diagnosis */
	if (sym_get_type(e->sym) == S_TRISTATE) {
		/* fexpr_y */
		if (e->tristate == yes) {
			if (e->assumption == true)
				/* 
				 * if diagnosis contains fexpr_m, fexpr_m was false
				 * => new value is mod
				 */
				return diagnosis_contains_fexpr(diagnosis, e->sym->fexpr_m) ? mod : no;
			else if (e->assumption == false)
				/*
				 * if fexpr_y is set to true, the new value must be yes
				 */
				return yes;
		}
		/* fexpr_m */
		if (e->tristate == mod) {
			if (e->assumption == true)
				/*
				 * if diagnosis contains fexpr_y, fexpr_y was false
				 * => new value is yes
				 */
				return diagnosis_contains_fexpr(diagnosis, e->sym->fexpr_m) ? yes : no;
			else if (e->assumption == false)
				/*
				 * if diagnosis contains fexpr_m, the new value must be mod
				 */
				return mod;
		}
		perror("Should not get here.\n");
	}
	
	perror("Error calculating new tristate value.\n");
	return no;
}

/*
 * calculate the new value for a non-boolean symbol given a diagnosis and an fexpr
 */
static const char * calculate_new_string_value(struct fexpr *e, GArray *diagnosis)
{
	assert(sym_is_nonboolean(e->sym));
	
	if (sym_get_type(e->sym) == S_INT) {
		/* if assumption was false before, this is the new value because only 1 variable can be true */
		if (e->assumption == false)
			return str_get(&e->nb_val);
		
		struct fexpr *e2;
		unsigned int i;
		
		/* a diagnosis always contains 2 variable for the same symbol
		 * one is set to true, the other to false
		 * otherwise you'd set 2 variables to true, which is not allowed */
		for (i = 0; i < diagnosis->len; i++) {
			e2 = g_array_index(diagnosis, struct fexpr *, i);
			
			/* not interested in other symbols or the same fexpr */
			if (e->sym != e2->sym || e == e2) continue;
			
			return str_get(&e2->nb_val);
		}
		
	}
	
	perror("Error calculating new string value.\n");
	return "";
}

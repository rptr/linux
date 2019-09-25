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
#include "utils.h"
#include "print.h"
#include "fexpr.h"


/*
 * initialize symbols
 */
void init_data(void)
{
	/* initialize array with all CNF clauses */
	cnf_clauses = g_array_new(false, false, sizeof(struct cnf_clause *));
	
	/* create hashtable with all fexpr */
	satmap = g_hash_table_new_full(
		g_int_hash, g_int_equal, //< This is an integer hash.
		NULL, //< Call "free" on the key (made with "malloc").
		free //< Call "free" on the value (made with "strdup").
	);
	
	printf("done.\n");
}

/*
 * bool-symbols have 1 variable (X), tristate-symbols have 2 variables (X, X_m)
 */
static void create_sat_variables(struct symbol *sym)
{
	sym->constraints = malloc(sizeof(struct garray_wrapper));
	sym->constraints->arr = g_array_new(false, false, sizeof(struct fexpr *));
	sym_create_fexpr(sym);
}

/*
 * assign SAT-variables to all fexpr and create the sat_map
 */
void assign_sat_variables(void)
{
	unsigned int i;
	struct symbol *sym;
	
	printf("Creating SAT-variables...");
	
	for_all_symbols(i, sym) {
		if (sym_get_type(sym) != S_UNKNOWN)
			create_sat_variables(sym);
	}
	
	printf("done.\n");
}

/*
 * create some constants
 */
void create_constants(void)
{
	printf("Creating constants...");
	/* create TRUE and FALSE constants */
	const_false = malloc(sizeof(struct fexpr));
	const_false->name = str_new();
	str_append(&const_false->name, "0");
	const_false->type = FE_FALSE;
	const_false->satval = sat_variable_nr++;
	
	const_true = malloc(sizeof(struct fexpr));
	const_true->name = str_new();
	str_append(&const_true->name, "1");
	const_true->type = FE_TRUE;
	const_true->satval = sat_variable_nr++;
	
	/* add to satmap */
	g_hash_table_insert(satmap, &const_false->satval, const_false);
	g_hash_table_insert(satmap, &const_true->satval, const_true);
	
	/* add fexpr of constants to tristate constants */
	symbol_yes.fexpr_y = const_true;
	symbol_yes.fexpr_m = const_false;
	
	symbol_mod.fexpr_y = const_false;
	symbol_mod.fexpr_m = const_true;
	
	symbol_no.fexpr_y = const_false;
	symbol_no.fexpr_m = const_false;
	
	printf("done.\n");
}

/*
 * create a fexpr
 */
struct fexpr * create_fexpr(int satval, enum fexpr_type type, char* name)
{
	struct fexpr *e = malloc(sizeof(struct fexpr));
	e->satval = satval;
	e->type = type;
	e->name = str_new();
	str_append(&e->name, name);
	
	return e;
}

/*
 * build a CNF clause with the SAT-variables given
 */
struct cnf_clause * build_cnf_clause(struct gstr *reason, int num, ...)
{
	va_list valist;
	va_start(valist, num);
	int i;
	
	struct cnf_clause *cl = create_cnf_clause_struct();
	
	for (i = 0; i < num; i++) {
		int val = va_arg(valist, int);
		add_literal_to_clause(cl, val);
	}
	cl->reason = str_new();
	str_append(&cl->reason, str_get(reason));
	
	g_array_append_val(cnf_clauses, cl);

	nr_of_clauses++;

	va_end(valist);
	return cl;
}


/*
 * add a literal to a CNF-clause
 */
void add_literal_to_clause(struct cnf_clause *cl, int val)
{
	struct cnf_literal *lit = malloc(sizeof(struct cnf_literal));

	/* get the fexpr */
	struct fexpr *e = get_fexpr_from_satmap(val);
	
	lit->val = val;
	lit->name = str_new();
	if (val <= 0)
		str_append(&lit->name, "-");
	str_append(&lit->name, str_get(&e->name));
	
	g_array_append_val(cl->lits, lit);
}

/*
 * create a struct for a CNF clause
 */
struct cnf_clause * create_cnf_clause_struct(void)
{
	struct cnf_clause *cl = malloc(sizeof(struct cnf_clause));
	cl->lits = g_array_new(false, false, sizeof(struct cnf_literal *));
	return cl;
}

/*
 * return a temporary SAT variable as string
 */
char * get_tmp_var_as_char(int i)
{
	char *tv_sval = malloc(sizeof(char) * 17);
	snprintf(tv_sval, 17,"T%d", i);
	return tv_sval;
}

/*
 * return a tristate value as a char *
 */
char * tristate_get_char(tristate val)
{
	switch (val) {
	case yes:
		return "yes";
	case mod:
		return "mod";
	case no:
		return "no";
	default:
		return "";
	}
}

/*
 * return if a k_expr can evaluate to mod
 */
bool can_evaluate_to_mod(struct k_expr *e)
{
	if (!e)
		return false;
	
	switch (e->type) {
	case KE_SYMBOL:
		return sym_get_type(e->sym) == S_TRISTATE ? true : false;
	case KE_AND:
	case KE_OR:
		return can_evaluate_to_mod(e->left) || can_evaluate_to_mod(e->right);
	case KE_NOT:
		return can_evaluate_to_mod(e->left);
	case KE_EQUAL:
	case KE_UNEQUAL:
	case KE_CONST_FALSE:
	case KE_CONST_TRUE:
		return false;
	}
	
	return false;
}

/*
 * return the constant FALSE as a k_expr
 */
struct k_expr * get_const_false_as_kexpr()
{
	struct k_expr *ke = malloc(sizeof(struct k_expr));
	ke->type = KE_CONST_FALSE;
	return ke;
}

/*
 * return the constant TRUE as a k_expr
 */
struct k_expr * get_const_true_as_kexpr()
{
	struct k_expr *ke = malloc(sizeof(struct k_expr));
	ke->type = KE_CONST_TRUE;
	return ke;
}

/*
 * retrieve value from satmap given a key (sat_variable_nr)
 */
struct fexpr * get_fexpr_from_satmap(int key)
{
	int *index = malloc(sizeof(*index));
	*index = abs(key);
	return (struct fexpr *) g_hash_table_lookup(satmap, index);
}

/*
 * parse an expr as a k_expr
 */
struct k_expr * parse_expr(struct expr *e, struct k_expr *parent)
{
	struct k_expr *ke = malloc(sizeof(struct k_expr));
	debug_print_kexpr(ke);
	ke->parent = parent;

	switch (e->type) {
	case E_SYMBOL:
		ke->type = KE_SYMBOL;
		ke->sym = e->left.sym;
		ke->tristate = no;
		return ke;
	case E_AND:
		ke->type = KE_AND;
		ke->left = parse_expr(e->left.expr, ke);
		ke->right = parse_expr(e->right.expr, ke);
		return ke;
	case E_OR:
		ke->type = KE_OR;
		ke->left = parse_expr(e->left.expr, ke);
		ke->right = parse_expr(e->right.expr, ke);
		return ke;
	case E_NOT:
		ke->type = KE_NOT;
		ke->left = parse_expr(e->left.expr, ke);
		ke->right = NULL;
		return ke;
	case E_EQUAL:
		ke->type = KE_EQUAL;
		ke->eqsym = e->left.sym;
		ke->eqvalue = e->right.sym;
		return ke;
	case E_UNEQUAL:
		ke->type = KE_UNEQUAL;
		ke->eqsym = e->left.sym;
		ke->eqvalue = e->right.sym;
		return ke;
	default:
		return NULL;
	}
}

/*
 * checks, if the symbol is a tristate-constant
 */
bool is_tristate_constant(struct symbol *sym) {
	return sym == &symbol_yes || sym == &symbol_mod || sym == &symbol_no;
}

/*
 * checks, if a symbol is of type boolean or tristate
 */
bool sym_is_boolean(struct symbol *sym)
{
	return sym_get_type(sym) == S_BOOLEAN || sym_get_type(sym) == S_TRISTATE;
}

/*
 * checks, if a symbol is a boolean/tristate or a tristate constant
 */
bool sym_is_bool_or_triconst(struct symbol *sym)
{
	return is_tristate_constant(sym) || sym_is_boolean(sym);
}

/*
 * checks, if a symbol is of type int, hex, or string
 */
bool sym_is_nonboolean(struct symbol *sym)
{
	return sym_get_type(sym) == S_INT || sym_get_type(sym) == S_HEX || sym_get_type(sym) == S_STRING;
}

/*
 * checks, if CNF clause is a tautology
 */
bool cnf_is_tautology(struct cnf_clause *cl)
{
	struct cnf_literal *lit;
	unsigned int i;
	for (i = 0; i < cl->lits->len; i++) {
		lit = g_array_index(cl->lits, struct cnf_literal *, i);
		/* clause is tautology, if a constant evaluates to true */
		if (lit->val == -(const_false->satval) || lit->val == const_true->satval)
			return true;
	}
	
	return false;
}

/*
 * add a constraint for a symbol
 */
void sym_add_constraint(struct symbol *sym, struct fexpr *constraint)
{
	if (!constraint) return;
	
	convert_fexpr_to_nnf(constraint);
	
	g_array_append_val(sym->constraints->arr, constraint);
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

	if (sym_get_type(sym) == S_BOOLEAN) {
		int tri_val = sym->def[S_DEF_USER].tri;
		tri_val = sym_get_tristate_value(sym);
		
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
	if (sym_get_type(sym) == S_TRISTATE) {
		int tri_val = sym->def[S_DEF_USER].tri;
		tri_val = sym_get_tristate_value(sym);
		
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
	if (sym_get_type(sym) == S_INT) {
		const char *string_val = sym_get_string_value(sym);
		
		struct fexpr *e;
		bool assumption_set = false;
		unsigned int i;
		
		/* do not set it for n */
		for (i = 1; i < sym->fexpr_nonbool->arr->len; i++) {
			e = g_array_index(sym->fexpr_nonbool->arr, struct fexpr *, i);
			if (strcmp(str_get(&e->nb_val), string_val) == 0) {
				picosat_assume(pico, e->satval);
				e->assumption = true;
				assumption_set = true;
			} else {
				picosat_assume(pico, -(e->satval));
				e->assumption = false;
			}
		}
		
		e = g_array_index(sym->fexpr_nonbool->arr, struct fexpr *, 0);
		/* if no assumption was set, set it to n */
		if (!assumption_set) {
			picosat_assume(pico, e->satval);
			e->assumption = true;
		} else {
			picosat_assume(pico, -(e->satval));
			e->assumption = false;
		}
// 		printf("Added assumption: %s %d\n", str_get(&e->name), e->assumption);
	}
}

/*
 * print a warning about unmet dependencies
 */
void sym_warn_unmet_dep(struct symbol *sym)
{
	struct gstr gs = str_new();

	str_printf(&gs,
		   "\nWARNING: unmet direct dependencies detected for %s\n",
		   sym->name);
	str_printf(&gs,
		   "  Depends on [%c]: ",
		   sym->dir_dep.tri == mod ? 'm' : 'n');
	expr_gstr_print(sym->dir_dep.expr, &gs);
	str_printf(&gs, "\n");

	expr_gstr_print_revdep(sym->rev_dep.expr, &gs, yes,
			       "  Selected by [y]:\n");
	expr_gstr_print_revdep(sym->rev_dep.expr, &gs, mod,
			       "  Selected by [m]:\n");

	fputs(str_get(&gs), stderr);
}

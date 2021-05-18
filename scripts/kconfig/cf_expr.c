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

static void create_fexpr_bool(struct symbol *sym);
static void create_fexpr_nonbool(struct symbol *sym);
static void create_fexpr_unknown(struct symbol *sym);
static void create_fexpr_choice(struct symbol *sym);

static int trans_count;


/*
 *  create a fexpr
 */
struct fexpr * fexpr_create(int satval, enum fexpr_type type, char *name)
{
	struct fexpr *e = xcalloc(1, sizeof(*e));
	e->satval = satval;
	e->type = type;
	e->name = str_new();
	str_append(&e->name, name);
	
	return e;
}

/*
 * create the fexpr for a symbol
 */
void sym_create_fexpr(struct symbol *sym)
{	
	if (sym_is_choice(sym))
		create_fexpr_choice(sym);
	else if (sym_is_boolean(sym))
		create_fexpr_bool(sym);
	else if (sym_is_nonboolean(sym))
		create_fexpr_nonbool(sym);
	else
		create_fexpr_unknown(sym);
}

/*
 * create the fexpr for symbols with reverse dependencies
 */
static void create_fexpr_selected(struct symbol *sym)
{
	/* fexpr_sel_y */
	struct fexpr *fexpr_sel_y = fexpr_create(sat_variable_nr++, FE_SELECT, sym->name);
	str_append(&fexpr_sel_y->name, "_sel_y");
	fexpr_sel_y->sym = sym;
	fexpr_add_to_satmap(fexpr_sel_y);
	
	sym->fexpr_sel_y = fexpr_sel_y;
	
	/* fexpr_sel_m */
	if (sym->type == S_BOOLEAN) return;
	struct fexpr *fexpr_sel_m = fexpr_create(sat_variable_nr++, FE_SELECT, sym->name);
	str_append(&fexpr_sel_m->name, "_sel_m");
	fexpr_sel_m->sym = sym;
	fexpr_add_to_satmap(fexpr_sel_m);
	
	sym->fexpr_sel_m = fexpr_sel_m;
}

/*
 * create the fexpr for a boolean/tristate symbol
 */
static void create_fexpr_bool(struct symbol *sym)
{
	struct fexpr *fexpr_y = fexpr_create(sat_variable_nr++, FE_SYMBOL, sym->name);
	fexpr_y->sym = sym;
	fexpr_y->tri = yes;
	fexpr_add_to_satmap(fexpr_y);
	
	sym->fexpr_y = fexpr_y;
	
	struct fexpr *fexpr_m;
	if (sym->type == S_TRISTATE) {
		fexpr_m = fexpr_create(sat_variable_nr++, FE_SYMBOL, sym->name);
		str_append(&fexpr_m->name, "_MODULE");
		fexpr_m->sym = sym;
		fexpr_m->tri = mod;
		fexpr_add_to_satmap(fexpr_m);
	} else {
		fexpr_m = const_false;
	}
	
	sym->fexpr_m = fexpr_m;
	
	if (sym->rev_dep.expr)
		create_fexpr_selected(sym);
}

/*
 * create the fexpr for a non-boolean symbol
 */
static void create_fexpr_nonbool(struct symbol *sym)
{
	sym->fexpr_y = NULL;
	sym->fexpr_m = NULL;
	sym->nb_vals = fexpr_list_init();

	/* default values */
	char int_values[][2] = {"n", "0", "1"};
	char hex_values[][4] = {"n", "0x0", "0x1"};
	char string_values[][9] = {"n", "", "nonempty"};
	
	int i;
	for (i = 0; i < 3; i++) {
		struct fexpr *e = fexpr_create(sat_variable_nr++, FE_NONBOOL, sym->name);
		e->sym = sym;
		str_append(&e->name, "=");
		e->nb_val = str_new();
		
		switch (sym->type) {
		case S_INT:
			str_append(&e->name, int_values[i]);
			str_append(&e->nb_val, int_values[i]);
			break;
		case S_HEX:
			str_append(&e->name, hex_values[i]);
			str_append(&e->nb_val, hex_values[i]);
			break;
		case S_STRING:
			str_append(&e->name, string_values[i]);
			str_append(&e->nb_val, string_values[i]);
			break;
		default:
			break;
		}

		fexpr_list_add(sym->nb_vals, e);
		fexpr_add_to_satmap(e);
	}
}


/*
 * set fexpr_y and fexpr_m simply to False
 */
static void create_fexpr_unknown(struct symbol *sym)
{
	sym->fexpr_y = const_false;
	sym->fexpr_m = const_false;
}


/*
 * create the fexpr for a choice symbol
 */
static void create_fexpr_choice(struct symbol *sym)
{
	assert(sym_is_boolean(sym));
	
	struct property *prompt = sym_get_prompt(sym);
	assert(prompt);
	
	char *name = strdup(prompt->text);
	
	/* remove spaces */
	char *write = name, *read = name;
	do {
		if (*read != ' ')
			*write++ = *read;
	} while (*read++);

	struct fexpr *fexpr_y = fexpr_create(sat_variable_nr++, FE_CHOICE, "Choice_");
	str_append(&fexpr_y->name, name);
	fexpr_y->sym = sym;
	fexpr_y->tri = yes;
	fexpr_add_to_satmap(fexpr_y);
	
	sym->fexpr_y = fexpr_y;
	
	struct fexpr *fexpr_m;
	if (sym->type == S_TRISTATE) {
		fexpr_m = fexpr_create(sat_variable_nr++, FE_CHOICE, "Choice_");
		str_append(&fexpr_m->name, name);
		str_append(&fexpr_m->name, "_MODULE");
		fexpr_m->sym = sym;
		fexpr_m->tri = mod;
		fexpr_add_to_satmap(fexpr_m);
	} else {
		fexpr_m = const_false;
	}
	sym->fexpr_m = fexpr_m;
}

/*
 * evaluate an unequality with GCC_VERSION
 */
static struct pexpr * gcc_version_eval(struct expr* e)
{
	if (!e) return pexf(const_false);
	
	long long actual_gcc_ver, sym_gcc_ver;
	if (e->left.sym == sym_find("GCC_VERSION")) {
		actual_gcc_ver = strtoll(sym_get_string_value(e->left.sym), NULL, 10);
		sym_gcc_ver = strtoll(e->right.sym->name, NULL, 10);
	} else {
		actual_gcc_ver = strtoll(sym_get_string_value(e->right.sym), NULL, 10);
		sym_gcc_ver = strtoll(e->left.sym->name, NULL, 10);
	}
	
	switch (e->type) {
	case E_LTH:
		return actual_gcc_ver < sym_gcc_ver ? pexf(const_true) : pexf(const_false);
	case E_LEQ:
		return actual_gcc_ver <= sym_gcc_ver ? pexf(const_true) : pexf(const_false);
	case E_GTH:
		return actual_gcc_ver > sym_gcc_ver ? pexf(const_true) : pexf(const_false);
	case E_GEQ:
		return actual_gcc_ver >= sym_gcc_ver ? pexf(const_true) : pexf(const_false);
	default:
		perror("Wrong type in gcc_version_eval.");
	}
	
	return pexf(const_false);
}

/*
 * evaluate an unequality with 2 boolean symbols
 */
static struct pexpr * expr_eval_unequal_bool(struct expr *e)
{
	if (!e) return pexf(const_false);
	
	assert(sym_is_boolean(e->left.sym));
	assert(sym_is_boolean(e->right.sym));
	
	int val_left = sym_get_tristate_value(e->left.sym);
	int val_right = sym_get_tristate_value(e->right.sym);
	
	switch (e->type) {
	case E_LTH:
		return val_left < val_right ? pexf(const_true) : pexf(const_false);
	case E_LEQ:
		return val_left <= val_right ? pexf(const_true) : pexf(const_false);
	case E_GTH:
		return val_left > val_right ? pexf(const_true) : pexf(const_false);
	case E_GEQ:
		return val_left >= val_right ? pexf(const_true) : pexf(const_false);
	default:
		perror("Wrong type in expr_eval_unequal_bool.");
	}
	
	return pexf(const_false);
}
/*
 * calculate, when expr will evaluate to yes or mod
 */
struct pexpr * expr_calculate_pexpr_both(struct expr *e)
{
	if (!e) return pexf(const_false);
	
	if (!expr_can_evaluate_to_mod(e))
		return expr_calculate_pexpr_y(e);
	
	switch (e->type) {
	case E_SYMBOL:
		return pexpr_or(expr_calculate_pexpr_m(e), expr_calculate_pexpr_y(e));
	case E_AND:
		return expr_calculate_pexpr_both_and(e->left.expr, e->right.expr);
	case E_OR:
		return expr_calculate_pexpr_both_or(e->left.expr, e->right.expr);
	case E_NOT:
		return pexpr_or(expr_calculate_pexpr_m(e), expr_calculate_pexpr_y(e));
	case E_EQUAL:
		return expr_calculate_pexpr_y_equals(e);
	case E_UNEQUAL:
		return expr_calculate_pexpr_y_unequals(e);
	case E_LTH:
	case E_LEQ:
	case E_GTH:
	case E_GEQ:
		return expr_calculate_pexpr_y_comp(e);
	default:
		// TODO
// 		perror("Unhandled type - expr_calculate_pexpr_both");
		return pexf(const_false);
	}
}

/*
 * calculate, when expr will evaluate to yes
 */
struct pexpr * expr_calculate_pexpr_y(struct expr *e){
	if (!e) return NULL;
	
	switch (e->type) {
	case E_SYMBOL:
		return pexf(e->left.sym->fexpr_y);
	case E_AND:
		return expr_calculate_pexpr_y_and(e->left.expr, e->right.expr);
	case E_OR:
		return expr_calculate_pexpr_y_or(e->left.expr, e->right.expr);
	case E_NOT:
		return expr_calculate_pexpr_y_not(e->left.expr);
	case E_EQUAL:
		return expr_calculate_pexpr_y_equals(e);
	case E_UNEQUAL:
		return expr_calculate_pexpr_y_unequals(e);
	case E_LTH:
	case E_LEQ:
	case E_GTH:
	case E_GEQ:
		return expr_calculate_pexpr_y_comp(e);
	default:
		perror("Unhandled type - expr_calculate_pexpr_y");
	}
}

/*
 * calculate, when expr will evaluate to mod
 */
struct pexpr * expr_calculate_pexpr_m(struct expr *e){
	if (!e) return NULL;
	
	if (!expr_can_evaluate_to_mod(e)) 
		return pexf(const_false);
	
	switch (e->type) {
	case E_SYMBOL:
		return pexf(e->left.sym->fexpr_m);
	case E_AND:
		return expr_calculate_pexpr_m_and(e->left.expr, e->right.expr);
	case E_OR:
		return expr_calculate_pexpr_m_or(e->left.expr, e->right.expr);
	case E_NOT:
		return expr_calculate_pexpr_m_not(e->left.expr);
	default:
		perror("Trying to evaluate to mod.");
	}
}

/*
 * calculate, when expr of type AND will evaluate to yes
 * A && B
 */
struct pexpr * expr_calculate_pexpr_y_and(struct expr *a, struct expr *b)
{
	return pexpr_and(expr_calculate_pexpr_y(a), expr_calculate_pexpr_y(b));
}

/*
 * calculate, when expr of type AND will evaluate to mod
 * (A || A_m) && (B || B_m) && !(A && B)
 */
struct pexpr * expr_calculate_pexpr_m_and(struct expr *a, struct expr *b)
{
	struct pexpr *topright = pexpr_not(pexpr_and(expr_calculate_pexpr_y(a), expr_calculate_pexpr_y(b)));
	struct pexpr *ll_left = pexpr_or(expr_calculate_pexpr_y(a), expr_calculate_pexpr_m(a));
	struct pexpr *ll_right = pexpr_or(expr_calculate_pexpr_y(b), expr_calculate_pexpr_m(b));
	struct pexpr *topleft = pexpr_and(ll_left, ll_right);

	return pexpr_and(topleft, topright);
}

/*
 * calculate, when expr of type AND will evaluate to mod or yes
 * (A || A_m) && (B || B_m)
 */
struct pexpr * expr_calculate_pexpr_both_and(struct expr *a, struct expr *b)
{
	struct pexpr *left = pexpr_or(expr_calculate_pexpr_y(a), expr_calculate_pexpr_m(a));
	struct pexpr *right = pexpr_or(expr_calculate_pexpr_y(b), expr_calculate_pexpr_m(b));
	return pexpr_and(left, right);
}

/*
 * calculate, when expr of type OR will evaluate to yes
 * A || B
 */
struct pexpr * expr_calculate_pexpr_y_or(struct expr *a, struct expr *b)
{
	return pexpr_or(expr_calculate_pexpr_y(a), expr_calculate_pexpr_y(b));
}

/*
 * calculate, when expr of type OR will evaluate to mod
 * (A_m || B_m) && !A && !B
 */
struct pexpr * expr_calculate_pexpr_m_or(struct expr *a, struct expr *b)
{
	struct pexpr *topright = pexpr_not(expr_calculate_pexpr_y(b));
	struct pexpr *lowerleft = pexpr_or(expr_calculate_pexpr_m(a), expr_calculate_pexpr_m(b));
	struct pexpr *topleft = pexpr_and(lowerleft, pexpr_not(expr_calculate_pexpr_y(a)));
	
	return pexpr_and(topleft, topright);
}

/*
 * calculate, when expr of type OR will evaluate to mod or yes
 * (A_m || A ||  B_m || B)
 */
struct pexpr * expr_calculate_pexpr_both_or(struct expr *a, struct expr *b)
{
	struct pexpr *left = pexpr_or(expr_calculate_pexpr_y(a), expr_calculate_pexpr_m(a));
	struct pexpr *right = pexpr_or(expr_calculate_pexpr_y(b), expr_calculate_pexpr_m(b));
	return pexpr_or(left, right);
}

/*
 * calculate, when expr of type NOT will evaluate to yes
 * !(A || A_m)
 */
struct pexpr * expr_calculate_pexpr_y_not(struct expr * e)
{
	return pexpr_not(pexpr_or(expr_calculate_pexpr_y(e), expr_calculate_pexpr_m(e)));
}

/*
 * calculate, when expr of type NOT will evaluate to mod
 * A_m
 */
struct pexpr * expr_calculate_pexpr_m_not(struct expr * e)
{
	return expr_calculate_pexpr_m(e);
}

static struct pexpr * equiv_pexpr(struct pexpr *a, struct pexpr *b)
{
	struct pexpr *yes = pexpr_and(a, b);
	struct pexpr *not = pexpr_and(pexpr_not(a), pexpr_not(b));
	
	return pexpr_or(yes, not);
}

/*
 * create the fexpr of a non-boolean symbol for a specific value
 */
struct fexpr * sym_create_nonbool_fexpr(struct symbol *sym, char *value)
{
	struct fexpr *e = sym_get_nonbool_fexpr(sym, value);
	
	/* fexpr already exists */
	if (e != NULL) return e;
	
	e = fexpr_create(sat_variable_nr++, FE_NONBOOL, sym->name);
	e->sym = sym;
	str_append(&e->name, "=");
	str_append(&e->name, value);
	e->nb_val = str_new();
	str_append(&e->nb_val, value);
	
	fexpr_list_add(sym->nb_vals, e);
	fexpr_add_to_satmap(e);
	
	return e;
}

/*
 * return the fexpr of a non-boolean symbol for a specific value, NULL if non-existent
 */
struct fexpr * sym_get_nonbool_fexpr(struct symbol *sym, char *value)
{
	struct fexpr_node *e;
	fexpr_list_for_each(e, sym->nb_vals) {
		if (strcmp(str_get(&e->elem->nb_val), value) == 0)
			return e->elem;
	}
	
	return NULL;
}

/*
 * return the fexpr of a non-boolean symbol for a specific value, if it exists
 * otherwise create it
 */
struct fexpr * sym_get_or_create_nonbool_fexpr(struct symbol *sym, char *value)
{
	struct fexpr *e = sym_get_nonbool_fexpr(sym, value);
	
	if (e != NULL)
		return e;
	else
		return sym_create_nonbool_fexpr(sym, value);
}

/*
 * calculate, when expr of type EQUAL will evaluate to yes
 * (A=B) && (A_m=B_m)
 */
struct pexpr * expr_calculate_pexpr_y_equals(struct expr *e)
{
	/* comparing 2 tristate constants */
	if (sym_is_tristate_constant(e->left.sym) && sym_is_tristate_constant(e->right.sym))
		return e->left.sym == e->right.sym ? pexf(const_true) : pexf(const_false);
	
	/* comparing 2 nonboolean constants */
	if (e->left.sym->type == S_UNKNOWN && e->right.sym->type == S_UNKNOWN)
		return strcmp(e->left.sym->name, e->right.sym->name) == 0 ? pexf(const_true) : pexf(const_false);
	
	/* comparing 2 boolean/tristate incl. yes/mod/no constants */
	if (sym_is_bool_or_triconst(e->left.sym) && sym_is_bool_or_triconst(e->right.sym)) {
		struct pexpr *yes = equiv_pexpr(pexf(e->left.sym->fexpr_y), pexf(e->right.sym->fexpr_y));
		struct pexpr *mod = equiv_pexpr(pexf(e->left.sym->fexpr_m), pexf(e->right.sym->fexpr_m));
		
		return pexpr_and(yes, mod);
	}
	
	/* comparing nonboolean with a constant */
	if (sym_is_nonboolean(e->left.sym) && e->right.sym->type == S_UNKNOWN) {
		return pexf(sym_get_or_create_nonbool_fexpr(e->left.sym, e->right.sym->name));
	}
	if (e->left.sym->type == S_UNKNOWN && sym_is_nonboolean(e->right.sym)) {
		return pexf(sym_get_or_create_nonbool_fexpr(e->right.sym, e->left.sym->name));
	}
	
	/* comparing nonboolean with tristate constant, will never be true */
	if (sym_is_nonboolean(e->left.sym) && sym_is_tristate_constant(e->right.sym))
		return pexf(const_false);
	if (sym_is_tristate_constant(e->left.sym) && sym_is_nonboolean(e->right.sym))
		return pexf(const_false);
	
	/* comparing 2 nonboolean symbols */
	// TODO
	
	/* comparing boolean item with nonboolean constant, will never be true */
	// TODO
	
	return pexf(const_false);
}

/*
 * transform an UNEQUAL into a Not(EQUAL)
 */
struct pexpr * expr_calculate_pexpr_y_unequals(struct expr *e)
{
	return pexpr_not(expr_calculate_pexpr_y_equals(e));
}

struct pexpr * expr_calculate_pexpr_y_comp(struct expr *e)
{
	if (!e) return pexf(const_false);
	
	switch (e->type) {
	case E_LTH:
	case E_LEQ:
	case E_GTH:
	case E_GEQ:
		/* "special" hack for GCC_VERSION */
		if (expr_contains_symbol(e, sym_find("GCC_VERSION")))
			return gcc_version_eval(e);
		
		/* "special" hack for CRAMFS <= MTD */
		if (expr_contains_symbol(e, sym_find("CRAMFS")) && expr_contains_symbol(e, sym_find("MTD")))
			return expr_eval_unequal_bool(e);
		
// 		print_expr("Unhandled", e, 0);
// 		getchar();
		return pexf(const_false);
	default:
		perror("Unhandled type - expr_calculate_pexpr_y_comp");
	}
}

/*
 * macro to create a pexpr of type AND
 */
struct pexpr * pexpr_and(struct pexpr *a, struct pexpr *b)
{
	// TODO optimise
	/* simplifications:
	 * expr && False -> False
	 * expr && True  -> expr
	 * expr && expr  -> expr
	 */
	if (a->type == PE_SYMBOL && a->left.fexpr == const_false) {
// 		pexpr_free(b);
		return a;
	}
	if (b->type == PE_SYMBOL && b->left.fexpr == const_false) {
// 		pexpr_free(a);
		return b;
	}

	if (a->type == PE_SYMBOL && a->left.fexpr == const_true) {
		return b;
	}
	if (b->type == PE_SYMBOL && b->left.fexpr == const_true) {
		return a;
	}
	
	if (pexpr_eq(a,b)) {
// 		pexpr_free(b);
		return a;
	}
	
	struct pexpr *e = xcalloc(1, sizeof(*e));
	e->type = PE_AND;
	e->left.pexpr = a;
	e->right.pexpr = b;
	return e;
}

/*
 * macro to create a pexpr of type OR
 */
struct pexpr * pexpr_or(struct pexpr *a, struct pexpr *b)
{	
	// TODO optimise
	/* simplifications:
	 * expr || False -> expr
	 * expr || True  -> True
	 * expr || expr  -> expr
	 */
	if (a->type == PE_SYMBOL && a->left.fexpr == const_false) {
// 		if (a->type == PE_AND || a->type == PE_OR)
// 			pexpr_free(a);
		return b;
	}
	if (b->type == PE_SYMBOL && b->left.fexpr == const_false) {
// 		if (b->type ==PE_AND || b->type == PE_OR)
// 			pexpr_free(b);
		return a;
	}
	
	if (a->type == PE_SYMBOL && a->left.fexpr == const_true) {
// 		if (b->type ==PE_AND || b->type == PE_OR)
// 			pexpr_free(b);
		return a;
	}
	if (b->type == PE_SYMBOL && b->left.fexpr == const_true) {
// 		if (a->type == PE_AND || a->type == PE_OR)
// 			pexpr_free(a);
		return b;
	}

	if (pexpr_eq(a,b)) {
// 		pexpr_free(b);
		return a;
	}
	
	struct pexpr *e = xcalloc(1, sizeof(*e));
	e->type = PE_OR;
	e->left.pexpr = a;
	e->right.pexpr = b;
	return e;
}

/*
 * macro to create a pexpr of type NOT
 */
struct pexpr * pexpr_not(struct pexpr *a)
{
	// TODO optimise
	if (a->type == PE_SYMBOL && a->left.fexpr == const_false)
		return pexf(const_true);
	if (a->type == PE_SYMBOL && a->left.fexpr == const_true)
		return pexf(const_false);
	
	/* eliminate double negation */
	if (a->type == PE_NOT)
		return a->left.pexpr;
	
	/* De Morgan */
	if (a->type == PE_AND) {
		struct pexpr *e = xcalloc(1, sizeof(*e));
		e->type = PE_OR;
		e->left.pexpr = pexpr_not(a->left.pexpr);
		e->right.pexpr = pexpr_not(a->right.pexpr);
		return e;
	}
	if (a->type == PE_OR) {
		struct pexpr *e = xcalloc(1, sizeof(*e));
		e->type = PE_AND;
		e->left.pexpr = pexpr_not(a->left.pexpr);
		e->right.pexpr = pexpr_not(a->right.pexpr);
		return e;
	}
	
	struct pexpr *e = xcalloc(1, sizeof(*e));
	e->type = PE_NOT;
	e->left.pexpr = a;
	return e;
}

/*
 * check whether a pexpr is in CNF
 */
bool pexpr_is_cnf(struct pexpr *e)
{
	if (!e) return false;
	
	switch (e->type) {
	case PE_SYMBOL:
		return true;
	case PE_AND:
		return false;
	case PE_OR:
		return pexpr_is_cnf(e->left.pexpr) && pexpr_is_cnf(e->right.pexpr);
	case PE_NOT:
		return e->left.pexpr->type == PE_SYMBOL;
	}
	
	return false;
}

/*
 * check whether a pexpr is in NNF
 */
bool pexpr_is_nnf(struct pexpr *e)
{
	if (!e) return false;
	
	switch (e->type) {
	case PE_SYMBOL:
		return true;
	case PE_AND:
	case PE_OR:
		return pexpr_is_nnf(e->left.pexpr) && pexpr_is_nnf(e->right.pexpr);
	case PE_NOT:
		return e->left.pexpr->type == PE_SYMBOL;
	}
	
	return false;
}


/*
 * return fexpr_both for a symbol
 */
struct pexpr * sym_get_fexpr_both(struct symbol *sym)
{
	return sym->type == S_TRISTATE ? pexpr_or(pexf(sym->fexpr_m), pexf(sym->fexpr_y)) : pexf(sym->fexpr_y);
}

/*
 * return fexpr_sel_both for a symbol
 */
struct pexpr * sym_get_fexpr_sel_both(struct symbol *sym)
{
	if (!sym->rev_dep.expr)
		return pexf(const_false);
	
	return sym->type == S_TRISTATE ? pexpr_or(pexf(sym->fexpr_sel_m), pexf(sym->fexpr_sel_y)) : pexf(sym->fexpr_sel_y);
}

/*
 * macro to construct a pexpr for "A implies B"
 */
struct pexpr * pexpr_implies(struct pexpr *a, struct pexpr *b)
{
	return pexpr_or(pexpr_not(a), b);
}

/*
 * check, if the fexpr is a symbol, a True/False-constant, a literal symbolizing a non-boolean or a choice symbol
 */
bool fexpr_is_symbol(struct fexpr *e)
{
	return e->type == FE_SYMBOL || e->type == FE_FALSE || e->type == FE_TRUE || e->type == FE_NONBOOL || e->type == FE_CHOICE || e->type == FE_SELECT || e->type == FE_NPC;
}

/* 
 * check whether a pexpr is a symbol or a negated symbol 
 */
bool pexpr_is_symbol(struct pexpr *e)
{
	return e->type == PE_SYMBOL || (e->type == PE_NOT && e->left.pexpr->type == PE_SYMBOL);
}

/*
 * check whether the fexpr is a constant (true/false)
 */
bool fexpr_is_constant(struct fexpr *e)
{
	return e == const_true || e == const_false;
}

/* 
 * add a fexpr to the satmap
 */
void fexpr_add_to_satmap(struct fexpr *e)
{
	if (e->satval >= satmap_size) {
		satmap = xrealloc(satmap, satmap_size * 2 * sizeof(*satmap));
		satmap_size *= 2;
	}
	
	satmap[e->satval] = *e;
}

/*
 * print a fexpr
 */
void fexpr_print(char *tag, struct fexpr *e)
{
	if (!e) return;
	
	printf("%s: %s\n", tag, str_get(&e->name));
}

/*
 * write an fexpr into a string (format needed for testing)
 */
void fexpr_as_char(struct fexpr *e, struct gstr *s)
{
	if (!e) return;
	
	switch (e->type) {
	case FE_SYMBOL:
	case FE_CHOICE:
	case FE_SELECT:
	case FE_NPC:
	case FE_NONBOOL:
		str_append(s, "definedEx(");
		str_append(s, str_get(&e->name));
		str_append(s, ")");
		return;
	case FE_FALSE:
		str_append(s, "0");
		return;
	case FE_TRUE:
		str_append(s, "1");
		return;
	default:
		return;
	}
}

/*
 * write a pexpr into a string
 */
void pexpr_as_char_short(struct pexpr *e, struct gstr *s, int parent)
{
	if (!e) return;
	
	switch (e->type) {
	case PE_SYMBOL:
		str_append(s, str_get(&e->left.fexpr->name));
		return;
	case PE_AND:
		/* need this hack for the FeatureExpr parser */
		if (parent != PE_AND)
			str_append(s, "(");
		pexpr_as_char_short(e->left.pexpr, s, PE_AND);
		str_append(s, " && ");
		pexpr_as_char_short(e->right.pexpr, s, PE_AND);
		if (parent != PE_AND)
			str_append(s, ")");
		return;
	case PE_OR:
		if (parent != PE_OR)
			str_append(s, "(");
		pexpr_as_char_short(e->left.pexpr, s, PE_OR);
		str_append(s, " || ");
		pexpr_as_char_short(e->right.pexpr, s, PE_OR);
		if (parent != PE_OR)
			str_append(s, ")");
		return;
	case PE_NOT:
		str_append(s, "!");
		pexpr_as_char_short(e->left.pexpr, s, PE_NOT);
		return;
	}
}

/* 
 * init list of fexpr
 */
struct fexpr_list * fexpr_list_init()
{
	struct fexpr_list *list = xcalloc(1, sizeof(*list));
	list->head = NULL;
	list->tail = NULL;
	list->size = 0;
	
	return list;
}

/* 
 * init list of fexpr_list
 */
struct fexl_list * fexl_list_init()
{
	struct fexl_list *list = xcalloc(1, sizeof(*list));
	list->head = NULL;
	list->tail = NULL;
	list->size = 0;
	
	return list;
}

/* 
 * init list of pexpr
 */
struct pexpr_list * pexpr_list_init()
{
	struct pexpr_list *list = xcalloc(1, sizeof(*list));
	list->head = NULL;
	list->tail = NULL;
	list->size = 0;
	
	return list;
}

/* 
 * init list of symbol_fix
 */
struct sfix_list * sfix_list_init(void)
{
	struct sfix_list *list = xcalloc(1, sizeof(*list));
	list->head = NULL;
	list->tail = NULL;
	list->size = 0;
	
	return list;
}

/* 
 * init list of symbol_fix
 */
struct sfl_list * sfl_list_init(void)
{
	struct sfl_list *list = xcalloc(1, sizeof(*list));
	list->head = NULL;
	list->tail = NULL;
	list->size = 0;
	
	return list;
}

/* 
 * init list of symbol_dvalue
 */
struct sdv_list * sdv_list_init(void)
{
	struct sdv_list *list = xcalloc(1, sizeof(*list));
	list->head = NULL;
	list->tail = NULL;
	list->size = 0;
	
	return list;
}

/* 
 * init list of symbols
 */
struct sym_list * sym_list_init(void)
{
	struct sym_list *list = xcalloc(1, sizeof(*list));
	list->head = NULL;
	list->tail = NULL;
	list->size = 0;
	
	return list;
}

/* 
 * init list of default_maps
 */
struct defm_list * defm_list_init(void)
{
	struct defm_list *list = xcalloc(1, sizeof(*list));
	list->head = NULL;
	list->tail = NULL;
	list->size = 0;
	
	return list;
}

/* 
 * add element to tail of a fexpr_list
 */
void fexpr_list_add(struct fexpr_list *list, struct fexpr *fe)
{
	struct fexpr_node *node = xcalloc(1, sizeof(*node));
	node->elem = fe;
	
	if (list->size == 0) {
		list->head = node;
		list->tail = node;
	} else {
		node->prev = list->tail;
		list->tail = node;
		node->prev->next = node;
	}

	list->size++;
}

/* 
 * add element to tail of a fexl_list
 */
void fexl_list_add(struct fexl_list *list, struct fexpr_list *elem)
{
	struct fexl_node *node = xcalloc(1, sizeof(*node));
	node->elem = elem;
	
	if (list->size == 0) {
		list->head = node;
		list->tail = node;
	} else {
		node->prev = list->tail;
		list->tail = node;
		node->prev->next = node;
	}

	list->size++;
}

/* 
 * add element to tail of a pexpr_list
 */
void pexpr_list_add(struct pexpr_list *list, struct pexpr *e)
{
	struct pexpr_node *node = xcalloc(1, sizeof(*node));
	node->elem = e;
	
	if (list->size == 0) {
		list->head = node;
		list->tail = node;
	} else {
		node->prev = list->tail;
		list->tail = node;
		node->prev->next = node;
	}

	list->size++;
}

/* 
 * add element to tail of a sfix_list
 */
void sfix_list_add(struct sfix_list *list, struct symbol_fix *fix)
{
	struct sfix_node *node = xcalloc(1, sizeof(*node));
	node->elem = fix;
	
	if (list->size == 0) {
		list->head = node;
		list->tail = node;
	} else {
		node->prev = list->tail;
		list->tail = node;
		node->prev->next = node;
	}

	list->size++;
}

/* 
 * add element to tail of a sfl_list
 */
void sfl_list_add(struct sfl_list *list, struct sfix_list *elem)
{
	struct sfl_node *node = xcalloc(1, sizeof(*node));
	node->elem = elem;
	
	if (list->size == 0) {
		list->head = node;
		list->tail = node;
	} else {
		node->prev = list->tail;
		list->tail = node;
		node->prev->next = node;
	}

	list->size++;
}

/* 
 * add element to tail of a sdv_list
 */
void sdv_list_add(struct sdv_list *list, struct symbol_dvalue *sdv)
{
	struct sdv_node *node = xcalloc(1, sizeof(*node));
	node->elem = sdv;
	
	if (list->size == 0) {
		list->head = node;
		list->tail = node;
	} else {
		node->prev = list->tail;
		list->tail = node;
		node->prev->next = node;
	}

	list->size++;
}

/* 
 * add element to tail of a sym_list
 */
void sym_list_add(struct sym_list *list, struct symbol *sym)
{
	struct sym_node *node = xcalloc(1, sizeof(*node));
	node->elem = sym;
	
	if (list->size == 0) {
		list->head = node;
		list->tail = node;
	} else {
		node->prev = list->tail;
		list->tail = node;
		node->prev->next = node;
	}

	list->size++;
}

/* 
 * add element to tail of a defm_list 
 */
void defm_list_add(struct defm_list *list, struct default_map *map)
{
	struct defm_node *node = xcalloc(1, sizeof(*node));
	node->elem = map;
	
	if (list->size == 0) {
		list->head = node;
		list->tail = node;
	} else {
		node->prev = list->tail;
		list->tail = node;
		node->prev->next = node;
	}

	list->size++;
}

/* 
 * delete an element from a fexpr_list
 */
void fexpr_list_delete(struct fexpr_list *list, struct fexpr_node *node)
{
	assert(node != NULL);
	if (list->size == 0) return;
	
	if (node == list->head)
		list->head = node->next;
	else
		node->prev->next = node->next;
	
	if (node == list->tail)
		list->tail = node->prev;
	else
		node->next->prev = node->prev;
	
	list->size--;
	free(node);
}

/* 
 * delete an element from a fexpr_list
 */
void sfix_list_delete(struct sfix_list *list, struct sfix_node *node)
{
	assert(node != NULL);
	if (list->size == 0) return;
	
	if (node == list->head)
		list->head = node->next;
	else
		node->prev->next = node->next;
	
	if (node == list->tail)
		list->tail = node->prev;
	else
		node->next->prev = node->prev;
	
	list->size--;
	free(node);
}

/* 
 * delete an element from a fexl_list
 */
void fexl_list_delete(struct fexl_list *list, struct fexl_node *node)
{
	assert(node != NULL);
	if (list->size == 0) return;
	
	if (node == list->head)
		list->head = node->next;
	else
		node->prev->next = node->next;
	
	if (node == list->tail)
		list->tail = node->prev;
	else
		node->next->prev = node->prev;
	
	list->size--;
	free(node);
}

/* 
 * delete the first occurence of elem in an fexl_list
 */
void fexl_list_delete_elem(struct fexl_list *list, struct fexpr_list *elem)
{
	struct fexl_node *node, *to_delete = NULL;
	fexl_list_for_each(node, list) {
		if (node->elem == elem) {
			to_delete = node;
			break;
		}
	}
	
	if (to_delete != NULL)
		fexl_list_delete(list, to_delete);
}

/* 
 * make a shallow copy of a fexpr_list 
 */
struct fexpr_list * fexpr_list_copy(struct fexpr_list *list)
{
	struct fexpr_list *ret = fexpr_list_init();
	struct fexpr_node *node;
	fexpr_list_for_each(node, list)
		fexpr_list_add(ret, node->elem);
	
	return ret;
}

/* 
 * make a shallow copy of a fexl_list 
 */
struct fexl_list * fexl_list_copy(struct fexl_list *list)
{
	struct fexl_list *ret = fexl_list_init();
	struct fexl_node *node;
	fexl_list_for_each(node, list)
		fexl_list_add(ret, node->elem);
	
	return ret;
}

/* 
 * make a shallow copy of a sdv_list
 */
struct sdv_list * sdv_list_copy(struct sdv_list *list)
{
	struct sdv_list *ret = sdv_list_init();
	struct sdv_node *node;
	sdv_list_for_each(node, list)
		sdv_list_add(ret, node->elem);

	
	return ret;
}

/* 
 * make a shallow copy of a sfix_list
 */
struct sfix_list * sfix_list_copy(struct sfix_list *list)
{
	struct sfix_list *ret = sfix_list_init();
	struct sfix_node *node;
	sfix_list_for_each(node, list)
		sfix_list_add(ret, node->elem);

	
	return ret;
}

/* 
 * print a fexpr_list
 */
void fexpr_list_print(char *title, struct fexpr_list *list)
{
	struct fexpr_node *node;
	printf("%s: [", title);
	
	fexpr_list_for_each(node, list) {
		printf("%s", str_get(&node->elem->name));
		if (node->next != NULL)
			printf(", ");
	}
	
	printf("]\n");
}

/*
 * print a fexl_list
 */
void fexl_list_print(char *title, struct fexl_list *list)
{
	struct fexl_node *node;
	printf("%s:\n", title);
	
	fexl_list_for_each(node, list)
		fexpr_list_print(":", node->elem);
}

/* 
 * free an fexpr_list
 */
void fexpr_list_free(struct fexpr_list *list)
{
	struct fexpr_node *node = list->head, *tmp;
	
	while (node != NULL) {
		tmp = node->next;
		free(node);
		node = tmp;
	}
	
	free(list);
}

/* 
 * free an fexl_list
 */
void fexl_list_free(struct fexl_list *list){
	struct fexl_node *node = list->head, *tmp;
	
	while (node != NULL) {
		tmp = node->next;
		free(node);
		node = tmp;
	}
	
	free(list);
}

/* 
 * free a sdv_list
 */
void sdv_list_free(struct sdv_list *list)
{
	struct sdv_node *node = list->head, *tmp;
	
	while (node != NULL) {
		tmp = node->next;
		free(node);
		node = tmp;
	}
	
	free(list);
}

/*
 * simplify a pexpr in-place
 * 	pexpr && False -> False
 * 	pexpr && True  -> pexpr
 * 	pexpr || False -> pexpr
 * 	pexpr || True  -> True
 */
static struct pexpr * pexpr_eliminate_yn(struct pexpr *e)
{
	struct pexpr *tmp;
	
	if (e) switch (e->type) {
	case PE_AND:
		e->left.pexpr = pexpr_eliminate_yn(e->left.pexpr);
		e->right.pexpr = pexpr_eliminate_yn(e->right.pexpr);
		if (e->left.pexpr->type == PE_SYMBOL) {
			if (e->left.pexpr->left.fexpr == const_false) {
				pexpr_free(e->left.pexpr);
				pexpr_free(e->right.pexpr);
				e->type = PE_SYMBOL;
				e->left.fexpr = const_false;
				e->right.pexpr = NULL;
				return e;
			} else if (e->left.pexpr->left.fexpr == const_true) {
				free(e->left.pexpr);
				tmp = e->right.pexpr;
				*e = *(e->right.pexpr);
				free(tmp);
				return e;
			}
		}
		if (e->right.pexpr->type == PE_SYMBOL) {
			if (e->right.pexpr->left.fexpr == const_false) {
				pexpr_free(e->left.pexpr);
				pexpr_free(e->right.pexpr);
				e->type = PE_SYMBOL;
				e->left.fexpr = const_false;
				e->right.fexpr = NULL;
				return e;
			} else if (e->right.pexpr->left.fexpr == const_true) {
				free(e->right.pexpr);
				tmp = e->left.pexpr;
				*e = *(e->left.pexpr);
				free(tmp);
				return e;
			}
		}
		break;
	case PE_OR:
		e->left.pexpr = pexpr_eliminate_yn(e->left.pexpr);
		e->right.pexpr = pexpr_eliminate_yn(e->right.pexpr);
		if (e->left.pexpr->type == PE_SYMBOL) {
			if (e->left.pexpr->left.fexpr == const_false) {
				free(e->left.pexpr);
				tmp = e->right.pexpr;
				*e = *(e->right.pexpr);
				free(tmp);
				return e;
			} else if (e->left.pexpr->left.fexpr == const_true) {
				pexpr_free(e->left.pexpr);
				pexpr_free(e->right.pexpr);
				e->type = PE_SYMBOL;
				e->left.fexpr = const_true;
				e->right.pexpr = NULL;
			}
		}
		if (e->right.pexpr->type == PE_SYMBOL) {
			if (e->right.pexpr->left.fexpr == const_false) {
				free(e->right.pexpr);
				tmp = e->left.pexpr;
				*e = *(e->left.pexpr);
				free(tmp);
				return e;
			} else if (e->right.pexpr->left.fexpr == const_true) {
				pexpr_free(e->left.pexpr);
				pexpr_free(e->right.pexpr);
				e->type = PE_SYMBOL;
				e->left.fexpr = const_true;
				e->right.pexpr = NULL;
				return e;
			}
		}
	default:
		;
	}
	
	return e;
}

/*
 * copy a pexpr
 */
struct pexpr * pexpr_copy(const struct pexpr *org)
{
	struct pexpr *e;
	
	if (!org) return NULL;
	
	e = xmalloc(sizeof(*org));
	memcpy(e, org, sizeof(*org));
	switch (org->type) {
	case PE_SYMBOL:
		e->left = org->left;
		break;
	case PE_AND:
	case PE_OR:
		e->left.pexpr = pexpr_copy(org->left.pexpr);
		e->right.pexpr = pexpr_copy(org->right.pexpr);
		break;
	case PE_NOT:
		e->left.pexpr = pexpr_copy(org->left.pexpr);
		break;
	}
	
	return e;
}

/*
 * free a pexpr
 */
void pexpr_free(struct pexpr *e)
{
	if (!e) return;
	
	switch (e->type) {
	case PE_SYMBOL:
		break;
	case PE_AND:
	case PE_OR:
		pexpr_free(e->left.pexpr);
		pexpr_free(e->right.pexpr);
		break;
	case PE_NOT:
		pexpr_free(e->left.pexpr);
		break;
	}
	
	free(e);
}

#define e1 (*ep1)
#define e2 (*ep2)
/*
 * pexpr_eliminate_eq() helper
 */
static void __pexpr_eliminate_eq(enum pexpr_type type, struct pexpr **ep1, struct pexpr **ep2)
{
	/* recurse down to the leaves */
	if (e1->type == type) {
		__pexpr_eliminate_eq(type, &e1->left.pexpr, &e2);
		__pexpr_eliminate_eq(type, &e1->right.pexpr, &e2);
		return;
	}
	if (e2->type == type) {
		__pexpr_eliminate_eq(type, &e1, &e2->left.pexpr);
		__pexpr_eliminate_eq(type, &e1, &e2->right.pexpr);
		return;
	}
	
	/* e1 and e2 are leaves. Compare them. */
	if (e1->type == PE_SYMBOL && e2->type == PE_SYMBOL &&
		e1->left.fexpr->satval == e2->left.fexpr->satval &&
		(e1->left.fexpr == const_true || e2->left.fexpr == const_false))
		return;
	if (!pexpr_eq(e1, e2))
		return;
	
	/* e1 and e2 are equal leaves. Prepare them for elimination. */
	trans_count++;
	pexpr_free(e1);
	pexpr_free(e2);
	switch (type) {
	case PE_AND:
		e1 = pexf(const_true);
		e2 = pexf(const_true);
		break;
	case PE_OR:
		e1 = pexf(const_false);
		e2 = pexf(const_false);
		break;
	default:
		;
	}
}

/*
 * rewrite pexpr ep1 and ep2 to remove operands common to both
 */
static void pexpr_eliminate_eq(struct pexpr **ep1, struct pexpr **ep2)
{	
	if (!e1 || !e2) return;
	
	switch (e1->type) {
	case PE_AND:
	case PE_OR:
		__pexpr_eliminate_eq(e1->type, ep1, ep2);
	default:
		;
	}
	if (e1->type != e2->type) switch (e2->type) {
	case PE_AND:
	case PE_OR:
		__pexpr_eliminate_eq(e2->type, ep1, ep2);
	default:
		;
	}
	e1 = pexpr_eliminate_yn(e1);
	e2 = pexpr_eliminate_yn(e2);
}
#undef e1
#undef e2

/*
 * check whether 2 pexpr are equal
 */
bool pexpr_eq(struct pexpr *e1, struct pexpr *e2)
{
	bool res;
	int old_count;
	
	if (!e1 || !e2) return false;
	
	if (e1->type != e2->type)
		return false;
	
	switch (e1->type) {
	case PE_SYMBOL:
		return e1->left.fexpr->satval == e2->left.fexpr->satval;
	case PE_AND:
	case PE_OR:
		e1 = pexpr_copy(e1);
		e2 = pexpr_copy(e2);
		old_count = trans_count;
		pexpr_eliminate_eq(&e1, &e2);
		res = (e1->type == PE_SYMBOL && e2->type == PE_SYMBOL &&
			e1->left.fexpr->satval == e2->left.fexpr->satval);
		pexpr_free(e1);
		pexpr_free(e2);
		trans_count = old_count;
		return res;
	case PE_NOT:
		return pexpr_eq(e1->left.pexpr, e2->left.pexpr);
	}
	
	return false;
}

/*
 * print a pexpr
 */
static void pexpr_print_util(struct pexpr *e, int prevtoken)
{
	if (!e) return;
	
	switch (e->type) {
	case PE_SYMBOL:
		printf("%s", str_get(&e->left.fexpr->name));
		break;
	case PE_AND:
		if (prevtoken != PE_AND && prevtoken != -1)
			printf("(");
		pexpr_print_util(e->left.pexpr, PE_AND);
		printf(" && ");
		pexpr_print_util(e->right.pexpr, PE_AND);
		if (prevtoken != PE_AND && prevtoken != -1)
			printf(")");
		break;
	case PE_OR:
		if (prevtoken != PE_OR && prevtoken != -1)
			printf("(");
		pexpr_print_util(e->left.pexpr, PE_OR);
		printf(" || ");
		pexpr_print_util(e->right.pexpr, PE_OR);
		if (prevtoken != PE_OR && prevtoken != -1)
			printf(")");
		break;
	case PE_NOT:
		printf("!");
		pexpr_print_util(e->left.pexpr, PE_NOT);
		break;
	}
}
void pexpr_print(char *tag, struct pexpr *e, int prevtoken)
{
	printf("%s: ", tag);
	pexpr_print_util(e, prevtoken);
	printf("\n");
}

/*
 * convert a fexpr to a pexpr
 */
struct pexpr * pexf(struct fexpr *fe)
{
	struct pexpr *pe = xcalloc(1, sizeof(*pe));
	pe->type = PE_SYMBOL;
	pe->left.fexpr = fe;
	return pe;
}

static struct pexpr * pexpr_join_or(struct pexpr *e1, struct pexpr *e2)
{
	if (pexpr_eq(e1, e2)) 
		return pexpr_copy(e1);
	else
		return NULL;
}

static struct pexpr * pexpr_join_and(struct pexpr *e1, struct pexpr *e2)
{	
	if (pexpr_eq(e1, e2)) 
		return pexpr_copy(e1);
	else
		return NULL;
}

/*
 * pexpr_eliminate_dups() helper.
 */
static void pexpr_eliminate_dups1(enum pexpr_type type, struct pexpr **ep1, struct pexpr **ep2)
{
#define e1 (*ep1)
#define e2 (*ep2)
	
	struct pexpr *tmp;
	
	/* recurse down to leaves */
	if (e1->type == type) {
		pexpr_eliminate_dups1(type, &e1->left.pexpr, &e2);
		pexpr_eliminate_dups1(type, &e1->right.pexpr, &e2);
		return;
	}
	if (e2->type == type) {
		pexpr_eliminate_dups1(type, &e1, &e2->left.pexpr);
		pexpr_eliminate_dups1(type, &e1, &e2->right.pexpr);
		return;
	}
	
	/* e1 and e2 are leaves. Compare them. */
	
	if (e1 == e2) return;
	
	switch (e1->type) {
	case PE_AND:
	case PE_OR:
		pexpr_eliminate_dups1(e1->type, &e1, &e1);
	default:
		;
	}
	
	switch (type) {
	case PE_AND:
		tmp = pexpr_join_and(e1, e2);
		if (tmp) {
			pexpr_free(e1);
			pexpr_free(e2);
			e1 = pexf(const_true);
			e2 = tmp;
			trans_count++;
		}
		break;
	case PE_OR:
		tmp = pexpr_join_or(e1, e2);
		if (tmp) {
			pexpr_free(e1);
			pexpr_free(e2);
			e1 = pexf(const_false);
			e2 = tmp;
			trans_count++;
		}
		break;
	default:
		;
	}
	
#undef e1
#undef e2	
}

/*
 * eliminate duplicate and redundant operands
 */
struct pexpr * pexpr_eliminate_dups(struct pexpr *e)
{
	if (!e) return e;
	
	int oldcount = trans_count;
	while (true) {
		trans_count = 0;
		switch (e->type) {
		case PE_AND:
		case PE_OR:
			pexpr_eliminate_dups1(e->type, &e, &e);
		default:
			;
		}
		if (!trans_count)
			/* no simplification done in this pass. We're done. */
			break;
		e = pexpr_eliminate_yn(e);
	}
	trans_count = oldcount;
	return e;
}

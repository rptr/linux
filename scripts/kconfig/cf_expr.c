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
	struct fexpr *e = malloc(sizeof(struct fexpr));
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
	/* add it to satmap */
	g_hash_table_insert(satmap, &fexpr_sel_y->satval, fexpr_sel_y);
	
	sym->fexpr_sel_y = fexpr_sel_y;
	
	/* fexpr_sel_m */
	if (sym->type == S_BOOLEAN) return;
	struct fexpr *fexpr_sel_m = fexpr_create(sat_variable_nr++, FE_SELECT, sym->name);
	str_append(&fexpr_sel_m->name, "_sel_m");
	fexpr_sel_m->sym = sym;
	/* add it to satmap */
	g_hash_table_insert(satmap, &fexpr_sel_m->satval, fexpr_sel_m);
	
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
	/* add it to satmap */
	g_hash_table_insert(satmap, &fexpr_y->satval, fexpr_y);
	
	sym->fexpr_y = fexpr_y;
	
	struct fexpr *fexpr_m;
	if (sym->type == S_TRISTATE) {
		fexpr_m = fexpr_create(sat_variable_nr++, FE_SYMBOL, sym->name);
		str_append(&fexpr_m->name, "_MODULE");
		fexpr_m->sym = sym;
		fexpr_m->tri = mod;
		/* add it to satmap */
		g_hash_table_insert(satmap, &fexpr_m->satval, fexpr_m);
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

		/* add it to satmap */
		g_hash_table_insert(satmap, &e->satval, e);
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
	/* add it to satmap */
	g_hash_table_insert(satmap, &fexpr_y->satval, fexpr_y);
	
	sym->fexpr_y = fexpr_y;
	
	struct fexpr *fexpr_m;
	if (sym->type == S_TRISTATE) {
		fexpr_m = fexpr_create(sat_variable_nr++, FE_CHOICE, "Choice_");
		str_append(&fexpr_m->name, name);
		str_append(&fexpr_m->name, "_MODULE");
		fexpr_m->sym = sym;
		fexpr_m->tri = mod;
		/* add it to satmap */
		g_hash_table_insert(satmap, &fexpr_m->satval, fexpr_m);
	} else {
		fexpr_m = const_false;
	}
	sym->fexpr_m = fexpr_m;
}


/*
 * calculate, when k_expr will evaluate to yes or mod
 */
struct fexpr * calculate_fexpr_both(struct k_expr *e)
{
	return can_evaluate_to_mod(e) ? fexpr_or(calculate_fexpr_m(e), calculate_fexpr_y(e)) : calculate_fexpr_y(e);
}
struct pexpr * calculate_pexpr_both(struct k_expr *e)
{
	if (!e) return NULL;
	
	if (!can_evaluate_to_mod(e))
		calculate_pexpr_y(e);
	
	switch (e->type) {
	case KE_SYMBOL:
		return pexpr_or(calculate_pexpr_m(e), calculate_pexpr_y(e));
	case KE_AND:
		return calculate_pexpr_both_and(e->left, e->right);
	case KE_OR:
		return calculate_pexpr_both_or(e->left, e->right);
	case KE_NOT:
		return pexpr_or(calculate_pexpr_m(e), calculate_pexpr_y(e));
	case KE_EQUAL:
		return calculate_pexpr_y_equals(e);
	case KE_UNEQUAL:
		return calculate_pexpr_y_unequals(e);
	case KE_CONST_FALSE:
		return pexf(const_false);
	case KE_CONST_TRUE:
		return pexf(const_true);
	}
	
	return NULL;
}


/*
 * calculate, when k_expr will evaluate to yes
 */
struct fexpr * calculate_fexpr_y(struct k_expr *e)
{
	if (!e) return NULL;

	switch (e->type) {
	case KE_SYMBOL:
		return e->sym->fexpr_y;
	case KE_AND:
		return calculate_fexpr_y_and(e->left, e->right);
	case KE_OR:
		return calculate_fexpr_y_or(e->left, e->right);
	case KE_NOT:
		return calculate_fexpr_y_not(e->left);
	case KE_EQUAL:
		return calculate_fexpr_y_equals(e);
	case KE_UNEQUAL:
		return calculate_fexpr_y_unequals(e);
	case KE_CONST_FALSE:
		return const_false;
	case KE_CONST_TRUE:
		return const_true;
	}
	
	return NULL;
}
struct pexpr * calculate_pexpr_y(struct k_expr *e)
{
	if (!e) return NULL;

	switch (e->type) {
	case KE_SYMBOL:
		return pexf(e->sym->fexpr_y);
	case KE_AND:
		return calculate_pexpr_y_and(e->left, e->right);
	case KE_OR:
		return calculate_pexpr_y_or(e->left, e->right);
	case KE_NOT:
		return calculate_pexpr_y_not(e->left);
	case KE_EQUAL:
		return calculate_pexpr_y_equals(e);
	case KE_UNEQUAL:
		return calculate_pexpr_y_unequals(e);
	case KE_CONST_FALSE:
		return pexf(const_false);
	case KE_CONST_TRUE:
		return pexf(const_true);
	}
	
	return NULL;
}

/*
 * calculate, when k_expr will evaluate to mod
 */
struct fexpr * calculate_fexpr_m(struct k_expr *e)
{
	if (!e) return NULL;
	
	if (!can_evaluate_to_mod(e)) return const_false;

	switch (e->type) {
	case KE_SYMBOL:
		return e->sym->fexpr_m;
	case KE_AND:
		return calculate_fexpr_m_and(e->left, e->right);
	case KE_OR:
		return calculate_fexpr_m_or(e->left, e->right);
	case KE_NOT:
		return calculate_fexpr_m_not(e->left);
	default:
		return const_false;
	}
	
	return NULL;
}
struct pexpr * calculate_pexpr_m(struct k_expr *e)
{
	if (!e) return NULL;
	
	if (!can_evaluate_to_mod(e)) return pexf(const_false);

	switch (e->type) {
	case KE_SYMBOL:
		return pexf(e->sym->fexpr_m);
	case KE_AND:
		return calculate_pexpr_m_and(e->left, e->right);
	case KE_OR:
		return calculate_pexpr_m_or(e->left, e->right);
	case KE_NOT:
		return calculate_pexpr_m_not(e->left);
	default:
		return pexf(const_false);
	}
	
	return NULL;
}

/*
 * calculate, when k_expr of type AND will evaluate to yes
 * A && B
 */
struct fexpr * calculate_fexpr_y_and(struct k_expr *a, struct k_expr *b)
{
	return fexpr_and(calculate_fexpr_y(a), calculate_fexpr_y(b));
}
struct pexpr * calculate_pexpr_y_and(struct k_expr *a, struct k_expr *b)
{
	return pexpr_and(calculate_pexpr_y(a), calculate_pexpr_y(b));
}

/*
 * calculate, when k_expr of type AND will evaluate to mod
 * (A || A_m) && (B || B_m) && !(A && B)
 */
struct fexpr * calculate_fexpr_m_and(struct k_expr *a, struct k_expr *b)
{
	struct fexpr *topright = fexpr_not(fexpr_and(calculate_fexpr_y(a), calculate_fexpr_y(b)));
	struct fexpr *ll_left = fexpr_or(calculate_fexpr_y(a), calculate_fexpr_m(a));
	struct fexpr *ll_right = fexpr_or(calculate_fexpr_y(b), calculate_fexpr_m(b));
	struct fexpr *topleft = fexpr_and(ll_left, ll_right);

	return fexpr_and(topleft, topright);
}
struct pexpr * calculate_pexpr_m_and(struct k_expr *a, struct k_expr *b)
{
	struct pexpr *topright = pexpr_not(pexpr_and(calculate_pexpr_y(a), calculate_pexpr_y(b)));
	struct pexpr *ll_left = pexpr_or(calculate_pexpr_y(a), calculate_pexpr_m(a));
	struct pexpr *ll_right = pexpr_or(calculate_pexpr_y(b), calculate_pexpr_m(b));
	struct pexpr *topleft = pexpr_and(ll_left, ll_right);

	return pexpr_and(topleft, topright);
}

/*
 * calculate, when k_expr of type AND will evaluate to mod or yes
 * (A || A_m) && (B || B_m)
 */
struct pexpr * calculate_pexpr_both_and(struct k_expr *a, struct k_expr *b)
{
	struct pexpr *left = pexpr_or(calculate_pexpr_y(a), calculate_pexpr_m(a));
	struct pexpr *right = pexpr_or(calculate_pexpr_y(b), calculate_pexpr_m(b));
	return pexpr_and(left, right);
}

/*
 * calculate, when k_expr of type OR will evaluate to yes
 * A || B
 */
struct fexpr * calculate_fexpr_y_or(struct k_expr *a, struct k_expr *b)
{
	return fexpr_or(calculate_fexpr_y(a), calculate_fexpr_y(b));
}
struct pexpr * calculate_pexpr_y_or(struct k_expr *a, struct k_expr *b)
{
	return pexpr_or(calculate_pexpr_y(a), calculate_pexpr_y(b));
}

/*
 * calculate, when k_expr of type OR will evaluate to mod
 * (A_m || B_m) && !A && !B
 */
struct fexpr * calculate_fexpr_m_or(struct k_expr *a, struct k_expr *b)
{
	struct fexpr *topright = fexpr_not(calculate_fexpr_y(b));
	struct fexpr *lowerleft = fexpr_or(calculate_fexpr_m(a), calculate_fexpr_m(b));
	struct fexpr *topleft = fexpr_and(lowerleft, fexpr_not(calculate_fexpr_y(a)));
	
	return fexpr_and(topleft, topright);
}
struct pexpr * calculate_pexpr_m_or(struct k_expr *a, struct k_expr *b)
{
	struct pexpr *topright = pexpr_not(calculate_pexpr_y(b));
	struct pexpr *lowerleft = pexpr_or(calculate_pexpr_m(a), calculate_pexpr_m(b));
	struct pexpr *topleft = pexpr_and(lowerleft, pexpr_not(calculate_pexpr_y(a)));
	
	return pexpr_and(topleft, topright);
}

/*
 * calculate, when k_expr of type OR will evaluate to mod or yes
 * (A_m || A ||  B_m || B)
 */
struct pexpr * calculate_pexpr_both_or(struct k_expr *a, struct k_expr *b)
{
	struct pexpr *left = pexpr_or(calculate_pexpr_y(a), calculate_pexpr_m(a));
	struct pexpr *right = pexpr_or(calculate_pexpr_y(b), calculate_pexpr_m(b));
	return pexpr_or(left, right);
}

/*
 * calculate, when k_expr of type NOT will evaluate to yes
 * !(A || A_m)
 */
struct fexpr * calculate_fexpr_y_not(struct k_expr *a)
{
	return fexpr_not(fexpr_or(calculate_fexpr_y(a), calculate_fexpr_m(a)));
}
struct pexpr * calculate_pexpr_y_not(struct k_expr *a)
{
	return pexpr_not(pexpr_or(calculate_pexpr_y(a), calculate_pexpr_m(a)));
}

/*
 * calculate, when k_expr of type NOT will evaluate to mod
 * A_m
 */
struct fexpr * calculate_fexpr_m_not(struct k_expr *a)
{
	return calculate_fexpr_m(a);
}
struct pexpr * calculate_pexpr_m_not(struct k_expr *a)
{
	return calculate_pexpr_m(a);
}

static struct fexpr * equiv_fexpr(struct fexpr *a, struct fexpr *b)
{
	struct fexpr *yes = fexpr_and(a, b);
	struct fexpr *not = fexpr_and(fexpr_not(a), fexpr_not(b));
	
	return fexpr_or(yes, not);
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
	
	/* add it to the sat-map */
	g_hash_table_insert(satmap, &e->satval, e);
	
	/* add it to the symbol's list */
	fexpr_list_add(sym->nb_vals, e);
	
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
 * calculate, when k_expr of type EQUAL will evaluate to yes
 * (A=B) && (A_m=B_m)
 */
struct fexpr * calculate_fexpr_y_equals(struct k_expr *a)
{
	/* comparing 2 tristate constants */
	if (is_tristate_constant(a->eqsym) && is_tristate_constant(a->eqvalue))
		return a->eqsym == a->eqvalue ? const_true : const_false;
	
	/* comparing 2 nonboolean constants */
	if (a->eqsym->type == S_UNKNOWN && a->eqvalue->type == S_UNKNOWN) {
		return strcmp(a->eqsym->name, a->eqvalue->name) == 0 ? const_true : const_false;
	}
	
	/* comparing 2 boolean/tristate incl. yes/mod/no constants */
	if (sym_is_bool_or_triconst(a->eqsym) && sym_is_bool_or_triconst(a->eqvalue)) {
		struct fexpr *yes = equiv_fexpr(a->eqsym->fexpr_y, a->eqvalue->fexpr_y);
		struct fexpr *mod = equiv_fexpr(a->eqsym->fexpr_m, a->eqvalue->fexpr_m);
		
		return fexpr_and(yes, mod);
	}
	
	/* comparing nonboolean with a constant */
	if (sym_is_nonboolean(a->eqsym) && a->eqvalue->type == S_UNKNOWN) {
		return sym_get_or_create_nonbool_fexpr(a->eqsym, a->eqvalue->name);
	}
	if (a->eqsym->type == S_UNKNOWN && sym_is_nonboolean(a->eqvalue)) {
		return sym_get_or_create_nonbool_fexpr(a->eqvalue, a->eqsym->name);
	}
	
	/* comparing nonboolean with tristate constant, will never be true */
	if (sym_is_nonboolean(a->eqsym) && is_tristate_constant(a->eqvalue))
		return const_false;
	if (is_tristate_constant(a->eqsym) && sym_is_nonboolean(a->eqvalue))
		return const_false;
	
	/* comparing 2 nonboolean symbols */
	// TODO
	
	/* comparing boolean item with nonboolean constant, will never be true */
	// TODO
	
	return const_false;
}
struct pexpr * calculate_pexpr_y_equals(struct k_expr *a)
{
	/* comparing 2 tristate constants */
	if (is_tristate_constant(a->eqsym) && is_tristate_constant(a->eqvalue))
		return a->eqsym == a->eqvalue ? pexf(const_true) : pexf(const_false);
	
	/* comparing 2 nonboolean constants */
	if (a->eqsym->type == S_UNKNOWN && a->eqvalue->type == S_UNKNOWN) {
		return strcmp(a->eqsym->name, a->eqvalue->name) == 0 ? pexf(const_true) : pexf(const_false);
	}
	
	/* comparing 2 boolean/tristate incl. yes/mod/no constants */
	if (sym_is_bool_or_triconst(a->eqsym) && sym_is_bool_or_triconst(a->eqvalue)) {
		struct pexpr *yes = equiv_pexpr(pexf(a->eqsym->fexpr_y), pexf(a->eqvalue->fexpr_y));
		struct pexpr *mod = equiv_pexpr(pexf(a->eqsym->fexpr_m), pexf(a->eqvalue->fexpr_m));
		
		return pexpr_and(yes, mod);
	}
	
	/* comparing nonboolean with a constant */
	if (sym_is_nonboolean(a->eqsym) && a->eqvalue->type == S_UNKNOWN) {
		return pexf(sym_get_or_create_nonbool_fexpr(a->eqsym, a->eqvalue->name));
	}
	if (a->eqsym->type == S_UNKNOWN && sym_is_nonboolean(a->eqvalue)) {
		return pexf(sym_get_or_create_nonbool_fexpr(a->eqvalue, a->eqsym->name));
	}
	
	/* comparing nonboolean with tristate constant, will never be true */
	if (sym_is_nonboolean(a->eqsym) && is_tristate_constant(a->eqvalue))
		return pexf(const_false);
	if (is_tristate_constant(a->eqsym) && sym_is_nonboolean(a->eqvalue))
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
struct fexpr * calculate_fexpr_y_unequals(struct k_expr *a)
{
	return fexpr_not(calculate_fexpr_y_equals(a));
}
struct pexpr * calculate_pexpr_y_unequals(struct k_expr *a)
{
	return pexpr_not(calculate_pexpr_y_equals(a));
}

/*
 * macro to create a fexpr of type AND
 */
struct fexpr * fexpr_and(struct fexpr *a, struct fexpr *b)
{
	if (a == const_false || b == const_false)
		return const_false;
	
	if (a == const_true)
		return b;
	if (b == const_true)
		return a;
	
	struct fexpr *fe = malloc(sizeof(struct fexpr));
	fe->type = FE_AND;
	fe->left = a;
	fe->right = b;
	
	return fe;
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
 * macro to create a fexpr of type OR
 */
struct fexpr * fexpr_or(struct fexpr *a, struct fexpr *b)
{	
	if (a == const_false)
		return b;
	if (b == const_false)
		return a;
	
	if (a == const_true || b == const_true)
		return const_true;
	
	struct fexpr *fe = malloc(sizeof(struct fexpr));
	fe->type = FE_OR;
	fe->left = a;
	fe->right = b;
	
	return fe;
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
 * macro to create a fexpr of type NOT
 */
struct fexpr * fexpr_not(struct fexpr *a)
{
	if (a == const_false)
		return const_true;
	if (a == const_true)
		return const_false;
	
	/* eliminate double negation */
	if (a->type == FE_NOT)
		return a->left;

	/* De Morgan */
	if (a->type == FE_AND) {
		struct fexpr *fe = malloc(sizeof(struct fexpr));
		fe->type = FE_OR;
		fe->left = fexpr_not(a->left);
		fe->right = fexpr_not(a->right);

		return fe;
	} else if (a->type == FE_OR) {
		struct fexpr *fe = malloc(sizeof(struct fexpr));
		fe->type = FE_AND;
		fe->left = fexpr_not(a->left);
		fe->right = fexpr_not(a->right);

		return fe;
	}
	
	struct fexpr *fe = malloc(sizeof(struct fexpr));
	fe->type = FE_NOT;
	fe->left = a;

	return fe;
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
bool fexpr_is_cnf(struct fexpr *e)
{
	if (!e) return false;
	
	switch (e->type) {
	case FE_SYMBOL:
	case FE_TRUE:
	case FE_FALSE:
	case FE_NONBOOL:
	case FE_SELECT:
	case FE_CHOICE:
	case FE_NPC:
		return true;
	case FE_AND:
		return false;
	case FE_OR:
		return fexpr_is_cnf(e->left) && fexpr_is_cnf(e->right);
	case FE_NOT:
		return fexpr_is_symbol(e->left);
	default:
		perror("Not in CNF, FE_EQUALS.");
		return false;
	}
}
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
struct fexpr * sym_get_fexpr_both(struct symbol *sym)
{
	return sym->type == S_TRISTATE ? fexpr_or(sym->fexpr_m, sym->fexpr_y) : sym->fexpr_y;
}

/*
 * return fexpr_sel_y for a symbol
 */
struct fexpr * sym_get_fexpr_sel_y(struct symbol *sym)
{
	if (sym->fexpr_sel_y == NULL)
		return const_false;
	
	return sym->fexpr_sel_y;
}

/*
 * return fexpr_sel_m for a symbol
 */
struct fexpr * sym_get_fexpr_sel_m(struct symbol *sym)
{
	if (sym->fexpr_sel_m == NULL || sym->type != S_TRISTATE)
		return const_false;
	
	return sym->fexpr_sel_m;
}

/*
 * return fexpr_sel_both for a symbol
 */
struct fexpr * sym_get_fexpr_sel_both(struct symbol *sym)
{
	if (!sym->rev_dep.expr)
		return const_false;
	
	return sym->type == S_TRISTATE ? fexpr_or(sym->fexpr_sel_m, sym->fexpr_sel_y) : sym->fexpr_sel_y;
}

/*
 * macro to construct a fexpr for "A implies B"
 */
struct fexpr * implies_fexpr(struct fexpr *a, struct fexpr *b)
{
	return fexpr_or(fexpr_not(a), b);
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
 * check, if the fexpr is a symbol, a True/False-constant, a literal symbolizing a non-boolean, a choice symbol or NOT
 */
bool fexpr_is_symbol_or_not(struct fexpr *e)
{
	return fexpr_is_symbol(e) || e->type == FE_NOT;
}

/*
 * check, if a fexpr is a symbol, a True/False-constant, a literal symbolizing a non-boolean, a choice symbol or a negated "symbol"
 */
bool fexpr_is_symbol_or_neg_atom(struct fexpr *e)
{
	return fexpr_is_symbol(e) || (e->type == FE_NOT && fexpr_is_symbol(e->left));
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
 * convert a fexpr into negation normal form
 */
// static bool convert_fexpr_to_nnf_util(struct fexpr *e)
// {
// 	if (!e) return false;
// 	
// 	switch (e->type) {
// 	case FE_SYMBOL:
// 	case FE_CHOICE:
// 	case FE_FALSE:
// 	case FE_TRUE:
// 	case FE_SELECT:
// 	case FE_TMPSATVAR:
// 		return false;
// 	case FE_AND:
// 	case FE_OR:
// 		return convert_fexpr_to_nnf_util(e->left) || convert_fexpr_to_nnf_util(e->right);
// 	case FE_NOT:
// 		/* De Morgan */
// 		if (e->left->type == FE_AND) {
// 			struct fexpr *left = malloc(sizeof(struct fexpr));
// 			left->type = FE_NOT;
// 			left->left = e->left->left;
// 			struct fexpr *right = malloc(sizeof(struct fexpr));
// 			right->type = FE_NOT;
// 			right->left = e->left->right;
// 			
// 			e->type = FE_OR;
// 			e->left = left;
// 			e->right = right;
// 			
// 			return true;
// 		} else if (e->left->type == FE_OR) {
// 			struct fexpr *left = malloc(sizeof(struct fexpr));
// 			left->type = FE_NOT;
// 			left->left = e->left->left;
// 			struct fexpr *right = malloc(sizeof(struct fexpr));
// 			right->type = FE_NOT;
// 			right->left = e->left->right;
// 			
// 			e->type = FE_AND;
// 			e->left = left;
// 			e->right = right;
// 			
// 			return true;
// 		} 
// 		/* double negation */
// 		else if (e->left->type == FE_NOT) {
// 			if (e->left->left->type == FE_SYMBOL || e->left->left->type == FE_CHOICE) {
// 				e->type = e->left->left->type;
// 				e->name = str_new();
// 				str_append(&e->name, str_get(&e->left->left->name));
// 				e->satval = e->left->left->satval;
// 				e->sym = e->left->left->sym;
// 				if (e->type == FE_SYMBOL)
// 					e->tri = e->left->left->tri;
// 				return true;
// 			} else if (e->left->left->type == FE_AND || e->left->left->type == FE_OR) {
// 				e->type = e->left->left->type;
// 				e->right = e->left->left->right;
// 				e->left = e->left->left->left;
// 				return true;
// 			} else if (e->left->left->type == FE_EQUALS) {
// 				e->type = e->left->left->type;
// 				e->eqsym = e->left->left->eqsym;
// 				e->eqvalue = e->left->left->eqvalue;
// 				return true;
// 			}
// 			return false;
// 		}
// 		return false;
// 	case FE_EQUALS:
// 	case FE_NONBOOL:
// 		return false;
// 	}
// 	return false;
// }
// void convert_fexpr_to_nnf(struct fexpr *e) {
// 	while (convert_fexpr_to_nnf_util(e))
// 		;
// }

/* 
 * convert a fexpr from negation normal form into conjunctive normal form
 */
// static bool convert_nnf_to_cnf_util(struct fexpr *e)
// {
// 	switch (e->type) {
// 	case FE_SYMBOL:
// 	case FE_CHOICE:
// 	case FE_FALSE:
// 	case FE_TRUE:
// 	case FE_SELECT:
// 	case FE_TMPSATVAR:
// 		return false;
// 	case FE_AND:
// 		return convert_nnf_to_cnf_util(e->left) || convert_nnf_to_cnf_util(e->right);
// 	case FE_OR:
// 		if (e->left->type == FE_AND) {
// 			e->type = FE_AND;
// 			struct fexpr *fe_left = malloc(sizeof(struct fexpr));
// 			struct fexpr *fe_right = malloc(sizeof(struct fexpr));
// 			
// 			fe_left->type = FE_OR;
// 			fe_left->left = e->left->left;
// 			fe_left->right = e->right;
// 	
// 			fe_right->type = FE_OR;
// 			fe_right->left = e->left->right;
// 			fe_right->right = e->right;
// 			
// 			e->left = fe_left;
// 			e->right = fe_right;
// 			
// 			return true;
// 		}
// 		else if (e->right->type == FE_AND) {
// 			e->type = FE_AND;
// 			struct fexpr *fe_left = malloc(sizeof(struct fexpr));
// 			struct fexpr *fe_right = malloc(sizeof(struct fexpr));
// 			
// 			fe_left->type = FE_OR;
// 			fe_left->left = e->left;
// 			fe_left->right = e->right->left;
// 	
// 			fe_right->type = FE_OR;
// 			fe_right->left = e->left;
// 			fe_right->right = e->right->right;
// 			
// 			e->left = fe_left;
// 			e->right = fe_right;
// 			
// 			return true;
// 		}
// 		return convert_nnf_to_cnf_util(e->left) || convert_nnf_to_cnf_util(e->right);
// 	case FE_NOT:
// 	case FE_EQUALS:
// 	case FE_NONBOOL:
// 		return false;
// 	}
// 	return false;
// }
// void convert_fexpr_to_cnf(struct fexpr *e) {
// 	while (convert_nnf_to_cnf_util(e))
// 		;
// }

/*
 * print a fexpr
 */
static void fexpr_print_util(struct fexpr *e, int parent)
{
	if (!e) return;
	
	switch (e->type) {
	case FE_SYMBOL:
	case FE_CHOICE:
	case FE_SELECT:
	case FE_NPC:
	case FE_NONBOOL:
	case FE_TMPSATVAR:
		printf("%s", str_get(&e->name));
		break;
	case FE_AND:
		if (parent != FE_AND && parent != -1)
			printf("(");
		fexpr_print_util(e->left, FE_AND);
		printf(" && ");
		fexpr_print_util(e->right, FE_AND);
		if (parent != FE_AND && parent != -1)
			printf(")");
		break;
	case FE_OR:
		if (parent != FE_OR && parent != -1)
			printf("(");
		fexpr_print_util(e->left, FE_OR);
		printf(" || ");
		fexpr_print_util(e->right, FE_OR);
		if (parent != FE_OR && parent != -1)
			printf(")");
		break;
	case FE_NOT:
		printf("!");
		fexpr_print_util(e->left, FE_NOT);
		break;
	case FE_EQUALS:
		printf("%s=%s", e->sym->name, e->eqvalue->name);
		break;
	case FE_FALSE:
		printf("0");
		break;
	case FE_TRUE:
		printf("1");
		break;
	}
}
void fexpr_print(char *tag, struct fexpr *e, int parent)
{
	printf("%s ", tag);
	fexpr_print_util(e, parent);
	printf("\n");
}

/*
 * write an fexpr into a string (format needed for testing)
 */
void fexpr_as_char(struct fexpr *e, struct gstr *s, int parent)
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
	case FE_AND:
		/* need this hack for the FeatureExpr parser */
		if (parent != FE_AND)
			str_append(s, "(");
		fexpr_as_char(e->left, s, FE_AND);
		str_append(s, " && ");
		fexpr_as_char(e->right, s, FE_AND);
		if (parent != FE_AND)
			str_append(s, ")");
		return;
	case FE_OR:
		if (parent != FE_OR)
			str_append(s, "(");
		fexpr_as_char(e->left, s, FE_OR);
		str_append(s, " || ");
		fexpr_as_char(e->right, s, FE_OR);
		if (parent != FE_OR)
			str_append(s, ")");
		return;
	case FE_NOT:
		str_append(s, "!");
		fexpr_as_char(e->left, s, FE_NOT);
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
 * write an fexpr into a string
 */
void fexpr_as_char_short(struct fexpr *e, struct gstr *s, int parent)
{
	if (!e) return;
	
	switch (e->type) {
	case FE_SYMBOL:
	case FE_CHOICE:
	case FE_SELECT:
	case FE_NPC:
	case FE_NONBOOL:
		str_append(s, str_get(&e->name));
		return;
	case FE_AND:
		/* need this hack for the FeatureExpr parser */
		if (parent != FE_AND)
			str_append(s, "(");
		fexpr_as_char_short(e->left, s, FE_AND);
		str_append(s, " && ");
		fexpr_as_char_short(e->right, s, FE_AND);
		if (parent != FE_AND)
			str_append(s, ")");
		return;
	case FE_OR:
		if (parent != FE_OR)
			str_append(s, "(");
		fexpr_as_char_short(e->left, s, FE_OR);
		str_append(s, " || ");
		fexpr_as_char_short(e->right, s, FE_OR);
		if (parent != FE_OR)
			str_append(s, ")");
		return;
	case FE_NOT:
		str_append(s, "!");
		fexpr_as_char_short(e->left, s, FE_NOT);
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
	case FE_AND:
		/* need this hack for the FeatureExpr parser */
		if (parent != PE_AND)
			str_append(s, "(");
		pexpr_as_char_short(e->left.pexpr, s, PE_AND);
		str_append(s, " && ");
		pexpr_as_char_short(e->right.pexpr, s, PE_AND);
		if (parent != PE_AND)
			str_append(s, ")");
		return;
	case FE_OR:
		if (parent != PE_OR)
			str_append(s, "(");
		pexpr_as_char_short(e->left.pexpr, s, PE_OR);
		str_append(s, " || ");
		pexpr_as_char_short(e->right.pexpr, s, PE_OR);
		if (parent != PE_OR)
			str_append(s, ")");
		return;
	case FE_NOT:
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
 * make a shallow copy of a fexpr_list 
 */
struct fexpr_list * fexpr_list_copy(struct fexpr_list *list)
{
	struct fexpr_list *ret = fexpr_list_init();
	struct fexpr_node *node;
	fexpr_list_for_each(node, list)
		fexpr_list_add(list, node->elem);
	
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
		fexl_list_add(list, node->elem);
	
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
	printf("%s ", tag);
	pexpr_print_util(e, prevtoken);
	printf("\n");
}

/*
 * convert a fexpr to a pexpr
 */
struct pexpr * pexf(struct fexpr *fe)
{
	struct pexpr *pe;
	
	switch (fe->type) {
	case FE_SYMBOL:
	case FE_TRUE:
	case FE_FALSE:
	case FE_NONBOOL:
	case FE_CHOICE:
	case FE_SELECT:
	case FE_NPC:
	case FE_TMPSATVAR:
		pe = xcalloc(1, sizeof(*pe));
		pe->type = PE_SYMBOL;
		pe->left.fexpr = fe;
		return pe;
	case FE_AND:
		pe = xcalloc(1, sizeof(*pe));
		pe->type = PE_AND;
		pe->left.pexpr = pexf(fe->left);
		pe->right.pexpr = pexf(fe->right);
		return pe;
	case FE_OR:
		pe = xcalloc(1, sizeof(*pe));
		pe->type = PE_OR;
		pe->left.pexpr = pexf(fe->left);
		pe->right.pexpr = pexf(fe->right);
		return pe;
	case FE_NOT:
		pe = xcalloc(1, sizeof(*pe));
		pe->type = PE_NOT;
		pe->left.pexpr = pexf(fe->left);
		return pe;
	case FE_EQUALS:
		perror("Should not happen.");
		break;
	}
	
	return NULL;
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

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

#define SATMAP_INIT_SIZE 2

static struct k_expr * gcc_version_eval(struct expr *e);
static struct k_expr * expr_eval_unequal_bool(struct expr *e);

static void print_expr_util(struct expr *e, int prevtoken);

/*
 * parse Kconfig-file and read .config
 */
void init_config(const char *Kconfig_file)
{
	conf_parse(Kconfig_file);
	conf_read(NULL);
}

/*
 * initialize satmap and cnf_clauses_map
 */
void init_data(void)
{
	/* create hashtable with all fexpr */
	satmap = xcalloc(SATMAP_INIT_SIZE, sizeof(*satmap));
	satmap_size = SATMAP_INIT_SIZE;
	
	printf("done.\n");
}

/*
 * bool-symbols have 1 variable (X), tristate-symbols have 2 variables (X, X_m)
 */
static void create_sat_variables(struct symbol *sym)
{
	sym->constraints = pexpr_list_init();
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
	
	for_all_symbols(i, sym)
		create_sat_variables(sym);
	
	printf("done.\n");
}

/*
 * create True/False constants
 */
void create_constants(void)
{
	printf("Creating constants...");
	
	/* create TRUE and FALSE constants */
	const_false = fexpr_create(sat_variable_nr++, FE_FALSE, "0");
	fexpr_add_to_satmap(const_false);

	const_true = fexpr_create(sat_variable_nr++, FE_TRUE, "1");
	fexpr_add_to_satmap(const_true);
	
	/* add fexpr of constants to tristate constants */
	symbol_yes.fexpr_y = const_true;
	symbol_yes.fexpr_m = const_false;
	
	symbol_mod.fexpr_y = const_false;
	symbol_mod.fexpr_m = const_true;
	
	symbol_no.fexpr_y = const_false;
	symbol_no.fexpr_m = const_false;
	
	/* create symbols yes/mod/no as fexpr */
	symbol_yes_fexpr = fexpr_create(0, FE_SYMBOL, "y");
	symbol_yes_fexpr->sym = &symbol_yes;
	symbol_yes_fexpr->tri = yes;
	
	symbol_mod_fexpr = fexpr_create(0, FE_SYMBOL, "m");
	symbol_mod_fexpr->sym = &symbol_mod;
	symbol_mod_fexpr->tri = mod;
	
	symbol_no_fexpr = fexpr_create(0, FE_SYMBOL, "n");
	symbol_no_fexpr->sym = &symbol_no;
	symbol_no_fexpr->tri = no;
	
	printf("done.\n");
}

/*
 * create a temporary SAT-variable
 */
struct fexpr * create_tmpsatvar(void)
{
	struct fexpr *t = fexpr_create(sat_variable_nr++, FE_TMPSATVAR, "");
	str_append(&t->name, get_tmp_var_as_char(tmp_variable_nr++));
	fexpr_add_to_satmap(t);
	
	return t;
}

/*
 * return a temporary SAT variable as string
 */
char * get_tmp_var_as_char(int i)
{
	char *val = malloc(sizeof(char) * 18);
	snprintf(val, 18,"T_%d", i);
	return val;
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
 * check if a k_expr can evaluate to mod
 */
bool can_evaluate_to_mod(struct k_expr *e)
{
	if (!e) return false;
	
	switch (e->type) {
	case KE_SYMBOL:
		return e->sym == &symbol_mod || e->sym->type == S_TRISTATE ? true : false;
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
 * evaluate an unequality with GCC_VERSION
 */
static struct k_expr * gcc_version_eval(struct expr* e)
{
	if (!e) get_const_false_as_kexpr();
	
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
		return actual_gcc_ver < sym_gcc_ver ? get_const_true_as_kexpr() : get_const_false_as_kexpr();
	case E_LEQ:
		return actual_gcc_ver <= sym_gcc_ver ? get_const_true_as_kexpr() : get_const_false_as_kexpr();
	case E_GTH:
		return actual_gcc_ver > sym_gcc_ver ? get_const_true_as_kexpr() : get_const_false_as_kexpr();
	case E_GEQ:
		return actual_gcc_ver >= sym_gcc_ver ? get_const_true_as_kexpr() : get_const_false_as_kexpr();
	default:
		perror("Wrong type in gcc_version_eval.");
	}
	
	return get_const_false_as_kexpr();
}

/*
 * evaluate an unequality with 2 boolean symbols
 */
static struct k_expr * expr_eval_unequal_bool(struct expr *e)
{
	if (!e) get_const_false_as_kexpr();
	
	assert(sym_is_boolean(e->left.sym));
	assert(sym_is_boolean(e->right.sym));
	
	int val_left = sym_get_tristate_value(e->left.sym);
	int val_right = sym_get_tristate_value(e->right.sym);
	
	switch (e->type) {
	case E_LTH:
		return val_left < val_right ? get_const_true_as_kexpr() : get_const_false_as_kexpr();
	case E_LEQ:
		return val_left <= val_right ? get_const_true_as_kexpr() : get_const_false_as_kexpr();
	case E_GTH:
		return val_left > val_right ? get_const_true_as_kexpr() : get_const_false_as_kexpr();
	case E_GEQ:
		return val_left >= val_right ? get_const_true_as_kexpr() : get_const_false_as_kexpr();
	default:
		perror("Wrong type in expr_eval_unequal_bool.");
	}
	
	return get_const_false_as_kexpr();
}


/*
 * parse an expr as a k_expr
 */
struct k_expr * parse_expr(struct expr *e, struct k_expr *parent)
{
	struct k_expr *ke = malloc(sizeof(struct k_expr));
	ke->parent = parent;
// 	print_expr("expr:", e, E_NONE);
// 	printf("type: %d\n", e->type);

	switch (e->type) {
	case E_SYMBOL:
		ke->type = KE_SYMBOL;
		ke->sym = e->left.sym;
		ke->tri = no;
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
	case E_LTH:
	case E_LEQ:
	case E_GTH:
	case E_GEQ:
		// TODO
// 		print_expr("UNEQUAL:", e, 0);
		
		/* "special" hack for GCC_VERSION */
		if (expr_contains_symbol(e, sym_find("GCC_VERSION")))
			return gcc_version_eval(e);
		
		/* "special" hack for CRAMFS <= MTD */
		if (expr_contains_symbol(e, sym_find("CRAMFS")) && expr_contains_symbol(e, sym_find("MTD")))
			return expr_eval_unequal_bool(e);
		
		return get_const_false_as_kexpr();
	default:
		return NULL;
	}
}

/*
 * print an expr
 */
static void print_expr_util(struct expr *e, int prevtoken)
{
	if (!e) return;

	switch (e->type) {
	case E_SYMBOL:
		if (sym_get_name(e->left.sym) != NULL)
			printf("%s", sym_get_name(e->left.sym));
		else
			printf("left was null\n");
		break;
	case E_NOT:
		printf("!");
		print_expr_util(e->left.expr, E_NOT);
		break;
	case E_AND:
		if (prevtoken != E_AND && prevtoken != 0)
			printf("(");
		print_expr_util(e->left.expr, E_AND);
		printf(" && ");
		print_expr_util(e->right.expr, E_AND);
		if (prevtoken != E_AND && prevtoken != 0)
			printf(")");
		break;
	case E_OR:
		if (prevtoken != E_OR && prevtoken != 0)
			printf("(");
		print_expr_util(e->left.expr, E_OR);
		printf(" || ");
		print_expr_util(e->right.expr, E_OR);
		if (prevtoken != E_OR && prevtoken != 0)
			printf(")");
		break;
	case E_EQUAL:
	case E_UNEQUAL:
		if (e->left.sym->name)
			printf("%s", e->left.sym->name);
		else
			printf("left was null\n");
		printf("%s", e->type == E_EQUAL ? "=" : "!=");
		printf("%s", e->right.sym->name);
		break;
	case E_LEQ:
	case E_LTH:
		if (e->left.sym->name)
			printf("%s", e->left.sym->name);
		else
			printf("left was null\n");
		printf("%s", e->type == E_LEQ ? "<=" : "<");
		printf("%s", e->right.sym->name);
		break;
	case E_GEQ:
	case E_GTH:
		if (e->left.sym->name)
			printf("%s", e->left.sym->name);
		else
			printf("left was null\n");
		printf("%s", e->type == E_GEQ ? ">=" : ">");
		printf("%s", e->right.sym->name);
		break;
	case E_RANGE:
		printf("[");
		printf("%s", e->left.sym->name);
		printf(" ");
		printf("%s", e->right.sym->name);
		printf("]");
		break;
	default:
		break;
	}
}
void print_expr(char *tag, struct expr *e, int prevtoken)
{
	printf("%s ", tag);
	print_expr_util(e, prevtoken);
	printf("\n");
}


/*
 * print a kexpr
 */
static void print_kexpr_util(struct k_expr *e)
{
	if (!e) return;

	switch (e->type) {
	case KE_SYMBOL:
		printf("%s", sym_get_name(e->sym));
		if (e->tri == mod)
			printf("_m");
		break;
	case KE_AND:
		printf("(");
		print_kexpr_util(e->left);
		printf(" && ");
		print_kexpr_util(e->right);
		printf(")");
		break;
	case KE_OR:
		printf("(");
		print_kexpr_util(e->left);
		printf(" || ");
		print_kexpr_util(e->right);
		printf(")");
		break;
	case KE_NOT:
		printf("!");
		print_kexpr_util(e->left);
		break;
	case KE_EQUAL:
	case KE_UNEQUAL:
		printf("%s", e->eqsym->name);
		printf("%s", e->type == KE_EQUAL ? "=" : "!=");
		printf("%s", e->eqvalue->name);
		break;
	case KE_CONST_FALSE:
		printf("0");
		break;
	case KE_CONST_TRUE:
		printf("1");
		break;
	}
}
void print_kexpr(char *tag, struct k_expr *e)
{
	printf("%s ", tag);
	print_kexpr_util(e);
	printf("\n");
}

/*
 * print some debug info about the tree structure of k_expr
 */
void debug_print_kexpr(struct k_expr *e)
{
	if (!e) return;
	
	#if DEBUG_KEXPR
		printf("e-type: %s", kexpr_type[e->type]);
		if (e->parent)
			printf(", parent %s", kexpr_type[e->parent->type]);
		printf("\n");
		switch (e->type) {
		case KE_SYMBOL:
			printf("name %s\n", e->sym->name);
			break;
		case KE_AND:
		case KE_OR:
			printf("left child: %s\n", kexpr_type[e->left->type]);
			printf("right child: %s\n", kexpr_type[e->right->type]);
			debug_print_kexpr(e->left);
			debug_print_kexpr(e->right);
			break;
		case KE_NOT:
		default:
			printf("child: %s\n", kexpr_type[e->left->type]);
			debug_print_kexpr(e->left);
			break;
		}
		
	#endif
}

/*
 * write a kexpr into a string
 */
void kexpr_as_char(struct k_expr *e, struct gstr *s)
{
	if (!e) return;
	
	switch (e->type) {
	case KE_SYMBOL:
		str_append(s, e->sym->name);
		return;
	case KE_AND:
		str_append(s, "(");
		kexpr_as_char(e->left, s);
		str_append(s, " && ");
		kexpr_as_char(e->right, s);
		str_append(s, ")");
		return;
	case KE_OR:
		str_append(s, "(");
		kexpr_as_char(e->left, s);
		str_append(s, " || ");
		kexpr_as_char(e->right, s);
		str_append(s, ")");
		return;
	case KE_NOT:
		str_append(s, "!");
		kexpr_as_char(e->left, s);
		return;
	case KE_EQUAL:
	case KE_UNEQUAL:
		str_append(s, e->eqsym->name);
		str_append(s, e->type == KE_EQUAL ? "=" : "!=");
		str_append(s, e->eqvalue->name);
		break;
	case KE_CONST_FALSE:
		str_append(s, "0");
		break;
	case KE_CONST_TRUE:
		str_append(s, "1");
		break;
	}
}

/*
 * check, if the symbol is a tristate-constant
 */
bool is_tristate_constant(struct symbol *sym) {
	return sym == &symbol_yes || sym == &symbol_mod || sym == &symbol_no;
}

/*
 * check, if a symbol is of type boolean or tristate
 */
bool sym_is_boolean(struct symbol *sym)
{
	return sym->type == S_BOOLEAN || sym->type == S_TRISTATE;
}

/*
 * check, if a symbol is a boolean/tristate or a tristate constant
 */
bool sym_is_bool_or_triconst(struct symbol *sym)
{
	return is_tristate_constant(sym) || sym_is_boolean(sym);
}

/*
 * check, if a symbol is of type int, hex, or string
 */
bool sym_is_nonboolean(struct symbol *sym)
{
	return sym->type == S_INT || sym->type == S_HEX || sym->type == S_STRING;
}

/*
 * check, if a symbol has a prompt
 */
bool sym_has_prompt(struct symbol *sym)
{
	struct property *prop;

	for_all_prompts(sym, prop)
		return true;

	return false;
}

/*
 * return the prompt of the symbol if there is one, NULL otherwise
 */
struct property * sym_get_prompt(struct symbol *sym)
{
	struct property *prop;
	
	for_all_prompts(sym, prop)
		return prop;
	
	return NULL;
}

/*
 * return the condition for the property, True if there is none
 */
struct pexpr * prop_get_condition(struct property *prop)
{
	assert(prop != NULL);
	
	/* if there is no condition, return True */
	if (!prop->visible.expr)
		return pexf(const_true);
	
	struct k_expr *ke = parse_expr(prop->visible.expr, NULL);
	
	return calculate_pexpr_both(ke);
}

/*
 * return the name of the symbol or the prompt-text, if it is a choice symbol
 */
char * sym_get_name(struct symbol *sym)
{
	if (sym_is_choice(sym)) {
		struct property *prompt = sym_get_prompt(sym);
		assert(prompt);
		return strdup(prompt->text);
	} else {
		return sym->name;
	}
}

/* 
 * check whether symbol is to be changed 
 */
bool sym_is_sdv(struct sdv_list *list, struct symbol *sym)
{
	struct sdv_node *node;
	sdv_list_for_each(node, list)
		if (sym == node->elem->sym)
			return true;
	
	return false;
}

/*
 * print a symbol's name
 */
void print_sym_name(struct symbol *sym)
{
	printf("Symbol: ");
	if (sym_is_choice(sym)) {
		struct property *prompt = sym_get_prompt(sym);
		printf("(Choice) %s", prompt->text);
	} else  {
		printf("%s", sym->name);
	}
	printf("\n");
}

/*
 * print all constraints for a symbol
 */
void print_sym_constraint(struct symbol* sym)
{
	struct pexpr_node *node;
	pexpr_list_for_each(node, sym->constraints)
		pexpr_print("::", node->elem, -1);
}

/*
 * print a default map
 */
void print_default_map(struct defm_list *map)
{
	struct default_map *entry;
	struct defm_node *node;
	
	defm_list_for_each(node, map) {
		entry = node->elem;
		struct gstr s = str_new();
		str_append(&s, "\t");
		str_append(&s, str_get(&entry->val->name));
		str_append(&s, " ->");
		pexpr_print(strdup(str_get(&s)), entry->e, -1);
		str_free(&s);
	}
}

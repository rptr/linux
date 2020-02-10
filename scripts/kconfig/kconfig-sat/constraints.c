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

#include "satconf.h"

static void build_tristate_constraint_clause(struct symbol *sym);
static void add_selects(struct symbol *sym);
static void add_dependencies(struct symbol* sym);
static void add_choice_prompt_cond(struct symbol *sym);
static void add_choice_constraints(struct symbol *sym);
static void sym_nonbool_at_least_1(struct symbol *sym);
static void sym_nonbool_at_most_1(struct symbol *sym);
static void sym_get_range_constraints(struct symbol *sym);

static struct default_map * default_map_create_entry(char *val, struct fexpr *e);
static struct property *sym_get_default_prop(struct symbol *sym);
static long long sym_get_range_val(struct symbol *sym, int base);

/* -------------------------------------- */

/*
 * build the constraints for each symbol
 */
void get_constraints(void)
{
	unsigned int i;
	struct symbol *sym;
	struct property *p, *p2;
	
	printf("Building constraints...");
	
	for_all_symbols(i, sym) {
		if (!sym_is_boolean(sym) && !sym_is_nonboolean(sym))
			continue;
		
		/* build tristate constraints */
		if (sym->type == S_TRISTATE)
			build_tristate_constraint_clause(sym);
		
// 		if (sym->rev_dep.expr)
// 			print_expr("rev_dep:", sym->rev_dep.expr, E_NONE);
// 		
// 		if (sym->dir_dep.expr)
// 			print_expr("dir_dep:", sym->dir_dep.expr, E_NONE);
		
		/* build constraints for select statements */
		if (sym->rev_dep.expr)
			add_selects(sym);
		
		/* build constraints for dependencies */
		if (sym->dir_dep.expr)
			add_dependencies(sym);
		
		/* build constraints for choice prompts */
		if (sym_is_choice(sym))
			add_choice_prompt_cond(sym);
		
		/* build constraints for the choice options */
		if (sym_is_choice(sym))
			add_choice_constraints(sym);
		
// 		print_sym_constraint(sym);
		
// 		GArray *defaults = g_array_new(false, false, sizeof(struct default_map));
// 		
// 		
// 		for_all_properties(sym, p, P_PROMPT) {
// 			if (!p->visible.expr) continue;
// 			
// 			printf("sym %s, prompt %s\n", sym->name, p->text);
// 			printf("Prompt condition: ");
// 			
// 			struct k_expr *ke = parse_expr(p->visible.expr, NULL);
// 			struct fexpr *nopromptCond = fexpr_not(calculate_fexpr_both(ke));
// 			
// 			/* get all default prompts */
// 			for_all_defaults(sym, p2) {
// 				
// 			}
// 
// 			
// 			print_fexpr(nopromptCond, -1);
// 			printf("\n");
// 			
// 			
// 			
// 			printf("promptCondition.fexpr_both:\n");
// 		}

	}
	
	/* build constraints for non-booleans */
	for_all_symbols(i, sym) {
		if (!sym_is_nonboolean(sym)) continue;
		
		/* get the range constraints for int/hex */
		if (sym->type == S_INT || sym->type == S_HEX)
			sym_get_range_constraints(sym);
		
		/* non-boolean symbols can have at most one of their symbols to be true */
		sym_nonbool_at_least_1(sym);
		sym_nonbool_at_most_1(sym);
	}
	
	printf("done.\n");
// 	printf("count: %d\n", count);
}

/*
 * enforce tristate constraints
 * X and X_MODULE cannot both be true
 * also, the MODULES symbol must be set to yes/mod for tristates to be allowed 
 */
static void build_tristate_constraint_clause(struct symbol *sym)
{
	assert(sym->type == S_TRISTATE);
	char reason[CNF_REASON_LENGTH];
	strcpy(reason, "(#): enforce tristate constraints for symbol ");
	if (sym->name)
		strcat(reason, sym->name);

	struct fexpr *c = fexpr_or(fexpr_not(sym->fexpr_y), fexpr_not(sym->fexpr_m));
	sym_add_constraint(sym, c);

	/* enforce MODULES constraint */
	// TODO doublecheck that
	if (modules_sym->fexpr_y != NULL) {
		struct fexpr *c2 = fexpr_or(fexpr_not(sym->fexpr_m), modules_sym->fexpr_y);
		sym_add_constraint(sym, c2);
	}
}

/*
 * build the select constraints
 */
static void add_selects(struct symbol *sym)
{
	/* parse reverse dependency */
	struct k_expr *ke = parse_expr(sym->rev_dep.expr, NULL);

	struct fexpr *fe_y = calculate_fexpr_y(ke);
	struct fexpr *sel_y = implies(fe_y, sym->fexpr_y);
	sym_add_constraint(sym, sel_y);

	struct fexpr *fe_both = calculate_fexpr_both(ke);
	struct fexpr *sel_both = implies(fe_both, sym_get_fexpr_both(sym));
	sym_add_constraint(sym, sel_both);
}

/*
 * build the dependency constraints
 */
static void add_dependencies(struct symbol *sym)
{
	assert(sym->dir_dep.expr);
	
// 	print_sym_name(sym);
// 	print_expr("dir_dep expr:", sym->dir_dep.expr, E_NONE);
	
	struct k_expr *ke_dirdep = parse_expr(sym->dir_dep.expr, NULL);
	struct k_expr *ke_revdep = sym->rev_dep.expr ? parse_expr(sym->rev_dep.expr, NULL) : get_const_false_as_kexpr();
	
// 	print_kexpr("kexpr:", ke_dirdep);

	struct gstr reason = str_new();
	str_append(&reason, "(#): ");
	str_append(&reason, sym->name);
	str_append(&reason, " depends on ");
	// TODO ke_dirdep | ke_revdep
	kexpr_as_char(ke_dirdep, &reason);
	
	struct fexpr *dep_both = calculate_fexpr_both(ke_dirdep);
	struct fexpr *sel_both = sym->rev_dep.expr ? calculate_fexpr_both(ke_revdep) : const_false;

// 	print_fexpr("fexpr dep_both:", dep_both, -1);
// 	print_fexpr("fexpr sel_both:", sel_both, -1);
	
	if (sym->type == S_TRISTATE) {
		struct fexpr *dep_y = calculate_fexpr_y(ke_dirdep);
		struct fexpr *sel_y = calculate_fexpr_y(ke_revdep);
		struct fexpr *fe_y = implies(sym->fexpr_y, fexpr_or(dep_y, sel_y));
		sym_add_constraint(sym, fe_y);
		
		struct fexpr *fe_both = implies(sym->fexpr_m, fexpr_or(dep_both, sel_both));
		sym_add_constraint(sym, fe_both);
	} else if (sym->type == S_BOOLEAN) {
		struct fexpr *fe_both = implies(sym->fexpr_y, fexpr_or(dep_both, sel_both));
		sym_add_constraint(sym, fe_both);
	} else if (sym_is_nonboolean(sym)) {
		char int_values[][2] = {"0", "1"};
		char hex_values[][4] = {"0x0", "0x1"};
		char string_values[][9] = {"", "nonempty"};
		struct fexpr *e1, *e2;
		
		switch (sym->type) {
		case S_INT:
			e1 = sym_get_or_create_nonbool_fexpr(sym, int_values[0]);
			e2 = sym_get_or_create_nonbool_fexpr(sym, int_values[1]);
			break;
		case S_HEX:
			e1 = sym_get_or_create_nonbool_fexpr(sym, hex_values[0]);
			e2 = sym_get_or_create_nonbool_fexpr(sym, hex_values[1]);
			break;
		case S_STRING:
			e1 = sym_get_or_create_nonbool_fexpr(sym, string_values[0]);
			e2 = sym_get_or_create_nonbool_fexpr(sym, string_values[1]);
			break;
		default:
			e1 = const_true;
			e2 = const_true;
			break;
		}
		
		// TODO
		struct fexpr *fe_both = implies(fexpr_or(e1, e2), fexpr_or(dep_both, sel_both));
		sym_add_constraint(sym, fe_both);
	}
	//printf("\n");
}

/*
 * build the constraints for the choice prompt
 */
static void add_choice_prompt_cond(struct symbol* sym)
{
	assert(sym_is_boolean(sym));
	
	struct property *prompt = sym_get_prompt(sym);
	assert(prompt);
	
	struct k_expr *ke = prompt->visible.expr ? parse_expr(prompt->visible.expr, NULL) : get_const_true_as_kexpr();
	struct fexpr *promptCondition = calculate_fexpr_both(ke);
	
	struct fexpr *fe_both = sym_get_fexpr_both(sym);
	
	if (!sym_is_optional(sym)) {
		struct fexpr *req_cond = implies(promptCondition, fe_both);
		sym_add_constraint(sym, req_cond);
	}
	
	struct fexpr *pr_cond = implies(fe_both, promptCondition);
	sym_add_constraint(sym, pr_cond);
}

/*
 * build constraints for the choice symbols
 */
static void add_choice_constraints(struct symbol *sym)
{
	assert(sym_is_boolean(sym));
	
	struct property *prompt = sym_get_prompt(sym);
	assert(prompt);
	
	unsigned int i, j;
	struct symbol *choice, *choice2;
	
	/* create list of all choice options */
	GArray *items = g_array_new(false, false, sizeof(struct symbol *));
	/* create list of choice options with a prompt */
	GArray *promptItems = g_array_new(false, false, sizeof(struct symbol *));
	
	struct property *prop;
	for_all_choices(sym, prop) {
		struct expr *expr;
		expr_list_for_each_sym(prop->expr, expr, choice) {
			g_array_append_val(items, choice);
			if (sym_get_prompt(choice) != NULL)
				g_array_append_val(promptItems, choice);
		}
	}
	
	/* if the choice is set to yes, at least one child must be set to yes */
	struct fexpr *c1;
	for (i = 0; i < promptItems->len; i++) {
		choice = g_array_index(promptItems, struct symbol *, i);
		c1 = i == 0 ? choice->fexpr_y : fexpr_or(c1, choice->fexpr_y);
	}
	sym_add_constraint(sym, implies(sym->fexpr_y, c1));
	
	/* every choice option (even those without a prompt) implies the choice */
	for (i = 0; i < items->len; i++) {
		choice = g_array_index(items, struct symbol *, i);
		c1 = implies(sym_get_fexpr_both(choice), sym_get_fexpr_both(sym));
		sym_add_constraint(sym, c1);
	}
	
// 	print_sym_constraint(sym);
// 	return;
	
	/* choice options can only select mod, if the entire choice is mod */
	if (sym->type == S_TRISTATE) {
		for (i = 0; i < items->len; i++) {
			choice = g_array_index(items, struct symbol *, i);
			if (choice->type == S_TRISTATE) {
				c1 = implies(choice->fexpr_m, sym->fexpr_m);
				sym_add_constraint(sym, c1);
			}
		}
	}
	
	/* tristate options cannot be m, if the choice symbol is boolean */
	if (sym->type == S_BOOLEAN) {
		for (i = 0; i < items->len; i++) {
			choice = g_array_index(items, struct symbol *, i);
			if (choice->type == S_TRISTATE)
				sym_add_constraint(sym, fexpr_not(choice->fexpr_m));
		}
	}
	
	/* all choice options with a mutually exclusive for yes */
	for (i = 0; i < promptItems->len; i++) {
		choice = g_array_index(promptItems, struct symbol *, i);
		for (j = i + 1; j < promptItems->len; j++) {
			choice2 = g_array_index(promptItems, struct symbol *, j);
			c1 = fexpr_or(fexpr_not(choice->fexpr_y), fexpr_not(choice2->fexpr_y));
			sym_add_constraint(sym, c1);
		}
	}
	
	/* if one choice option with a prompt is set to yes,
	 * then no other option may be set to mod */
	if (sym->type == S_TRISTATE) {
		for (i = 0; i < promptItems->len; i++) {
			choice = g_array_index(promptItems, struct symbol *, i);
		
			GArray *tmp_arr = g_array_new(false, false, sizeof(struct symbol *));
			for (j = 0; j < promptItems->len; j++) {
				choice2 = g_array_index(promptItems, struct symbol *, j);
				if (choice2->type == S_TRISTATE)
					g_array_append_val(tmp_arr, choice2);
			}
			if (tmp_arr->len == 0) continue;
			
			for (j = 0; j < tmp_arr->len; j++) {
				choice2 = g_array_index(tmp_arr, struct symbol *, j);
				c1 = j == 0 ? fexpr_not(choice2->fexpr_m) : fexpr_and(c1, fexpr_not(choice2->fexpr_m));
			}
			c1 = implies(choice->fexpr_y, c1);
			sym_add_constraint(sym, c1);
			
		}
		

	}
	
// 	print_sym_constraint(sym);
}


/*
 * build a constraint, s.t. at least 1 of the symbols for a non-boolean symbol is true
 */
static void sym_nonbool_at_least_1(struct symbol *sym)
{
	assert(sym_is_nonboolean(sym));
	
	struct fexpr *e = NULL;
	int i;
	for (i = 0; i < sym->fexpr_nonbool->arr->len; i++) {
		if (i == 0)
			e = g_array_index(sym->fexpr_nonbool->arr, struct fexpr *, i);
		else 
			e = fexpr_or(e, g_array_index(sym->fexpr_nonbool->arr, struct fexpr *, i));
		
	}
	sym_add_constraint(sym, e);
}

/*
 * build a constraint, s.t. at most 1 of the symbols for a non-boolean symbol can be true
 */
static void sym_nonbool_at_most_1(struct symbol *sym)
{
	assert(sym_is_nonboolean(sym));
	
	struct fexpr *e1, *e2;
	int i, j;
	for (i = 0; i < sym->fexpr_nonbool->arr->len; i++) {
		e1 = g_array_index(sym->fexpr_nonbool->arr, struct fexpr *, i);
		for (j = i + 1; j < sym->fexpr_nonbool->arr->len; j++) {
			e2 = g_array_index(sym->fexpr_nonbool->arr, struct fexpr *, j);
			struct fexpr *e = fexpr_or(fexpr_not(e1), fexpr_not(e2));
			sym_add_constraint(sym, e);
		}
	}
}

/*
 * get the range constraints for int/hex
 */
static void sym_get_range_constraints(struct symbol *sym)
{
	struct property *prop = sym_get_range_prop(sym);
	if (!prop) return;
	
	int base;
	long long range_min, range_max, tmp;
	
	switch (sym->type) {
	case S_INT:
		base = 10;
		break;
	case S_HEX:
		base = 16;
		break;
	default:
		return;
	}
	
	range_min = sym_get_range_val(prop->expr->left.sym, base);
	range_max = sym_get_range_val(prop->expr->right.sym, base);
	
	/* add the values to known values, if they don't exist yet */
	sym_create_nonbool_fexpr(sym, prop->expr->left.sym->name);
	sym_create_nonbool_fexpr(sym, prop->expr->right.sym->name);
	
	struct fexpr *e;
	unsigned int i;
	/* can skip the first non-boolean value, since this is 'n' */
	for (i = 1; i < sym->fexpr_nonbool->arr->len; i++) {
		e = g_array_index(sym->fexpr_nonbool->arr, struct fexpr *, i);
		tmp = strtoll(str_get(&e->nb_val), NULL, base);
		
		/* known value is in range, nothing to do here */
		if (tmp >= range_min && tmp <= range_max) continue;
		
		struct fexpr *not_e = fexpr_not(e);
		if (!prop->visible.expr) {
			/* no prompt condition => !e */
			sym_add_constraint(sym, not_e);
		} else {
			/* prompt condition => condition implies !e */
			struct k_expr *ke = parse_expr(prop->visible.expr, NULL);
			struct fexpr *promptCond = calculate_fexpr_both(ke);
			convert_fexpr_to_nnf(promptCond);
			
			struct fexpr *e4 = implies(promptCond, not_e);
			sym_add_constraint(sym, e4);
		}
	}
}


static struct default_map * default_map_create_entry(char *val, struct fexpr *e)
{
	struct default_map *map = malloc(sizeof(struct default_map));
	map->val = str_new();
	str_append(&map->val, val);
	map->e = e;
	
	return map;
}

/*
 * return the property for the default value of a symbol
 */
static struct property *sym_get_default_prop(struct symbol *sym)
{
	struct property *prop;

	for_all_defaults(sym, prop) {
		prop->visible.tri = expr_calc_value(prop->visible.expr);
		if (prop->visible.tri != no)
			return prop;
	}
	return NULL;
}

/* get the value for the range */
static long long sym_get_range_val(struct symbol *sym, int base)
{
	sym_calc_value(sym);
	switch (sym->type) {
	case S_INT:
		base = 10;
		break;
	case S_HEX:
		base = 16;
		break;
	default:
		break;
	}
	return strtoll(sym->curr.val, NULL, base);
}

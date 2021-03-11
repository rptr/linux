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

#define KCR_CMP false
#define DEBUG_FUN false

static void debug_info(void);

static void build_tristate_constraint_clause(struct symbol *sym);

static void add_selects_kcr(struct symbol *sym);
static void add_selects(struct symbol *sym);

static void add_dependencies_bool(struct symbol* sym);
static void add_dependencies_bool_kcr(struct symbol* sym);
static void add_dependencies_nonbool(struct symbol *sym);

static void add_choice_prompt_cond(struct symbol *sym);
static void add_choice_dependencies(struct symbol *sym);
static void add_choice_constraints(struct symbol *sym);
static void add_invisible_constraints(struct symbol *sym);
static void sym_nonbool_at_least_1(struct symbol *sym);
static void sym_nonbool_at_most_1(struct symbol *sym);
static void sym_add_nonbool_values_from_ranges(struct symbol *sym);
static void sym_add_range_constraints(struct symbol *sym);
static void sym_add_nonbool_prompt_constraint(struct symbol *sym);

static struct default_map * create_default_map_entry(struct fexpr *val, struct pexpr *e);
static GArray * get_defaults(struct symbol *sym);
static struct pexpr * get_default_y(GArray *arr);
static struct pexpr * get_default_m(GArray *arr);
static long long sym_get_range_val(struct symbol *sym, int base);

/* -------------------------------------- */

static void debug_info(void)
{
	unsigned int i;
	struct symbol *sym;
	sym = sym_find("REGMAP_I2C");
	if (sym->dir_dep.expr)
		print_expr("dep:", sym->dir_dep.expr, E_NONE);
	if (sym->rev_dep.expr)
		print_expr("rdep:", sym->rev_dep.expr, E_NONE);
	for_all_symbols(i, sym) {
// REGMAP_I2C, 
	}
}

/*
 * build the constraints for each symbol
 */
void get_constraints(void)
{
	unsigned int i;
	struct symbol *sym;
	
	if (DEBUG_FUN) {
		debug_info();
		return;
	}
	
	printf("Building constraints...");
	
	/* build constraints for boolean symbols */
	for_all_symbols(i, sym) {
		
		if (!sym_is_boolean(sym)) continue;

		/* build tristate constraints */
		if (sym->type == S_TRISTATE)
			build_tristate_constraint_clause(sym);
		
		/* build constraints for select statements
		 * need to treat choice symbols seperately */
		if (!KCR_CMP) {
			add_selects(sym);
		} else {
			if (sym->rev_dep.expr && !sym_is_choice(sym) && !sym_is_choice_value(sym))
				add_selects_kcr(sym);
		}
		
		/* build constraints for dependencies for booleans */
		if (sym->dir_dep.expr && !sym_is_choice(sym) && !sym_is_choice_value(sym)) {
			if (!KCR_CMP)
				add_dependencies_bool(sym);
			else
				add_dependencies_bool_kcr(sym);
		}
		
		/* build constraints for choice prompts */
		if (sym_is_choice(sym))
			add_choice_prompt_cond(sym);
		
		/* build constraints for dependencies (choice symbols and options) */
		if (sym_is_choice(sym) || sym_is_choice_value(sym))
			add_choice_dependencies(sym);
		
		/* build constraints for the choice groups */
		if (sym_is_choice(sym))
			add_choice_constraints(sym);
		
		
// 		if (sym->name && strcmp(sym->name, "CRAMFS_MTD") == 0) {
// 			printf("PRINTING CRAMFS_MTD\n");
// 			print_symbol(sym);
// 			print_sym_constraint(sym);
// 		}
		
		/* build invisible constraints */
		add_invisible_constraints(sym);
		
	}
	
	/* 
	 * build the constraints for select-variables
	 * skip non-Booleans, choice symbols/options och symbols without rev_dir
	 */
	for_all_symbols(i, sym) {
		
		if (KCR_CMP) continue;
		
		if (!sym_is_boolean(sym)) continue;
		
		if (sym_is_choice(sym) || sym_is_choice_value(sym)) continue;
		
		if (!sym->rev_dep.expr) continue;
		
// 		assert(sym->rev_dep.expr != NULL);
		if (sym->list_sel_y == NULL) {
			continue;
		}
        
// 		struct fexpr *sel_y = implies(sym->fexpr_sel_y, sym->fexpr_y);
// 		sym_add_constraint_fexpr(sym, sel_y);
		struct pexpr *sel_y = pexpr_implies(pexf(sym->fexpr_sel_y), pexf(sym->fexpr_y));
		sym_add_constraint(sym, sel_y);
		
// 		struct fexpr *c1 = implies(sym->fexpr_sel_y, sym->list_sel_y);
// 		sym_add_constraint_fexpr(sym, c1);
		struct pexpr *c1 = pexpr_implies(pexf(sym->fexpr_sel_y), sym->list_sel_y);
		sym_add_constraint(sym, c1);
		
		/* only continue for tristates */
		if (sym->type == S_BOOLEAN) continue;
        
		// TODO double check
		struct pexpr *sel_m = pexpr_implies(pexf(sym->fexpr_sel_m), pexf(sym_get_fexpr_both(sym)));
		sym_add_constraint(sym, sel_m);
		
// 		struct fexpr *sel_m = implies(sym->fexpr_sel_m, fexpr_or(sym->fexpr_m, sym->fexpr_y));
// 		sym_add_constraint_fexpr(sym, sel_m);
        
		struct pexpr *c2 = pexpr_implies(pexf(sym->fexpr_sel_m), sym->list_sel_m);
		sym_add_constraint(sym, c2);
		
// 		struct fexpr *c2 = implies(sym->fexpr_sel_m, sym->list_sel_m);
// 		sym_add_constraint_fexpr(sym, c2);
	}

	/* 
	 * build constraints for non-booleans
	 * these constraints might add "known values"
	 */
	for_all_symbols(i, sym) {
		
		if (!sym_is_nonboolean(sym)) continue;
		
		/* add known values from the range-attributes */
		if (sym->type == S_HEX || sym->type == S_INT)
			sym_add_nonbool_values_from_ranges(sym);
		
		/* build invisible constraints */
// 		struct property *prompt = sym_get_prompt(sym);
// 		if (prompt != NULL && prompt->visible.expr)
// 			add_invisible_constraints(sym, prompt);

		/* the symbol must have a value, if there is a prompt */
		if (sym_has_prompt(sym))
			sym_add_nonbool_prompt_constraint(sym);
		
		/* add current value to possible values */
		if (!sym->flags || !(sym->flags & SYMBOL_VALID))
			sym_calc_value(sym);
		const char *curr = sym_get_string_value(sym);
		if (strcmp(curr, "") != 0)
			sym_create_nonbool_fexpr(sym, (char *) curr);
	}
	
	/* 
	 * build constraints for non-booleans, part deux
	 * the following constraints will not add any "known values"
	 */
	for_all_symbols(i, sym) {
		
		if (!sym_is_nonboolean(sym)) continue;
		
		/* build the range constraints for int/hex */
		if (sym->type == S_HEX || sym->type == S_INT)
			sym_add_range_constraints(sym);
		
		/* build constraints for dependencies for non-booleans */
		if (sym->dir_dep.expr)
			add_dependencies_nonbool(sym);
		
		/* exactly one of the symbols must be true */
		sym_nonbool_at_least_1(sym);
		sym_nonbool_at_most_1(sym);
	}

// 	printf("done.\n");
}

/*
 * enforce tristate constraints
 * - X and X_MODULE are mutually exclusive
 * - X_MODULE implies the MODULES symbol
 */
static void build_tristate_constraint_clause(struct symbol *sym)
{
	assert(sym->type == S_TRISTATE);

	/* -X v -X_m */
// 	struct fexpr * c_tmp = fexpr_or(fexpr_not(sym->fexpr_y), fexpr_not(sym->fexpr_m));
	struct pexpr *X = pexf(sym->fexpr_y), *X_m = pexf(sym->fexpr_m), *modules = pexf(modules_sym->fexpr_y);
	
	struct pexpr *c = pexpr_or(pexpr_not(X), pexpr_not(X_m));
	sym_add_constraint(sym, c);

	/* X_m -> MODULES */
	// TODO doublecheck that
	if (modules_sym->fexpr_y != NULL) {
		struct pexpr *c2 = pexpr_implies(X_m, modules);
		sym_add_constraint(sym, c2);
	}
}

/*
 * build the select constraints
 * - RDep(X) implies X
 */
static void add_selects_kcr(struct symbol *sym)
{
	/* parse reverse dependency */
	struct k_expr *ke = parse_expr(sym->rev_dep.expr, NULL);
	
	struct pexpr *rdep_y = calculate_pexpr_y(ke);
	struct pexpr *c1 = pexpr_implies(rdep_y, pexf(sym->fexpr_y));
	sym_add_constraint(sym, c1);
	
	struct pexpr *rdep_both = calculate_pexpr_both(ke);
	struct pexpr *c2 = pexpr_implies(rdep_both, pexf(sym_get_fexpr_both(sym)));
	sym_add_constraint(sym, c2);
}

/*
 * build the select constraints simplified
 * - RDep(X) implies X
 */
static void add_selects(struct symbol *sym)
{
	assert(sym_is_boolean(sym));
	struct property *p;
	for_all_properties(sym, p, P_SELECT) {
		struct symbol *selected = p->expr->left.sym;
		
		if (selected->type == S_UNKNOWN) continue;
		
		assert(selected->rev_dep.expr);
		
		struct pexpr *cond_y = pexf(const_true);
		struct pexpr *cond_both = pexf(const_true);
		if (p->visible.expr) {
			struct k_expr *ke = parse_expr(p->visible.expr, NULL);
			cond_y = calculate_pexpr_y(ke);
			cond_both = calculate_pexpr_both(ke);
		}
		
		struct pexpr *e1 = pexpr_implies(pexpr_and(cond_y, pexf(sym->fexpr_y)), pexf(selected->fexpr_sel_y));
		
// 		struct fexpr *e1 = implies(fexpr_and(cond_y, sym->fexpr_y), selected->fexpr_sel_y);
        
		sym_add_constraint(selected, e1);
		
		/* imply that symbol is selected to y */
		if (selected->list_sel_y == NULL)
			selected->list_sel_y = pexpr_and(cond_y, pexf(sym->fexpr_y));
		else
			selected->list_sel_y = pexpr_or(selected->list_sel_y, pexpr_and(cond_y, pexf(sym->fexpr_y)));
			
		assert(selected->list_sel_y != NULL);
		
		/* nothing more to do here */
		if (sym->type == S_BOOLEAN && selected->type == S_BOOLEAN) continue;
		

		struct pexpr *e2 = pexpr_implies(pexpr_and(cond_both, pexf(sym_get_fexpr_both(sym))), pexf(sym_get_fexpr_sel_both(selected)));
		
// 		struct fexpr *e2 = implies(fexpr_and(sym_get_fexpr_both(sym), cond_both), sym_get_fexpr_sel_both(selected));

		sym_add_constraint(selected, e2);
		
		/* imply that symbol is selected */
		if (selected->type == S_TRISTATE) {
			if (selected->list_sel_m == NULL)
				selected->list_sel_m = pexpr_and(cond_both, pexf(sym_get_fexpr_both(sym)));
			else
				selected->list_sel_m = pexpr_or(selected->list_sel_m, pexpr_and(cond_both, pexf(sym_get_fexpr_both(sym))));
		} else {
			selected->list_sel_y = pexpr_or(selected->list_sel_y, pexpr_and(cond_both, pexf(sym_get_fexpr_both(sym))));
		}
	}
}

/*
 * build the dependency constraints for booleans
 *  - X implies Dep(X) or RDep(X)
 */
static void add_dependencies_bool(struct symbol *sym)
{
	assert(sym_is_boolean(sym));
	assert(sym->dir_dep.expr);
	
	struct k_expr *ke_dirdep = parse_expr(sym->dir_dep.expr, NULL);
	
// 	struct fexpr *dep_both = calculate_fexpr_both(ke_dirdep);
	struct pexpr *dep_both = calculate_pexpr_both(ke_dirdep);
	
	if (sym->type == S_TRISTATE) {
// 		struct fexpr *dep_y = calculate_fexpr_y(ke_dirdep);
		struct pexpr *dep_y = calculate_pexpr_y(ke_dirdep);
// 		struct fexpr *sel_y = sym->rev_dep.expr ? sym->fexpr_sel_y : const_false;
		struct pexpr *sel_y = sym->rev_dep.expr ? pexf(sym->fexpr_sel_y) : pexf(const_false);
		
// 		struct fexpr *fe_y = implies(sym->fexpr_y, fexpr_or(dep_y, sel_y));
		struct pexpr *c1 = pexpr_implies(pexf(sym->fexpr_y), pexpr_or(dep_y, sel_y));
		
		sym_add_constraint(sym, c1);
// 		sym_add_constraint_fexpr(sym, fe_y);
		
// 		struct fexpr *fe_both = implies(sym->fexpr_m, fexpr_or(dep_both, sym_get_fexpr_sel_both(sym)));
		
		struct pexpr *c2 = pexpr_implies(pexf(sym->fexpr_m), pexpr_or(dep_both, pexf(sym_get_fexpr_sel_both(sym))));

		sym_add_constraint(sym, c2);
// 		sym_add_constraint_fexpr(sym, fe_both);
	} else if (sym->type == S_BOOLEAN) {
		
// 		struct fexpr *fe_both = implies(sym->fexpr_y, fexpr_or(dep_both, sym_get_fexpr_sel_both(sym)));
// 		sym_add_constraint_fexpr(sym, fe_both);
		
		struct pexpr *c = pexpr_implies(pexf(sym->fexpr_y), pexpr_or(dep_both, pexf(sym_get_fexpr_both(sym))));
		sym_add_constraint(sym, c);
	}
}

/*
 * build the dependency constraints for booleans (KCR)
 *  - X implies Dep(X) or RDep(X)
 */
static void add_dependencies_bool_kcr(struct symbol *sym)
{
	assert(sym_is_boolean(sym));
	assert(sym->dir_dep.expr);
	
	struct k_expr *ke_dirdep = parse_expr(sym->dir_dep.expr, NULL);
	struct k_expr *ke_revdep = sym->rev_dep.expr ? parse_expr(sym->rev_dep.expr, NULL) : get_const_false_as_kexpr();
	
// 	struct fexpr *dep_both = calculate_fexpr_both(ke_dirdep);
	struct pexpr *dep_both = calculate_pexpr_both(ke_dirdep);
	
// 	struct fexpr *sel_both = sym->rev_dep.expr ? calculate_fexpr_both(ke_revdep) : const_false;
	struct pexpr *sel_both = sym->rev_dep.expr ? calculate_pexpr_both(ke_revdep) : pexf(const_false);
	
	if (sym->type == S_TRISTATE) {
// 		struct fexpr *dep_y = calculate_fexpr_y(ke_dirdep);
		struct pexpr *dep_y = calculate_pexpr_y(ke_dirdep);
// 		struct fexpr *sel_y = calculate_fexpr_y(ke_revdep);
		struct pexpr *sel_y = calculate_pexpr_y(ke_revdep);
// 		struct fexpr *fe_y = implies(sym->fexpr_y, fexpr_or(dep_y, sel_y));
		struct pexpr *c1 = pexpr_implies(pexf(sym->fexpr_y), pexpr_or(dep_y, sel_y));
		sym_add_constraint(sym, c1);
		
// 		sym_add_constraint_fexpr(sym, fe_y);
		
		struct pexpr *c2 = pexpr_implies(pexf(sym->fexpr_m), pexpr_or(dep_both, sel_both));
		sym_add_constraint(sym, c2);
		
// 		struct fexpr *fe_both = implies(sym->fexpr_m, fexpr_or(dep_both, sel_both));
// 		sym_add_constraint_fexpr(sym, fe_both);
	} else if (sym->type == S_BOOLEAN) {
// 		struct fexpr *fe_both = implies(sym->fexpr_y, fexpr_or(dep_both, sel_both));
// 		sym_add_constraint_fexpr(sym, fe_both);
		
		struct pexpr *c = pexpr_implies(pexf(sym->fexpr_y), pexpr_or(dep_both, sel_both));
		sym_add_constraint(sym, c);
	}
}

/*
 * build the dependency constraints for non-booleans
 * X_i implies Dep(X)
 */
static void add_dependencies_nonbool(struct symbol *sym)
{
	assert(sym_is_nonboolean(sym));
	assert(sym->dir_dep.expr);
	assert(!sym->rev_dep.expr);
	
	struct k_expr *ke_dirdep = parse_expr(sym->dir_dep.expr, NULL);
// 	struct k_expr *ke_revdep = sym->rev_dep.expr ? parse_expr(sym->rev_dep.expr, NULL) : get_const_false_as_kexpr();
	
// 	struct fexpr *dep_both = calculate_fexpr_both(ke_dirdep);
	struct pexpr *dep_both = calculate_pexpr_both(ke_dirdep);
// 	struct pexpr *sel_both = sym->rev_dep.expr ? calculate_pexpr_both(ke_revdep) : pexf(const_false);
// 	struct fexpr *sel_both = sym->rev_dep.expr ? calculate_fexpr_both(ke_revdep) : const_false;
	
	// TODO check
// 	if (sel_both->type == PE_SYMBOL && sel_both->left.fexpr != const_false)
// 		perror("Non-boolean symbol has reverse dependencies.");
	
// 	if (sym->rev_dep.expr)
// 		perror("Non-boolean symbol has reverse dependencies.");

// 	struct fexpr *nb_vals = const_false;
	struct pexpr *nb_vals = pexf(const_false);
	struct fexpr_node *tmp;
	/* can skip the first non-boolean value, since this is 'n' */
	fexpr_list_for_each(tmp, sym->nb_vals) {
		if (tmp->prev == NULL) continue;
		
		nb_vals = pexpr_or(nb_vals, pexf(tmp->elem));
	}
	

// 	struct fexpr *fe_both = implies(nb_vals, fexpr_or(dep_both, sel_both));
	struct pexpr *c = pexpr_implies(nb_vals, dep_both);
	
// 	sym_add_constraint_fexpr(sym, fe_both);
	sym_add_constraint(sym, c);
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
// 	struct fexpr *promptCondition = calculate_fexpr_both(ke);
	struct pexpr *promptCondition = calculate_pexpr_both(ke);

// 	struct fexpr *fe_both = sym_get_fexpr_both(sym);
	struct pexpr *fe_both = pexf(sym_get_fexpr_both(sym));

	if (!sym_is_optional(sym)) {
// 		struct fexpr *req_cond = implies(promptCondition, fe_both);
// 		sym_add_constraint_fexpr(sym, req_cond);
		struct pexpr *req_cond = pexpr_implies(promptCondition, fe_both);
		sym_add_constraint(sym, req_cond);
	}

// 	struct fexpr *pr_cond = implies(fe_both, promptCondition);
	struct pexpr *pr_cond = pexpr_implies(fe_both, promptCondition);
	
// 	sym_add_constraint_fexpr(sym, pr_cond);
	sym_add_constraint(sym, pr_cond);
}

/*
 * build constraints for dependencies (choice symbols and options)
 */
static void add_choice_dependencies(struct symbol *sym)
{
	assert(sym_is_choice(sym) || sym_is_choice_value(sym));
	
	struct property *prompt = sym_get_prompt(sym);
	assert(prompt);
	
	struct k_expr *ke_dirdep;
	if (sym_is_choice(sym)) {
		if (!prompt->visible.expr)
			return;
		ke_dirdep = parse_expr(prompt->visible.expr, NULL);
	} else {
		if (!sym->dir_dep.expr)
			return;
		ke_dirdep = parse_expr(sym->dir_dep.expr, NULL);
	}

// 	struct fexpr *dep_both = calculate_fexpr_both(ke_dirdep);
	struct pexpr *dep_both = calculate_pexpr_both(ke_dirdep);
		
	if (sym->type == S_TRISTATE) {
// 		struct fexpr *dep_y = calculate_fexpr_y(ke_dirdep);
		struct pexpr *dep_y = calculate_pexpr_y(ke_dirdep);
// 		struct fexpr *fe_y = implies(sym->fexpr_y, dep_y);
		struct pexpr *c1 = pexpr_implies(pexf(sym->fexpr_y), dep_y);
// 		sym_add_constraint_fexpr(sym, fe_y);
		sym_add_constraint_eq(sym, c1);
		
// 		struct fexpr *fe_both = implies(sym->fexpr_m, dep_both);
		struct pexpr *c2 = pexpr_implies(pexf(sym->fexpr_m), dep_both);
// 		sym_add_constraint_fexpr(sym, fe_both);
		sym_add_constraint_eq(sym, c2);
	} else if (sym->type == S_BOOLEAN) {
// 		struct fexpr *fe_both = implies(sym->fexpr_y, dep_both);
		struct pexpr *c = pexpr_implies(pexf(sym->fexpr_y), dep_both);
// 		sym_add_constraint_fexpr(sym, fe_both);
		sym_add_constraint_eq(sym, c);
	}
}


/*
 * build constraints for the choice groups
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
// 	struct fexpr *c1 = NULL;
	struct pexpr *c1 = NULL;
	for (i = 0; i < promptItems->len; i++) {
		choice = g_array_index(promptItems, struct symbol *, i);
// 		c1 = i == 0 ? choice->fexpr_y : fexpr_or(c1, choice->fexpr_y);
		c1 = i == 0 ? pexf(choice->fexpr_y) : pexpr_or(c1, pexf(choice->fexpr_y));
	}
	if (c1 != NULL) {
		struct pexpr *c2 = pexpr_implies(pexf(sym->fexpr_y), c1);
		sym_add_constraint(sym, c2);
	}
// 		sym_add_constraint_fexpr(sym, implies(sym->fexpr_y, c1));
	
	/* every choice option (even those without a prompt) implies the choice */
	for (i = 0; i < items->len; i++) {
		choice = g_array_index(items, struct symbol *, i);
// 		c1 = implies(sym_get_fexpr_both(choice), sym_get_fexpr_both(sym));
// 		sym_add_constraint_fexpr(sym, c1);
		c1 = pexpr_implies(pexf(sym_get_fexpr_both(choice)), pexf(sym_get_fexpr_both(sym)));
		sym_add_constraint(sym, c1);
	}

	/* choice options can only select mod, if the entire choice is mod */
	if (sym->type == S_TRISTATE) {
		for (i = 0; i < items->len; i++) {
			choice = g_array_index(items, struct symbol *, i);
			if (choice->type == S_TRISTATE) {
// 				c1 = implies(choice->fexpr_m, sym->fexpr_m);
// 				sym_add_constraint_fexpr(sym, c1);
				c1 = pexpr_implies(pexf(choice->fexpr_m), pexf(sym->fexpr_m));
				sym_add_constraint(sym, c1);
			}
		}
	}
	
	/* tristate options cannot be m, if the choice symbol is boolean */
	if (sym->type == S_BOOLEAN) {
		for (i = 0; i < items->len; i++) {
			choice = g_array_index(items, struct symbol *, i);
			if (choice->type == S_TRISTATE)
				sym_add_constraint(sym, pexpr_not(pexf(choice->fexpr_m)));
// 				sym_add_constraint_fexpr(sym, fexpr_not(choice->fexpr_m));
		}
	}
	
	/* all choice options are mutually exclusive for yes */
	for (i = 0; i < promptItems->len; i++) {
		choice = g_array_index(promptItems, struct symbol *, i);
		for (j = i + 1; j < promptItems->len; j++) {
			choice2 = g_array_index(promptItems, struct symbol *, j);
// 			c1 = fexpr_or(fexpr_not(choice->fexpr_y), fexpr_not(choice2->fexpr_y));
// 			sym_add_constraint_fexpr(sym, c1);
			c1 = pexpr_or(pexpr_not(pexf(choice->fexpr_y)), pexpr_not(pexf(choice2->fexpr_y)));
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
// 				c1 = j == 0 ? fexpr_not(choice2->fexpr_m) : fexpr_and(c1, fexpr_not(choice2->fexpr_m));
				if (j == 0)
					c1 = pexpr_not(pexf(choice2->fexpr_m));
				else
					c1 = pexpr_and(c1, pexpr_not(pexf(choice2->fexpr_m)));
			}
// 			c1 = implies(choice->fexpr_y, c1);
			c1 = pexpr_implies(pexf(choice->fexpr_y), c1);
// 			sym_add_constraint_fexpr(sym, c1);
			sym_add_constraint(sym, c1);
			
		}
	}
}

/*
 * build the constraints for invisible options such as defaults
 */
static void add_invisible_constraints(struct symbol *sym)
{
	struct property *prompt = sym_get_prompt(sym);
	
	/* no prompt conditions, nothing to do here */
	if (prompt != NULL && !prompt->visible.expr) return;
	
	struct pexpr *promptCondition_both, *promptCondition_yes, *nopromptCond;
	if (prompt == NULL) {
		promptCondition_both = pexf(const_false);
		promptCondition_yes = pexf(const_false);
		nopromptCond = pexf(const_true);
	} else {
		struct k_expr *ke_promptCond = parse_expr(prompt->visible.expr, NULL);
		promptCondition_both = calculate_pexpr_both(ke_promptCond);
		promptCondition_yes = calculate_pexpr_y(ke_promptCond);
		nopromptCond = pexpr_not(promptCondition_both);
	}
	
// 	struct k_expr *ke_promptCond = parse_expr(prompt->visible.expr, NULL);
// 	struct fexpr *promptCondition_both = calculate_fexpr_both(ke_promptCond);
// 	struct pexpr *promptCondition_both = calculate_pexpr_both(ke_promptCond);
// 	struct fexpr *promptCondition_yes = calculate_fexpr_y(ke_promptCond);
// 	struct pexpr *promptCondition_yes = calculate_pexpr_y(ke_promptCond);
// 	struct fexpr *nopromptCond = fexpr_not(promptCondition_both);
// 	struct pexpr *nopromptCond = pexpr_not(promptCondition_both);
	
	struct fexpr *npc = fexpr_create(sat_variable_nr++, FE_NPC, "");
	if (sym_is_choice(sym)) {
		str_append(&npc->name, "Choice_");
	}
	str_append(&npc->name, sym_get_name(sym));
	str_append(&npc->name, "_NPC");
	sym->noPromptCond = npc;
	/* add it to satmap */
	g_hash_table_insert(satmap, &npc->satval, npc);
	
	struct pexpr *npc_p = pexf(npc);
	
// 	struct fexpr *c = implies(nopromptCond, npc);
	struct pexpr *c = pexpr_implies(nopromptCond, npc_p);
// 	sym_add_constraint_fexpr(sym, c);
	sym_add_constraint(sym, c);
	

	
	GArray *defaults = get_defaults(sym);
	struct pexpr *default_y = get_default_y(defaults);
	struct pexpr *default_m = get_default_m(defaults);
	struct pexpr *default_both = pexpr_or(default_y, default_m);
	
	/* tristate elements are only selectable as yes, if they are visible as yes */
	if (sym->type == S_TRISTATE) {
		struct pexpr *e1 = pexpr_implies(promptCondition_both, pexpr_implies(pexf(sym->fexpr_y), promptCondition_yes));
		
		sym_add_constraint(sym, e1);
	}

	/* if invisible and off by default, then a symbol can only be deactivated by its reverse dependencies */
	if (sym->type == S_TRISTATE) {
		struct pexpr *sel_y, *sel_m, *sel_both;
		if (sym->fexpr_sel_y != NULL) {
// 			sel_y = implies(sym->fexpr_y, sym->fexpr_sel_y);
			sel_y = pexpr_implies(pexf(sym->fexpr_y), pexf(sym->fexpr_sel_y));
// 			sel_m = implies(sym->fexpr_m, fexpr_or(sym->fexpr_sel_y, sym->fexpr_sel_m));
			sel_m = pexpr_implies(pexf(sym->fexpr_m), pexf(sym->fexpr_sel_m));
// 			sel_both = implies(sym->fexpr_y, fexpr_or(sym->fexpr_sel_y, sym->fexpr_sel_m));
			sel_both = pexpr_implies(pexf(sym->fexpr_y), pexpr_or(pexf(sym->fexpr_sel_m), pexf(sym->fexpr_sel_y)));
		} else {
// 			sel_y = fexpr_not(sym->fexpr_y);
			sel_y = pexpr_not(pexf(sym->fexpr_y));
// 			sel_m = fexpr_not(sym->fexpr_m);
			sel_m = pexpr_not(pexf(sym->fexpr_m));
// 			sel_both = fexpr_not(sym->fexpr_y);
			sel_both = sel_y;
		}
		
		struct pexpr *c1 = pexpr_implies(pexpr_not(default_y), sel_y);
		struct pexpr *c2 = pexpr_implies(pexf(modules_sym->fexpr_y), c1);
		struct pexpr *c3 = pexpr_implies(npc_p, c2);
		sym_add_constraint(sym, c3);

		struct pexpr *d1 = pexpr_implies(pexpr_not(default_m), sel_m);
		struct pexpr *d2 = pexpr_implies(pexf(modules_sym->fexpr_y), d1);
		struct pexpr *d3 = pexpr_implies(npc_p, d2);
		sym_add_constraint(sym, d3);

		struct pexpr *e1 = pexpr_implies(pexpr_not(default_both), sel_both);
		struct pexpr *e2 = pexpr_implies(pexpr_not(pexf(modules_sym->fexpr_y)), e1);
		struct pexpr *e3 = pexpr_implies(npc_p, e2);
		sym_add_constraint(sym, e3);
	} else if (sym->type == S_BOOLEAN) {
		/* somewhat dirty hack since the symbol is defined twice.
		 * that creates problems with the prompt.
		 * needs to be revisited.
		 * TODO: fix it.
		 */
		if (sym->name && strcmp(sym->name, "X86_EXTENDED_PLATFORM") == 0) goto SKIP_PREV_CONSTRAINT;
		
		struct pexpr *sel_y;
		if (sym->fexpr_sel_y != NULL)
			sel_y = pexpr_implies(pexf(sym->fexpr_y), pexf(sym->fexpr_sel_y)); //sym->fexpr_sel_y;
		else
			sel_y = pexpr_not(pexf(sym->fexpr_y));
		
		struct pexpr *e1 = pexpr_implies(pexpr_not(default_both), sel_y);
		struct pexpr *e2 = pexpr_implies(npc_p, e1);
		
		sym_add_constraint_eq(sym, e2);
		

	} else {
		// TODO for non-booleans
	}
	
SKIP_PREV_CONSTRAINT:

	/* if invisible and on by default, then a symbol can only be deactivated by its dependencies */
	if (sym->type == S_TRISTATE) {
		if (defaults->len == 0) return;
		
		struct pexpr *e1 = pexpr_implies(npc_p, pexpr_implies(default_y, pexf(sym->fexpr_y)));
		sym_add_constraint(sym, e1);
		
		struct pexpr *e2 = pexpr_implies(npc_p, pexpr_implies(default_m, pexf(sym_get_fexpr_both(sym))));
		sym_add_constraint(sym, e2);
	} else if (sym->type == S_BOOLEAN) {
		if (defaults->len == 0) return;
		
		struct pexpr *c = pexpr_implies(default_both, pexf(sym->fexpr_y));
		
		// TODO tristate choice hack
		
		struct pexpr *c2 = pexpr_implies(npc_p, c);
		sym_add_constraint(sym, c2);
	} else {
		// TODO for non-booleans
	}

}

/*
 * add the known values from the range-attributes
 */
static void sym_add_nonbool_values_from_ranges(struct symbol *sym)
{
	struct property *prop;
	for_all_properties(sym, prop, P_RANGE) {
		assert(prop != NULL);
		
		/* add the values to known values, if they don't exist yet */
		sym_create_nonbool_fexpr(sym, prop->expr->left.sym->name);
		sym_create_nonbool_fexpr(sym, prop->expr->right.sym->name);
	}
}

/*
 * build the range constraints for int/hex
 */
static void sym_add_range_constraints(struct symbol *sym)
{
	struct property *prop;
	struct pexpr *prevs, *propCond, *e;
	unsigned int i;
	GArray *prevCond = g_array_new(false, false, sizeof(struct pexpr *));
	for_all_properties(sym, prop, P_RANGE) {
		assert(prop != NULL);
		
		prevs = pexf(const_true);
		propCond = prop_get_condition(prop);

		if (prevCond->len == 0) {
			prevs = propCond;
		} else {
			for (i = 0; i < prevCond->len; i++) {
				e = g_array_index(prevCond, struct pexpr *, i);
				prevs = pexpr_and(pexpr_not(e), prevs);
			}
			prevs = pexpr_and(propCond, prevs);
		}
		g_array_append_val(prevCond, propCond);

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
		
// 		/* can skip the first non-boolean value, since this is 'n' */
		struct fexpr_node *node;
		fexpr_list_for_each(node, sym->nb_vals) {
			tmp = strtoll(str_get(&node->elem->nb_val), NULL, base);
			
			/* known value is in range, nothing to do here */
			if (tmp >= range_min && tmp <= range_max) continue;
			
			struct pexpr *not_nb_val = pexpr_not(pexf(node->elem));
			if (tmp < range_min) {
				struct pexpr *c = pexpr_implies(prevs, not_nb_val);
				sym_add_constraint(sym, c);
			}
			
			if (tmp > range_max) {
				struct pexpr *c = pexpr_implies(prevs, not_nb_val);
				sym_add_constraint(sym, c);
			}
		}
	}
}

/*
 * at least 1 of the known values for a non-boolean symbol must be true
 */
static void sym_nonbool_at_least_1(struct symbol *sym)
{
	assert(sym_is_nonboolean(sym));
	
	struct pexpr *e = NULL;
	struct fexpr_node *node;
	fexpr_list_for_each(node, sym->nb_vals) {
		if (node->prev == NULL)
			e = pexf(node->elem);
		else
			e = pexpr_or(e, pexf(node->elem));
	}
	sym_add_constraint(sym, e);
}

/*
 * at most 1 of the known values for a non-boolean symbol can be true
 */
static void sym_nonbool_at_most_1(struct symbol *sym)
{
	assert(sym_is_nonboolean(sym));
	
	struct pexpr *e1, *e2;	
	struct fexpr_node *node1, *node2;
	fexpr_list_for_each(node1, sym->nb_vals) {
		e1 = pexf(node1->elem);
		for (node2 = node1->next; node2 != NULL; node2 = node2->next) {
			e2 = pexf(node2->elem);
			struct pexpr *e = pexpr_or(pexpr_not(e1), pexpr_not(e2));
			sym_add_constraint(sym, e);
		}
	}
}

/*
 * a visible prompt for a non-boolean implies a value for the symbol
 */
static void sym_add_nonbool_prompt_constraint(struct symbol *sym)
{
	struct property *prompt = sym_get_prompt(sym);
	if (prompt == NULL) return;
	
	struct pexpr *promptCondition = prop_get_condition(prompt);
	struct pexpr *n = pexf(sym_get_nonbool_fexpr(sym, "n"));
	assert(n->type == PE_SYMBOL && n->left.fexpr != NULL);
	struct pexpr *c = pexpr_implies(promptCondition, pexpr_not(n));
	
	sym_add_constraint(sym, c);
}


static struct default_map * create_default_map_entry(struct fexpr *val, struct pexpr *e)
{
	struct default_map *map = malloc(sizeof(struct default_map));
	map->val = val;
	map->e = e;
	
	return map;
}

static struct pexpr * findDefaultEntry(struct fexpr *val, GArray *defaults)
{
	unsigned int i;
	struct default_map *entry;
	for (i = 0; i < defaults->len; i++) {
		entry = g_array_index(defaults, struct default_map *, i);
		if (val == entry->val)
			return entry->e;
	}
	
	return pexf(const_false);
}

/* add a default value to the list */
// static struct fexpr *covered;
static struct pexpr *covered;
static void updateDefaultList(struct fexpr *val, struct pexpr *newCond, GArray *defaults)
{
// 	struct fexpr *prevCond = findDefaultEntry(val, defaults);
	struct pexpr *prevCond = findDefaultEntry(val, defaults);
// 	struct fexpr *cond = fexpr_or(prevCond, fexpr_and(newCond, fexpr_not(covered)));
	struct pexpr *cond = pexpr_or(prevCond, pexpr_and(newCond, pexpr_not(covered)));
	struct default_map *entry = create_default_map_entry(val, cond);
	g_array_append_val(defaults, entry);
// 	covered = fexpr_or(covered, newCond);
	covered = pexpr_or(covered, newCond);
}

/*
 * return all defaults for a symbol
 */
static GArray * get_defaults(struct symbol *sym)
{
	struct property *p;
// 	struct default_map *map;
	GArray *defaults = g_array_new(false, false, sizeof(struct default_map *));
	covered = pexf(const_false);
	
	struct k_expr *ke_expr;
	struct pexpr *expr_yes;
	struct pexpr *expr_mod;
	struct pexpr *expr_both;
	
	for_all_defaults(sym, p) {
		ke_expr = p->visible.expr ? parse_expr(p->visible.expr, NULL) : get_const_true_as_kexpr();
		expr_yes = calculate_pexpr_y(ke_expr);
		expr_mod = calculate_pexpr_m(ke_expr);
		expr_both = calculate_pexpr_both(ke_expr);
		
		struct k_expr *ke_v = parse_expr(p->expr, NULL);

// 		print_expr("v/expr:", p->expr, E_NONE);
// 		printf("type: %d\n", p->expr->type);
		
		
// 		assert(p->visible.expr);
// 		print_expr("expr/visible.expr:", p->visible.expr, E_NONE);
// 		assert(p->visible.expr);
		
		/* if tristate and def.value = y */
		if (p->expr->type == E_SYMBOL && sym->type == S_TRISTATE && p->expr->left.sym == &symbol_yes) {
// 			printf("IS TRISTATECONSTANT SYMBOL_YES\n");
			
			updateDefaultList(symbol_yes_fexpr, expr_yes, defaults);
			
			updateDefaultList(symbol_mod_fexpr, expr_mod, defaults);
		}
		/* if def.value = n/m/y */
		else if (p->expr->type == E_SYMBOL && is_tristate_constant(p->expr->left.sym)) {
// 			printf("IS TRISTATECONSTANT\n");
			assert(sym_is_boolean(sym));
			struct fexpr *s = const_true;
			if (p->expr->left.sym == &symbol_yes)
				s = symbol_yes_fexpr;
			else if (p->expr->left.sym == &symbol_mod)
				s = symbol_mod_fexpr;
			else {
// 				perror("Default value of n.");
// 				print_sym_name(sym);
				continue;
			}
			
			assert(s != const_true);
			updateDefaultList(s, expr_both, defaults);
		}
		/* if def.value = non-boolean constant */
		else if (p->expr->type == E_SYMBOL && p->expr->left.sym->type == S_UNKNOWN) {
			// TODO
			continue;
// 			printf("IS NONBOOL CONSTANT\n");
			print_sym_name(sym);
			print_sym_name(p->expr->left.sym);
// 			assert(sym_is_nonboolean(sym));

			struct fexpr *s = sym_get_or_create_nonbool_fexpr(sym, p->expr->left.sym->name);
			
			assert(sym_is_nonboolean(sym));
			
			updateDefaultList(s, expr_both, defaults);
		} 
		/* any expression which evaluates to n/m/y for a tristate */
		else if (sym->type == S_TRISTATE) {
// 			printf("IS TRISTATE EXPRESSION\n");
			struct k_expr *ke = malloc(sizeof(struct k_expr));
			ke->parent = NULL;
			ke->type = KE_AND;
			ke->left = ke_expr;
			ke->right = ke_v;
			
			updateDefaultList(symbol_yes_fexpr, calculate_pexpr_y(ke), defaults);
			
			updateDefaultList(symbol_mod_fexpr, calculate_pexpr_m(ke), defaults);
		}
		/* if non-boolean && def.value = non-boolean symbol */
		else if (p->expr->type == E_SYMBOL && sym_is_nonboolean(sym) && sym_is_nonboolean(p->expr->left.sym)){
// 			printf("IS NONBOOL ITEM\n");
// 			print_sym_name(p->expr->left.sym);
			// TODO
		} 
		/* any expression which evaluates to n/m/y */
		else {
// 			printf("IS ELSE EXPRESSION\n");
			assert(sym->type == S_BOOLEAN);
			struct k_expr *ke = malloc(sizeof(struct k_expr));
			ke->parent = NULL;
			ke->type = KE_AND;
			ke->left = ke_expr;
			ke->right = ke_v;
			
			updateDefaultList(symbol_yes_fexpr, calculate_pexpr_both(ke), defaults);
		}
	}
	
	return defaults;
}

/*
 * return the default_map for "y", False if it doesn't exist
 */
static struct pexpr * get_default_y(GArray *arr)
{
	unsigned int i;
	struct default_map *entry;
	
	for (i = 0; i < arr->len; i++) {
		entry = g_array_index(arr, struct default_map *, i);
		if (entry->val->type == FE_SYMBOL && entry->val->sym == &symbol_yes)
			return entry->e;
	}
	
	return pexf(const_false);
}

/*
 * return the default map for "m", False if it doesn't exist
 */
static struct pexpr * get_default_m(GArray *arr)
{
	unsigned int i;
	struct default_map *entry;
	
	for (i = 0; i < arr->len; i++) {
		entry = g_array_index(arr, struct default_map *, i);
		if (entry->val->type == FE_SYMBOL && entry->val->sym == &symbol_mod)
			return entry->e;
	}
	
	return pexf(const_false);
}

/* 
 * get the value for the range
 */
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

/*
 * count the number of all constraints
 */
unsigned int count_counstraints(void)
{
	unsigned int i, c = 0;
	struct symbol *sym;
	for_all_symbols(i, sym) {
		if (sym->type == S_UNKNOWN) continue;
		
		c += sym->constraints->size;
	}
	
	return c;
}

/*
 * add a constraint for a symbol
 */
void sym_add_constraint(struct symbol *sym, struct pexpr *constraint)
{
	if (!constraint) return;
	
	/* no need to add that */
	if (constraint->type == PE_SYMBOL && constraint->left.fexpr == const_true)
		return;
	
	/* this should never happen */
	if (constraint->type == PE_SYMBOL && constraint->left.fexpr == const_false)
		perror("Adding const_false.");
	
	pexpr_list_add(sym->constraints, constraint);
	
// 	g_array_append_val(sym->constraints->arr, constraint);
	
// 	char *name = sym_get_name(sym);
// 	pexpr_print(name, constraint, -1);
	if (!pexpr_is_nnf(constraint))
		pexpr_print("Not NNF:", constraint, -1);
}

void sym_add_constraint_fexpr(struct symbol *sym, struct fexpr *constraint)
{
	if (!constraint) return;
	
	/* no need to add that */
	if (constraint == const_true) return;
	
	/* this should never happen */
	if (constraint == const_false) perror("Adding const_false.");
	
// 	g_array_append_val(sym->constraints->arr, constraint);
}

/*
 * add a constraint for a symbol, but check for duplicate constraints
 */
static int no_eq = 0, no_cmp = 0;
void sym_add_constraint_eq(struct symbol *sym, struct pexpr *constraint)
{
	if (!constraint) return;
	
	/* no need to add that */
	if (constraint->type == PE_SYMBOL && constraint->left.fexpr == const_true) return;
	
	/* this should never happen */
	if (constraint->type == PE_SYMBOL && constraint->left.fexpr == const_false) perror("Adding const_false.");
	
	if (!pexpr_is_nnf(constraint))
		pexpr_print("Not NNF:", constraint, -1);

// 	struct pexpr *pe_orig;// = pexpr_copy(constraint);
// 	pe_orig = pexpr_eliminate_dups(constraint);
	
// 	if (!pexpr_eq(constraint, pe_orig)) {
// 		printf("SIMPLIFIED\n");
// 		pexpr_print("cons:", constraint, -1);
// 		pexpr_print("simp:", pe_orig, -1);
// 	}

	/* check the constraints for the same symbol */
	struct pexpr_node *node;
	pexpr_list_for_each(node, sym->constraints) {
		no_cmp++;
		if (pexpr_eq(constraint, node->elem)) {
// 			printf("EQUAL\n");
			no_eq++;
// 			if (no_eq % 50 == 0) {
// 				printf("Equiv: %d\n", no_eq);
// 				printf("Comps: %d\n", no_cmp);
// 				printf("Total: %d\n", count_counstraints());
// 				getchar();
// 			}
// 			pexpr_free(pe_orig);
			return;
		}
	}
	
	
// 	for (i = 0; i < sym->constraints->arr->len; i++) {
// 		e = g_array_index(sym->constraints->arr, struct pexpr *, i);
// 
// 		no_cmp++;
// 		if (pexpr_eq(constraint, e)) {
// 			printf("EQUAL\n");
// 			no_eq++;
// 			if (no_eq % 50 == 0) {
// 				printf("Equiv: %d\n", no_eq);
// 				printf("Comps: %d\n", no_cmp);
// 				printf("Total: %d\n", count_counstraints());
// 				getchar();
// 			}
// 			pexpr_free(pe_orig);
// 			return;
// 		}
// 	}
	
// 	char *name = sym_get_name(sym);
// 	pexpr_print(name, constraint, -1);
	
	pexpr_list_add(sym->constraints, constraint);

// 	pexpr_free(pe_orig);
// 	pexpr_free(constraint);
}

// SPDX-License-Identifier: GPL-2.0
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

static void get_constraints_bool(void);
static void get_constraints_select(void);
static void get_constraints_nonbool(void);

static void build_tristate_constraint_clause(struct symbol *sym);

static void add_selects_kcr(struct symbol *sym);
static void add_selects(struct symbol *sym);

static void add_dependencies_bool(struct symbol *sym);
static void add_dependencies_bool_kcr(struct symbol *sym);
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

static struct default_map *create_default_map_entry(struct fexpr *val,
						    struct pexpr *e);
static struct defm_list *get_defaults(struct symbol *sym);
static struct pexpr *get_default_y(struct defm_list *list);
static struct pexpr *get_default_m(struct defm_list *list);
static long long sym_get_range_val(struct symbol *sym, int base);

/* -------------------------------------- */

/*
 * build the constraints for each symbol
 */
void get_constraints(void)
{
#ifdef DEBUG
	printf("THIS SHOULD BE SHOWN IF DEBUG=1.\n");
#endif

	printf("Building constraints...");

	get_constraints_bool();
	get_constraints_select();
	get_constraints_nonbool();
}

/*
 *  build constraints for boolean symbols
 */
void get_constraints_bool(void)
{
	unsigned int i;
	struct symbol *sym;
	for_all_symbols(i, sym) {
		if (!sym_is_boolean(sym))
			continue;

		/* build tristate constraints */
		if (sym->type == S_TRISTATE)
			build_tristate_constraint_clause(sym);

		/* build constraints for select statements
		 * need to treat choice symbols seperately
		 */
		if (!KCR_CMP) {
			add_selects(sym);
		} else {
			if (sym->rev_dep.expr && !sym_is_choice(sym) &&
			    !sym_is_choice_value(sym))
				add_selects_kcr(sym);
		}

		/* build constraints for dependencies for booleans */
		if (sym->dir_dep.expr && !sym_is_choice(sym) &&
		    !sym_is_choice_value(sym)) {
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

		/* build invisible constraints */
		add_invisible_constraints(sym);
	}
}

/* 
 * build the constraints for select-variables
 * skip non-Booleans, choice symbols/options och symbols without rev_dir
 */
void get_constraints_select(void)
{
	unsigned int i;
	struct symbol *sym;

	for_all_symbols(i, sym) {
		if (KCR_CMP)
			continue;

		if (!sym_is_boolean(sym))
			continue;

		if (sym_is_choice(sym) || sym_is_choice_value(sym))
			continue;

		if (!sym->rev_dep.expr)
			continue;

		// 		assert(sym->rev_dep.expr != NULL);
		if (sym->list_sel_y == NULL)
			continue;

		struct pexpr *sel_y = pexpr_implies(pexf(sym->fexpr_sel_y),
						    pexf(sym->fexpr_y));
		sym_add_constraint(sym, sel_y);

		struct pexpr *c1 =
			pexpr_implies(pexf(sym->fexpr_sel_y), sym->list_sel_y);
		sym_add_constraint(sym, c1);

		/* only continue for tristates */
		if (sym->type == S_BOOLEAN)
			continue;

		struct pexpr *sel_m = pexpr_implies(pexf(sym->fexpr_sel_m),
						    sym_get_fexpr_both(sym));
		sym_add_constraint(sym, sel_m);

		struct pexpr *c2 =
			pexpr_implies(pexf(sym->fexpr_sel_m), sym->list_sel_m);
		sym_add_constraint(sym, c2);
	}
}

/*
 * build constraints for non-booleans
 */
void get_constraints_nonbool(void)
{
	unsigned int i;
	struct symbol *sym;

	/* these constraints might add "known values" */
	for_all_symbols(i, sym) {
		if (!sym_is_nonboolean(sym))
			continue;

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
			sym_create_nonbool_fexpr(sym, (char *)curr);
	}

	/* the following constraints will not add any "known values" */
	for_all_symbols(i, sym) {
		if (!sym_is_nonboolean(sym))
			continue;

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
}

/*
 * enforce tristate constraints
 * - X and X_MODULE are mutually exclusive
 * - X_MODULE implies the MODULES symbol
 */
static void build_tristate_constraint_clause(struct symbol *sym)
{
	if (sym->type == S_TRISTATE)
		return;

	/* -X v -X_m */
	struct pexpr *X = pexf(sym->fexpr_y), *X_m = pexf(sym->fexpr_m),
		     *modules = pexf(modules_sym->fexpr_y);

	struct pexpr *c = pexpr_or(pexpr_not(X), pexpr_not(X_m));
	sym_add_constraint(sym, c);

	/* X_m -> MODULES */
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
	struct pexpr *rdep_y = expr_calculate_pexpr_y(sym->rev_dep.expr);
	struct pexpr *c1 = pexpr_implies(rdep_y, pexf(sym->fexpr_y));

	sym_add_constraint(sym, c1);

	struct pexpr *rdep_both = expr_calculate_pexpr_both(sym->rev_dep.expr);
	struct pexpr *c2 = pexpr_implies(rdep_both, sym_get_fexpr_both(sym));

	sym_add_constraint(sym, c2);
}

/*
 * build the select constraints simplified
 * - RDep(X) implies X
 */
static void add_selects(struct symbol *sym)
{
	if (!sym_is_boolean(sym))
		return;

	struct property *p;

	for_all_properties(sym, p, P_SELECT) {
		struct symbol *selected = p->expr->left.sym;

		if (selected->type == S_UNKNOWN)
			continue;

		if (!selected->rev_dep.expr)
			continue;

		struct pexpr *cond_y = pexf(const_true);
		struct pexpr *cond_both = pexf(const_true);
		if (p->visible.expr) {
			cond_y = expr_calculate_pexpr_y(p->visible.expr);
			cond_both = expr_calculate_pexpr_both(p->visible.expr);
		}

		struct pexpr *e1 =
			pexpr_implies(pexpr_and(cond_y, pexf(sym->fexpr_y)),
				      pexf(selected->fexpr_sel_y));

		sym_add_constraint(selected, e1);

		/* imply that symbol is selected to y */
		if (selected->list_sel_y == NULL)
			selected->list_sel_y =
				pexpr_and(cond_y, pexf(sym->fexpr_y));
		else
			selected->list_sel_y =
				pexpr_or(selected->list_sel_y,
					 pexpr_and(cond_y, pexf(sym->fexpr_y)));

		if (selected->list_sel_y == NULL)
			continue;

		/* nothing more to do here */
		if (sym->type == S_BOOLEAN && selected->type == S_BOOLEAN)
			continue;

		struct pexpr *e2 = pexpr_implies(
			pexpr_and(cond_both, sym_get_fexpr_both(sym)),
			sym_get_fexpr_sel_both(selected));

		sym_add_constraint(selected, e2);

		/* imply that symbol is selected */
		if (selected->type == S_TRISTATE) {
			if (selected->list_sel_m == NULL)
				selected->list_sel_m = pexpr_and(
					cond_both, sym_get_fexpr_both(sym));
			else
				selected->list_sel_m = pexpr_or(
					selected->list_sel_m,
					pexpr_and(cond_both,
						  sym_get_fexpr_both(sym)));
		} else {
			selected->list_sel_y = pexpr_or(
				selected->list_sel_y,
				pexpr_and(cond_both, sym_get_fexpr_both(sym)));
		}
	}
}

/*
 * build the dependency constraints for booleans
 *  - X implies Dep(X) or RDep(X)
 */
static void add_dependencies_bool(struct symbol *sym)
{
	if (!sym_is_boolean(sym) || !sym->dir_dep.expr)
		return;

	struct pexpr *dep_both = expr_calculate_pexpr_both(sym->dir_dep.expr);

	if (sym->type == S_TRISTATE) {
		struct pexpr *dep_y = expr_calculate_pexpr_y(sym->dir_dep.expr);
		struct pexpr *sel_y = sym->rev_dep.expr ?
						    pexf(sym->fexpr_sel_y) :
						    pexf(const_false);

		struct pexpr *c1 = pexpr_implies(pexf(sym->fexpr_y),
						 pexpr_or(dep_y, sel_y));

		sym_add_constraint(sym, c1);

		struct pexpr *c2 = pexpr_implies(
			pexf(sym->fexpr_m),
			pexpr_or(dep_both, sym_get_fexpr_sel_both(sym)));

		sym_add_constraint(sym, c2);
	} else if (sym->type == S_BOOLEAN) {
		struct pexpr *c = pexpr_implies(
			pexf(sym->fexpr_y),
			pexpr_or(dep_both, sym_get_fexpr_both(sym)));
		sym_add_constraint(sym, c);
	}
}

/*
 * build the dependency constraints for booleans (KCR)
 *  - X implies Dep(X) or RDep(X)
 */
static void add_dependencies_bool_kcr(struct symbol *sym)
{
	if (!sym_is_boolean(sym) || !sym->dir_dep.expr)
		return;

	struct pexpr *dep_both = expr_calculate_pexpr_both(sym->dir_dep.expr);

	struct pexpr *sel_both =
		sym->rev_dep.expr ?
			      expr_calculate_pexpr_both(sym->rev_dep.expr) :
			      pexf(const_false);

	if (sym->type == S_TRISTATE) {
		struct pexpr *dep_y = expr_calculate_pexpr_y(sym->dir_dep.expr);
		struct pexpr *sel_y = expr_calculate_pexpr_y(sym->rev_dep.expr);
		struct pexpr *c1 = pexpr_implies(pexf(sym->fexpr_y),
						 pexpr_or(dep_y, sel_y));
		sym_add_constraint(sym, c1);

		struct pexpr *c2 = pexpr_implies(pexf(sym->fexpr_m),
						 pexpr_or(dep_both, sel_both));
		sym_add_constraint(sym, c2);
	} else if (sym->type == S_BOOLEAN) {
		struct pexpr *c = pexpr_implies(pexf(sym->fexpr_y),
						pexpr_or(dep_both, sel_both));
		sym_add_constraint(sym, c);
	}
}

/*
 * build the dependency constraints for non-booleans
 * X_i implies Dep(X)
 */
static void add_dependencies_nonbool(struct symbol *sym)
{
	if (!sym_is_nonboolean(sym) || !sym->dir_dep.expr || sym->rev_dep.expr)
		return;

	struct pexpr *dep_both = expr_calculate_pexpr_both(sym->dir_dep.expr);

	struct pexpr *nb_vals = pexf(const_false);
	struct fexpr_node *node;
	/* can skip the first non-boolean value, since this is 'n' */
	fexpr_list_for_each(node, sym->nb_vals) {
		if (node->prev == NULL)
			continue;

		nb_vals = pexpr_or(nb_vals, pexf(node->elem));
	}

	struct pexpr *c = pexpr_implies(nb_vals, dep_both);
	sym_add_constraint(sym, c);
}

/*
 * build the constraints for the choice prompt
 */
static void add_choice_prompt_cond(struct symbol *sym)
{
	if (!sym_is_boolean(sym))
		return;

	struct property *prompt = sym_get_prompt(sym);
	if (prompt == NULL)
		return;

	struct pexpr *promptCondition =
		prompt->visible.expr ?
			      expr_calculate_pexpr_both(prompt->visible.expr) :
			      pexf(const_true);

	struct pexpr *fe_both = sym_get_fexpr_both(sym);

	if (!sym_is_optional(sym)) {
		struct pexpr *req_cond =
			pexpr_implies(promptCondition, fe_both);
		sym_add_constraint(sym, req_cond);
	}

	struct pexpr *pr_cond = pexpr_implies(fe_both, promptCondition);
	sym_add_constraint(sym, pr_cond);
}

/*
 * build constraints for dependencies (choice symbols and options)
 */
static void add_choice_dependencies(struct symbol *sym)
{
	if (!sym_is_choice(sym) || !sym_is_choice_value(sym))
		return;

	struct property *prompt = sym_get_prompt(sym);
	if (prompt == NULL)
		return;

	struct expr *to_parse;
	if (sym_is_choice(sym)) {
		if (!prompt->visible.expr)
			return;
		to_parse = prompt->visible.expr;
	} else {
		if (!sym->dir_dep.expr)
			return;
		to_parse = sym->dir_dep.expr;
	}

	struct pexpr *dep_both = expr_calculate_pexpr_both(to_parse);

	if (sym->type == S_TRISTATE) {
		struct pexpr *dep_y = expr_calculate_pexpr_y(to_parse);
		struct pexpr *c1 = pexpr_implies(pexf(sym->fexpr_y), dep_y);
		sym_add_constraint_eq(sym, c1);

		struct pexpr *c2 = pexpr_implies(pexf(sym->fexpr_m), dep_both);
		sym_add_constraint_eq(sym, c2);
	} else if (sym->type == S_BOOLEAN) {
		struct pexpr *c = pexpr_implies(pexf(sym->fexpr_y), dep_both);
		sym_add_constraint_eq(sym, c);
	}
}

/*
 * build constraints for the choice groups
 */
static void add_choice_constraints(struct symbol *sym)
{
	if (!sym_is_boolean(sym))
		return;

	struct property *prompt = sym_get_prompt(sym);
	if (prompt == NULL)
		return;

	struct symbol *choice, *choice2;
	struct sym_node *node, *node2;

	/* create list of all choice options */
	struct sym_list *items = sym_list_init();
	/* create list of choice options with a prompt */
	struct sym_list *promptItems = sym_list_init();

	struct property *prop;
	for_all_choices(sym, prop) {
		struct expr *expr;
		expr_list_for_each_sym(prop->expr, expr, choice) {
			sym_list_add(items, choice);
			if (sym_get_prompt(choice) != NULL)
				sym_list_add(promptItems, choice);
		}
	}

	/* if the choice is set to yes, at least one child must be set to yes */
	struct pexpr *c1 = NULL;
	sym_list_for_each(node, promptItems) {
		choice = node->elem;
		c1 = node->prev == NULL ? pexf(choice->fexpr_y) :
						pexpr_or(c1, pexf(choice->fexpr_y));
	}
	if (c1 != NULL) {
		struct pexpr *c2 = pexpr_implies(pexf(sym->fexpr_y), c1);
		sym_add_constraint(sym, c2);
	}

	/* every choice option (even those without a prompt) implies the choice */
	sym_list_for_each(node, items) {
		choice = node->elem;
		c1 = pexpr_implies(sym_get_fexpr_both(choice),
				   sym_get_fexpr_both(sym));
		sym_add_constraint(sym, c1);
	}

	/* choice options can only select mod, if the entire choice is mod */
	if (sym->type == S_TRISTATE) {
		sym_list_for_each(node, items) {
			choice = node->elem;
			if (choice->type == S_TRISTATE) {
				c1 = pexpr_implies(pexf(choice->fexpr_m),
						   pexf(sym->fexpr_m));
				sym_add_constraint(sym, c1);
			}
		}
	}

	/* tristate options cannot be m, if the choice symbol is boolean */
	if (sym->type == S_BOOLEAN) {
		sym_list_for_each(node, items) {
			choice = node->elem;
			if (choice->type == S_TRISTATE)
				sym_add_constraint(
					sym, pexpr_not(pexf(choice->fexpr_m)));
		}
	}

	/* all choice options are mutually exclusive for yes */
	sym_list_for_each(node, promptItems) {
		choice = node->elem;
		for (node2 = node->next; node2 != NULL; node2 = node2->next) {
			choice2 = node2->elem;
			c1 = pexpr_or(pexpr_not(pexf(choice->fexpr_y)),
				      pexpr_not(pexf(choice2->fexpr_y)));
			sym_add_constraint(sym, c1);
		}
	}

	/* if one choice option with a prompt is set to yes,
	 * then no other option may be set to mod */
	if (sym->type == S_TRISTATE) {
		sym_list_for_each(node, promptItems) {
			choice = node->elem;

			struct sym_list *tmp = sym_list_init();
			for (node2 = node->next; node2 != NULL;
			     node2 = node2->next) {
				choice2 = node2->elem;
				if (choice2->type == S_TRISTATE)
					sym_list_add(tmp, choice2);
			}
			if (tmp->size == 0)
				continue;

			sym_list_for_each(node2, tmp) {
				choice2 = node2->elem;
				if (node2->prev == NULL)
					c1 = pexpr_not(pexf(choice2->fexpr_m));
				else
					c1 = pexpr_and(
						c1, pexpr_not(pexf(
							    choice2->fexpr_m)));
			}
			c1 = pexpr_implies(pexf(choice->fexpr_y), c1);
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
	if (prompt != NULL && !prompt->visible.expr)
		return;

	struct pexpr *promptCondition_both, *promptCondition_yes, *nopromptCond;
	if (prompt == NULL) {
		promptCondition_both = pexf(const_false);
		promptCondition_yes = pexf(const_false);
		nopromptCond = pexf(const_true);
	} else {
		promptCondition_both =
			expr_calculate_pexpr_both(prompt->visible.expr);
		promptCondition_yes =
			expr_calculate_pexpr_y(prompt->visible.expr);
		nopromptCond = pexpr_not(promptCondition_both);
	}

	struct fexpr *npc = fexpr_create(sat_variable_nr++, FE_NPC, "");
	if (sym_is_choice(sym))
		str_append(&npc->name, "Choice_");

	str_append(&npc->name, sym_get_name(sym));
	str_append(&npc->name, "_NPC");
	sym->noPromptCond = npc;
	fexpr_add_to_satmap(npc);

	struct pexpr *npc_p = pexf(npc);

	struct pexpr *c = pexpr_implies(nopromptCond, npc_p);
	sym_add_constraint(sym, c);

	struct defm_list *defaults = get_defaults(sym);
	struct pexpr *default_y = get_default_y(defaults);
	struct pexpr *default_m = get_default_m(defaults);
	struct pexpr *default_both = pexpr_or(default_y, default_m);

	/* tristate elements are only selectable as yes, if they are visible as yes */
	if (sym->type == S_TRISTATE) {
		struct pexpr *e1 = pexpr_implies(
			promptCondition_both,
			pexpr_implies(pexf(sym->fexpr_y), promptCondition_yes));
		sym_add_constraint(sym, e1);
	}

	/* if invisible and off by default, then a symbol can only be deactivated by its reverse dependencies */
	if (sym->type == S_TRISTATE) {
		struct pexpr *sel_y, *sel_m, *sel_both;
		if (sym->fexpr_sel_y != NULL) {
			sel_y = pexpr_implies(pexf(sym->fexpr_y),
					      pexf(sym->fexpr_sel_y));
			sel_m = pexpr_implies(pexf(sym->fexpr_m),
					      pexf(sym->fexpr_sel_m));
			sel_both =
				pexpr_implies(pexf(sym->fexpr_y),
					      pexpr_or(pexf(sym->fexpr_sel_m),
						       pexf(sym->fexpr_sel_y)));
		} else {
			sel_y = pexpr_not(pexf(sym->fexpr_y));
			sel_m = pexpr_not(pexf(sym->fexpr_m));
			sel_both = sel_y;
		}

		struct pexpr *c1 = pexpr_implies(pexpr_not(default_y), sel_y);
		struct pexpr *c2 =
			pexpr_implies(pexf(modules_sym->fexpr_y), c1);
		struct pexpr *c3 = pexpr_implies(npc_p, c2);
		sym_add_constraint(sym, c3);

		struct pexpr *d1 = pexpr_implies(pexpr_not(default_m), sel_m);
		struct pexpr *d2 =
			pexpr_implies(pexf(modules_sym->fexpr_y), d1);
		struct pexpr *d3 = pexpr_implies(npc_p, d2);
		sym_add_constraint(sym, d3);

		struct pexpr *e1 =
			pexpr_implies(pexpr_not(default_both), sel_both);
		struct pexpr *e2 = pexpr_implies(
			pexpr_not(pexf(modules_sym->fexpr_y)), e1);
		struct pexpr *e3 = pexpr_implies(npc_p, e2);
		sym_add_constraint(sym, e3);
	} else if (sym->type == S_BOOLEAN) {
		/* somewhat dirty hack since the symbol is defined twice.
		 * that creates problems with the prompt.
		 * needs to be revisited.
		 * TODO: fix it.
		 */
		if (sym->name &&
		    strcmp(sym->name, "X86_EXTENDED_PLATFORM") == 0)
			goto SKIP_PREV_CONSTRAINT;

		struct pexpr *sel_y;
		if (sym->fexpr_sel_y != NULL)
			sel_y = pexpr_implies(
				pexf(sym->fexpr_y),
				pexf(sym->fexpr_sel_y)); //sym->fexpr_sel_y;
		else
			sel_y = pexpr_not(pexf(sym->fexpr_y));

		struct pexpr *e1 =
			pexpr_implies(pexpr_not(default_both), sel_y);
		struct pexpr *e2 = pexpr_implies(npc_p, e1);

		sym_add_constraint_eq(sym, e2);

	} else {
		// TODO for non-booleans
	}

SKIP_PREV_CONSTRAINT:

	/* if invisible and on by default, then a symbol can only be deactivated by its dependencies */
	if (sym->type == S_TRISTATE) {
		if (defaults->size == 0)
			return;

		struct pexpr *e1 = pexpr_implies(
			npc_p, pexpr_implies(default_y, pexf(sym->fexpr_y)));
		sym_add_constraint(sym, e1);

		struct pexpr *e2 = pexpr_implies(
			npc_p,
			pexpr_implies(default_m, sym_get_fexpr_both(sym)));
		sym_add_constraint(sym, e2);
	} else if (sym->type == S_BOOLEAN) {
		if (defaults->size == 0)
			return;

		struct pexpr *c =
			pexpr_implies(default_both, pexf(sym->fexpr_y));

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
		if (prop == NULL)
			continue;

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
	struct pexpr *prevs, *propCond;
	struct pexpr_list *prevCond = pexpr_list_init();
	for_all_properties(sym, prop, P_RANGE) {
		if (prop == NULL)
			continue;

		prevs = pexf(const_true);
		propCond = prop_get_condition(prop);

		if (prevCond->size == 0) {
			prevs = propCond;
		} else {
			struct pexpr_node *node;
			pexpr_list_for_each(node, prevCond) prevs =
				pexpr_and(pexpr_not(node->elem), prevs);

			prevs = pexpr_and(propCond, prevs);
		}
		pexpr_list_add(prevCond, propCond);

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

		/* can skip the first non-boolean value, since this is 'n' */
		struct fexpr_node *node;
		fexpr_list_for_each(node, sym->nb_vals) {
			tmp = strtoll(str_get(&node->elem->nb_val), NULL, base);

			/* known value is in range, nothing to do here */
			if (tmp >= range_min && tmp <= range_max)
				continue;

			struct pexpr *not_nb_val = pexpr_not(pexf(node->elem));
			if (tmp < range_min) {
				struct pexpr *c =
					pexpr_implies(prevs, not_nb_val);
				sym_add_constraint(sym, c);
			}

			if (tmp > range_max) {
				struct pexpr *c =
					pexpr_implies(prevs, not_nb_val);
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
	if (!sym_is_boolean(sym))
		return;

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
	if (!sym_is_boolean(sym))
		return;

	struct pexpr *e1, *e2;
	struct fexpr_node *node1, *node2;
	fexpr_list_for_each(node1, sym->nb_vals) {
		e1 = pexf(node1->elem);
		for (node2 = node1->next; node2 != NULL; node2 = node2->next) {
			e2 = pexf(node2->elem);
			struct pexpr *e =
				pexpr_or(pexpr_not(e1), pexpr_not(e2));
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
	if (prompt == NULL)
		return;

	struct pexpr *promptCondition = prop_get_condition(prompt);
	struct pexpr *n = pexf(sym_get_nonbool_fexpr(sym, "n"));

	if (n->type != PE_SYMBOL)
		return;
	if (n->left.fexpr == NULL)
		return;

	struct pexpr *c = pexpr_implies(promptCondition, pexpr_not(n));

	sym_add_constraint(sym, c);
}

static struct default_map *create_default_map_entry(struct fexpr *val,
						    struct pexpr *e)
{
	struct default_map *map = malloc(sizeof(struct default_map));
	map->val = val;
	map->e = e;

	return map;
}

static struct pexpr *findDefaultEntry(struct fexpr *val,
				      struct defm_list *defaults)
{
	struct defm_node *node;
	defm_list_for_each(node,
			   defaults) if (val ==
					 node->elem->val) return node->elem->e;

	return pexf(const_false);
}

/* add a default value to the list */
static struct pexpr *covered;
static void updateDefaultList(struct fexpr *val, struct pexpr *newCond,
			      struct defm_list *defaults)
{
	struct pexpr *prevCond = findDefaultEntry(val, defaults);
	struct pexpr *cond =
		pexpr_or(prevCond, pexpr_and(newCond, pexpr_not(covered)));
	struct default_map *entry = create_default_map_entry(val, cond);
	defm_list_add(defaults, entry);
	covered = pexpr_or(covered, newCond);
}

/*
 * return all defaults for a symbol
 */
static struct defm_list *get_defaults(struct symbol *sym)
{
	struct property *p;
	// 	struct default_map *map;
	struct defm_list *defaults = defm_list_init();
	covered = pexf(const_false);

	struct expr *vis_cond;
	struct pexpr *expr_yes;
	struct pexpr *expr_mod;
	struct pexpr *expr_both;

	for_all_defaults(sym, p) {
		if (p->visible.expr) {
			vis_cond = p->visible.expr;
			expr_yes = expr_calculate_pexpr_y(vis_cond);
			expr_mod = expr_calculate_pexpr_m(vis_cond);
			expr_both = expr_calculate_pexpr_both(vis_cond);
		} else {
			vis_cond = NULL;
			expr_yes = pexf(const_true);
			expr_mod = pexf(const_true);
			expr_both = pexf(const_true);
		}

		/* if tristate and def.value = y */
		if (p->expr->type == E_SYMBOL && sym->type == S_TRISTATE &&
		    p->expr->left.sym == &symbol_yes) {
			// 			printf("IS TRISTATECONSTANT SYMBOL_YES\n");

			updateDefaultList(symbol_yes_fexpr, expr_yes, defaults);

			updateDefaultList(symbol_mod_fexpr, expr_mod, defaults);
		}
		/* if def.value = n/m/y */
		else if (p->expr->type == E_SYMBOL &&
			 sym_is_tristate_constant(p->expr->left.sym)) {
			// 			printf("IS TRISTATECONSTANT\n");
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

			updateDefaultList(s, expr_both, defaults);
		}
		/* if def.value = non-boolean constant */
		else if (p->expr->type == E_SYMBOL &&
			 p->expr->left.sym->type == S_UNKNOWN) {
			// TODO
			continue;
			// 			printf("IS NONBOOL CONSTANT\n");
			print_sym_name(sym);
			print_sym_name(p->expr->left.sym);

			struct fexpr *s = sym_get_or_create_nonbool_fexpr(
				sym, p->expr->left.sym->name);

			assert(sym_is_nonboolean(sym));

			updateDefaultList(s, expr_both, defaults);
		}
		/* any expression which evaluates to n/m/y for a tristate */
		else if (sym->type == S_TRISTATE) {
			struct pexpr *e1 = expr_calculate_pexpr_y(p->expr);
			if (vis_cond != NULL)
				e1 = pexpr_and(
					e1, expr_calculate_pexpr_y(vis_cond));
			updateDefaultList(symbol_yes_fexpr, e1, defaults);

			struct pexpr *e2 = expr_calculate_pexpr_m(p->expr);
			if (vis_cond != NULL)
				e2 = pexpr_and(
					e2, expr_calculate_pexpr_m(vis_cond));
			updateDefaultList(symbol_mod_fexpr, e2, defaults);
		}
		/* if non-boolean && def.value = non-boolean symbol */
		else if (p->expr->type == E_SYMBOL && sym_is_nonboolean(sym) &&
			 sym_is_nonboolean(p->expr->left.sym)) {
			// 			printf("IS NONBOOL ITEM\n");
			// 			print_sym_name(p->expr->left.sym);
			// TODO
		}
		/* any expression which evaluates to n/m/y */
		else {
			// 			printf("IS ELSE EXPRESSION\n");
			assert(sym->type == S_BOOLEAN);

			struct pexpr *e3 = expr_calculate_pexpr_both(p->expr);
			if (vis_cond != NULL)
				e3 = pexpr_and(e3, expr_calculate_pexpr_both(
							   vis_cond));
			updateDefaultList(symbol_yes_fexpr, e3, defaults);
		}
	}

	return defaults;
}

/*
 * return the default_map for "y", False if it doesn't exist
 */
static struct pexpr *get_default_y(struct defm_list *list)
{
	struct default_map *entry;
	struct defm_node *node;

	defm_list_for_each(node, list) {
		entry = node->elem;
		if (entry->val->type == FE_SYMBOL &&
		    entry->val->sym == &symbol_yes)
			return entry->e;
	}

	return pexf(const_false);
}

/*
 * return the default map for "m", False if it doesn't exist
 */
static struct pexpr *get_default_m(struct defm_list *list)
{
	struct default_map *entry;
	struct defm_node *node;

	defm_list_for_each(node, list) {
		entry = node->elem;
		if (entry->val->type == FE_SYMBOL &&
		    entry->val->sym == &symbol_mod)
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
		if (sym->type == S_UNKNOWN)
			continue;

		c += sym->constraints->size;
	}

	return c;
}

/*
 * add a constraint for a symbol
 */
void sym_add_constraint(struct symbol *sym, struct pexpr *constraint)
{
	if (!constraint)
		return;

	/* no need to add that */
	if (constraint->type == PE_SYMBOL &&
	    constraint->left.fexpr == const_true)
		return;

	/* this should never happen */
	if (constraint->type == PE_SYMBOL &&
	    constraint->left.fexpr == const_false)
		perror("Adding const_false.");

	pexpr_list_add(sym->constraints, constraint);

	if (!pexpr_is_nnf(constraint))
		pexpr_print("Not NNF:", constraint, -1);
}

/*
 * add a constraint for a symbol, but check for duplicate constraints
 */
static int no_eq = 0, no_cmp = 0;
void sym_add_constraint_eq(struct symbol *sym, struct pexpr *constraint)
{
	if (!constraint)
		return;

	/* no need to add that */
	if (constraint->type == PE_SYMBOL &&
	    constraint->left.fexpr == const_true)
		return;

	/* this should never happen */
	if (constraint->type == PE_SYMBOL &&
	    constraint->left.fexpr == const_false)
		perror("Adding const_false.");

	if (!pexpr_is_nnf(constraint))
		pexpr_print("Not NNF:", constraint, -1);

	/* check the constraints for the same symbol */
	struct pexpr_node *node;
	pexpr_list_for_each(node, sym->constraints) {
		no_cmp++;
		if (pexpr_eq(constraint, node->elem)) {
			no_eq++;
			return;
		}
	}

	pexpr_list_add(sym->constraints, constraint);
}

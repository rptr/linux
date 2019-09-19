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
#include "constraints.h"
#include "fexpr.h"
#include "satprint.h"
#include "satutils.h"

static void create_tristate_constraint_clause(struct symbol *sym);
static void add_selects(struct symbol *sym);
static void add_dependencies(struct symbol* sym);
static void sym_nonbool_at_least_1(struct symbol *sym);
static void sym_nonbool_at_most_1(struct symbol *sym);

static struct default_map * default_map_create_entry(char *val, struct fexpr *e);
static struct property *sym_get_default_prop(struct symbol *sym);

/* -------------------------------------- */

/*
 * build the constraints for each symbol
 */
void get_constraints(void)
{
	unsigned int i;
	struct symbol *sym;
	struct property *p, *p2;
	
	printf("Creating constraints...");
	
// 	printf("\n");
	for_all_symbols(i, sym) {
		if (!sym_is_boolean(sym) && !sym_is_nonboolean(sym))
			continue;
		
		/* create tristate constraints */
		if (sym_get_type(sym) == S_TRISTATE)
			create_tristate_constraint_clause(sym);
		
		/* build constraints for select statements */
		if (sym->rev_dep.expr)
			add_selects(sym);
		
		/* build constraints for dependencies */
		if (sym->dir_dep.expr)
			add_dependencies(sym);
		
		GArray *defaults = g_array_new(false, false, sizeof(struct default_map));
		
		
		for_all_properties(sym, p, P_PROMPT) {
			if (!p->visible.expr) continue;
			
// 			printf("sym %s, prompt %s\n", sym->name, p->text);
// 			printf("Prompt condition: ");
			
			struct k_expr *ke = parse_expr(p->visible.expr, NULL);
			struct fexpr *nopromptCond = fexpr_not(calculate_fexpr_both(ke));
			
			struct property *defprop = sym_get_default_prop(sym);
			if (defprop->visible.expr) {
				
			}

			
// 			print_fexpr(nopromptCond, -1);
// 			printf("\n");
			
			
			
// 			printf("promptCondition.fexpr_both:\n");
		}

	}
	
	/* create constraints for non-booleans */
	for_all_symbols(i, sym) {
		/* non-boolean symbols can have at most one of their symbols to be true */
		if (sym_is_nonboolean(sym)) {
			sym_nonbool_at_least_1(sym);
			sym_nonbool_at_most_1(sym);
		}
	}
	
	printf("done.\n");
}

/*
 * enforce tristate constraints
 * X and X_MODULE cannot both be true
 * also, the MODULES symbol must be set to yes/mod for tristates to be allowed 
 */
static void create_tristate_constraint_clause(struct symbol *sym)
{
	assert(sym_get_type(sym) == S_TRISTATE);
	char reason[CNF_REASON_LENGTH];
	strcpy(reason, "(#): enforce tristate constraints for symbol ");
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
 * create the select constraints
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
 * create the dependency constraints
 */
static void add_dependencies(struct symbol *sym)
{
	assert(sym->dir_dep.expr);
	
	struct k_expr *ke_dirdep = parse_expr(sym->dir_dep.expr, NULL);
	struct k_expr *ke_revdep = sym->rev_dep.expr ? parse_expr(sym->rev_dep.expr, NULL) : get_const_false_as_kexpr();

	struct gstr reason = str_new();
	str_append(&reason, "(#): ");
	str_append(&reason, sym->name);
	str_append(&reason, " depends on ");
	// TODO ke_dirdep | ke_revdep
	kexpr_as_char(ke_dirdep, &reason);
	
	struct fexpr *dep_both = calculate_fexpr_both(ke_dirdep);
	struct fexpr *sel_both = calculate_fexpr_both(ke_revdep);
	
	if (sym_get_type(sym) == S_TRISTATE) {
		struct fexpr *dep_y = calculate_fexpr_y(ke_dirdep);
		struct fexpr *sel_y = calculate_fexpr_y(ke_revdep);
		struct fexpr *fe_y = implies(sym->fexpr_y, fexpr_or(dep_y, sel_y));
		sym_add_constraint(sym, fe_y);
		
		struct fexpr *fe_both = implies(sym->fexpr_m, fexpr_or(dep_both, sel_both));
		sym_add_constraint(sym, fe_both);
	} else if (sym_get_type(sym) == S_BOOLEAN) {
			struct fexpr *fe_both = implies(sym->fexpr_y, fexpr_or(dep_both, sel_both));
			sym_add_constraint(sym, fe_both);
	} else if (sym_is_nonboolean(sym)) {
		char int_values[][2] = {"0", "1"};
		char hex_values[][4] = {"0x0", "0x1"};
		char string_values[][9] = {"", "nonempty"};
		struct fexpr *e1, *e2;
		
		switch (sym_get_type(sym)) {
		case S_INT:
			e1 = sym_get_nonbool_fexpr(sym, int_values[0]);
			e2 = sym_get_nonbool_fexpr(sym, int_values[1]);
			break;
		case S_HEX:
			e1 = sym_get_nonbool_fexpr(sym, hex_values[0]);
			e2 = sym_get_nonbool_fexpr(sym, hex_values[1]);
			break;
		case S_STRING:
			e1 = sym_get_nonbool_fexpr(sym, string_values[0]);
			e2 = sym_get_nonbool_fexpr(sym, string_values[1]);
			break;
		default:
			e1 = const_true;
			e2 = const_true;
			break;
		}
		
		struct fexpr *fe_both = implies(fexpr_or(e1, e2), fexpr_or(dep_both, sel_both));
		sym_add_constraint(sym, fe_both);
	}

}

/*
 * build a constraint, s.t. at least 1 of the symbols for a non-boolean symbol is true
 */
static void sym_nonbool_at_least_1(struct symbol *sym)
{
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

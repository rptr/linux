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
#include "cnf.h"
#include "satprint.h"
#include "satutils.h"
#include "fexpr.h"

static struct tmp_sat_variable * create_tmp_sat_var(struct tmp_sat_variable *parent);
static void build_cnf_tseytin_t(struct tmp_sat_variable *t, struct fexpr *e);
static void build_cnf_tseytin_or(struct tmp_sat_variable *t, struct fexpr *e);
static void build_cnf_tseytin_and(struct tmp_sat_variable *t, struct fexpr *e);
static void build_cnf_tseytin_not(struct tmp_sat_variable *t, struct fexpr *e);


/*
 * construct the CNF-clauses from the constraints
 */
void construct_cnf_clauses()
{
	unsigned int i, j;
	struct symbol *sym;
	
	printf("Constructing CNF-clauses...");
	
	for_all_symbols(i, sym) {
		if (sym_get_type(sym) == S_UNKNOWN) continue;
		
		struct fexpr *e;
		for (j = 0; j < sym->constraints->arr->len; j++) {
			e = g_array_index(sym->constraints->arr, struct fexpr *, j);
			convert_fexpr_to_cnf(e);
			unfold_cnf_clause(e);
		}
	}
	
	printf("done.\n");
}

void build_cnf(struct fexpr *e)
{
	if (!e)
		return;
	
	switch (e->type) {
	case FE_SYMBOL:
		break;
	case FE_OR:
		if (fexpr_is_symbol_or_not(e->left) && fexpr_is_symbol_or_not(e->right)) {
			int a = e->left->type == FE_NOT ? -e->left->left->satval : e->left->satval;
			int b = e->right->type == FE_NOT ? -e->right->left->satval : e->right->satval;
			struct gstr reason = str_new();
			fexpr_as_char_short(e, &reason, -1);
			build_cnf_clause(&reason, 2, a, b);
		} else {
			build_cnf_tseytin_or(create_tmp_sat_var(NULL), e);
		}
		break;
	case FE_AND:
		if (fexpr_is_symbol(e->left) && fexpr_is_symbol(e->right)) {
			int a = e->left->type == FE_NOT ? -e->left->left->satval : e->left->satval;
			int b = e->right->type == FE_NOT ? -e->right->left->satval : e->right->satval;
			struct gstr reason = str_new();
			fexpr_as_char_short(e, &reason, -1);
			build_cnf_clause(&reason, 1, a);
			build_cnf_clause(&reason, 1, b);
		} else {
			build_cnf_tseytin_and(create_tmp_sat_var(NULL), e);
		}
	default:
		break;
	}
	
}

/*
 * create a struct for a temporary SAT variable
 */
static struct tmp_sat_variable * create_tmp_sat_var(struct tmp_sat_variable *parent)
{
	struct tmp_sat_variable *tvar = malloc(sizeof(struct tmp_sat_variable));
	tvar->nr = tmp_variable_nr++;
	tvar->satval = sat_variable_nr++;
	strcpy(tvar->sval, get_tmp_var_as_char(tvar->nr));
	tvar->parent = parent;
	tvar->next = tmp_sat_vars;
	tmp_sat_vars = tvar;
	
	// TODO
	struct gstr tstr = str_new();
	str_append(&tstr, tvar->sval);
	/* add it to the satmap */
	g_hash_table_insert(satmap, &tvar->satval, strdup(str_get(&tstr)));
	
	return tvar;
}

static void build_cnf_tseytin_t(struct tmp_sat_variable *t, struct fexpr *e)
{
	if (!e || t == NULL)
		return;
	
	switch (e->type) {
	case FE_OR:
		build_cnf_tseytin_or(t, e);
		break;
	case FE_AND:
		build_cnf_tseytin_and(t, e);
		break;
	case FE_NOT:
		build_cnf_tseytin_not(t, e);
		break;
	default:
		printf("SHOULD NOT HAPPEN, ONLY OR/AND/NONBOOL\n");
	}
}


static void build_cnf_tseytin_or(struct tmp_sat_variable *t, struct fexpr *e)
{
	struct gstr reason = str_new();
	str_append(&reason, "(#): ");
	str_append(&reason, t->sval);
	str_append(&reason, " = ");
	fexpr_as_char_short(e, &reason, -1);
	
	if (fexpr_is_symbol(e->left) && fexpr_is_symbol(e->right)) {
		int a = e->left->satval;
		int b = e->right->satval;
		/* A v B v -T */
		build_cnf_clause(&reason, 3, a, b, -t->satval);
		/* -A v T */
		build_cnf_clause(&reason, 2, -a, t->satval);
		/* -B v T */
		build_cnf_clause(&reason, 2, -b, t->satval);
		
	} else if (!fexpr_is_symbol(e->left) && !fexpr_is_symbol(e->right)) {

		struct tmp_sat_variable *t_a = create_tmp_sat_var(t);
		struct tmp_sat_variable *t_b = create_tmp_sat_var(t);
		/* A v B v -T */
		build_cnf_clause(&reason, 3, t_a->satval, t_b->satval, -t->satval);
		/* -A v T */
		build_cnf_clause(&reason, 2, -t_a->satval, t->satval);
		/* -B v T */
		build_cnf_clause(&reason, 2, -t_b->satval, t->satval);

		build_cnf_tseytin_t(t_a, e->left);
		build_cnf_tseytin_t(t_b, e->right);
	} else {
		int a;
		struct tmp_sat_variable *b = create_tmp_sat_var(t);
		if (fexpr_is_symbol(e->left)) {
			a = e->left->satval;
		} else {
			a = e->right->satval;
		}
		/* A v B v -T */
		build_cnf_clause(&reason, 3, a, b->satval, -t->satval);
		/* -A v T */
		build_cnf_clause(&reason, 2, -a, t->satval);
		/* -B v T */
		build_cnf_clause(&reason, 2, -b->satval, t->satval);

		build_cnf_tseytin_t(b, fexpr_is_symbol(e->left) ? e->right : e->left);
	}
	
}

static void build_cnf_tseytin_and(struct tmp_sat_variable *t, struct fexpr *e)
{
	struct gstr reason = str_new();
	str_append(&reason, "(#): ");
	str_append(&reason, t->sval);
	str_append(&reason, " = ");
	fexpr_as_char_short(e, &reason, -1);
	
	if (fexpr_is_symbol(e->left) && fexpr_is_symbol(e->right)) {
		int a = e->left->satval;
		int b = e->right->satval;
		/* -A v -B v T */
		build_cnf_clause(&reason, 3, -a, -b, t->satval);
		/* A v -T */
		build_cnf_clause(&reason, 2, a, -t->satval);
		/* B v -T */
		build_cnf_clause(&reason, 2, b, -t->satval);
		
	} else if (!fexpr_is_symbol(e->left) && !fexpr_is_symbol(e->right)) {
		struct tmp_sat_variable *t_a = create_tmp_sat_var(t);
		struct tmp_sat_variable *t_b = create_tmp_sat_var(t);
		/* -A v -B v T */
		build_cnf_clause(&reason, 3, -t_a->satval, -t_b->satval, t->satval);
		/* A v -T */
		build_cnf_clause(&reason, 2, t_a->satval, -t->satval);
		/* B v -T */
		build_cnf_clause(&reason, 2, t_b->satval, -t->satval);
		
		build_cnf_tseytin_t(t_a, e->left);
		build_cnf_tseytin_t(t_b, e->right);
	} else {
		int a;
		struct tmp_sat_variable *b = create_tmp_sat_var(t);
		if (fexpr_is_symbol(e->left)) {
			a = e->left->satval;
		} else {
			a = e->right->satval;
		}
		/* -A v -B v T */
		build_cnf_clause(&reason, 3, -a, -b->satval, t->satval);
		/* -A v T */
		build_cnf_clause(&reason, 2, a, -t->satval);
		/* -B v T */
		build_cnf_clause(&reason, 2, b->satval, -t->satval);
		
		build_cnf_tseytin_t(b, fexpr_is_symbol(e->left) ? e->right : e->left);
	}
}

static void build_cnf_tseytin_not(struct tmp_sat_variable *t, struct fexpr *e)
{
	struct gstr reason = str_new();
	str_append(&reason, "(#): ");
	str_append(&reason, t->sval);
	str_append(&reason, " = ");
	fexpr_as_char_short(e, &reason, -1);
	
	if (fexpr_is_symbol(e->left)) {
		int a = e->left->satval;
		/* -A v -T */
		build_cnf_clause(&reason, 2, -a, -t->satval);
		/* A v T */
		build_cnf_clause(&reason, 2, a, t->satval);
	} else {
		assert(fexpr_is_symbol(e->left));
	}
}

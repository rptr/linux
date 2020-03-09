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


// static struct tmp_sat_variable * create_tmp_sat_var(struct tmp_sat_variable *parent);
// static void build_cnf_tseytin(struct fexpr *e);
// static void build_cnf_tseytin_or(struct tmp_sat_variable *t, struct fexpr *e);
// static void build_cnf_tseytin_and(struct tmp_sat_variable *t, struct fexpr *e);
// static void build_cnf_tseytin_not(struct tmp_sat_variable *t, struct fexpr *e);

static bool fexpr_is_cnf(struct fexpr *e);
static void unfold_cnf_clause(struct fexpr *e);
static void build_cnf_tseytin(struct fexpr *e);
static void build_cnf_tseytin_and(struct fexpr *e, struct fexpr *t);
static void build_cnf_tseytin_or(struct fexpr *e, struct fexpr *t);
static void build_cnf_tseytin_not(struct fexpr *e, struct fexpr *t);

static struct fexpr * get_fexpr_tseytin(struct fexpr *e);

/*
 * construct the CNF-clauses from the constraints
 */
void construct_cnf_clauses()
{
	unsigned int i, j;
	struct symbol *sym;
	
// 	printf("Constructing CNF-clauses...");
	
// 	printf("\n");

	for_all_symbols(i, sym) {
		if (sym->type == S_UNKNOWN) continue;
		
// 		print_sym_name(sym);
// 		print_sym_constraint(sym);
		
		struct fexpr *e;
		for (j = 0; j < sym->constraints->arr->len; j++) {
			e = g_array_index(sym->constraints->arr, struct fexpr *, j);
			
			if (fexpr_is_cnf(e)) {
// 				print_fexpr("CNF:", e, -1);
				unfold_cnf_clause(e);
			} else {
// 				print_fexpr("!Not CNF:", e, -1);
				build_cnf_tseytin(e);
			}
// 			convert_fexpr_to_nnf(e);
// 			convert_fexpr_to_cnf(e);
// 			unfold_cnf_clause(e);
		}
	}

// 	printf("done.\n");
}

/*
 * check, if a fexpr is in CNF
 */
static bool fexpr_is_cnf(struct fexpr *e)
{
	if (!e) return false;
	
	switch (e->type) {
	case FE_SYMBOL:
	case FE_TRUE:
	case FE_FALSE:
	case FE_NONBOOL:
	case FE_CHOICE:
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

/*
 * helper function to add an expression to a CNF-clause
 */
static void unfold_cnf_clause_util(struct cnf_clause *cl, struct fexpr *e)
{
	switch (e->type) {
	case FE_SYMBOL:
	case FE_TRUE:
	case FE_FALSE:
	case FE_NONBOOL:
	case FE_CHOICE:
		add_literal_to_clause(cl, e->satval);
		break;
	case FE_OR:
		unfold_cnf_clause_util(cl, e->left);
		unfold_cnf_clause_util(cl, e->right);
		break;
	case FE_NOT:
		add_literal_to_clause(cl, -(e->left->satval));
		break;
	default:
		perror("Not in CNF, FE_EQUALS.");
	}
}

/*
 * extract the variables from a fexpr in CNF
 */
static void unfold_cnf_clause(struct fexpr *e)
{
	assert(fexpr_is_cnf(e));
	
	struct gstr empty_string = str_new();
	struct cnf_clause *cl = build_cnf_clause(&empty_string, 0);
	
	unfold_cnf_clause_util(cl, e);
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
	assert(abs(val) == e->satval);
	
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
 * build CNF-clauses for a fexpr not in CNF
 */
static void build_cnf_tseytin(struct fexpr *e)
{
	assert(!fexpr_is_cnf(e));
	
// 	print_fexpr("e:", e, -1);
	struct fexpr *t;
	switch (e->type) {
	case FE_AND:
// 		printf("TYPE AND\n");
		t = create_tmpsatvar();
		build_cnf_tseytin_and(e, t);
		break;
	case FE_OR:
// 		printf("TYPE OR\n");
		t = create_tmpsatvar();
		build_cnf_tseytin_or(e, t);
		break;
	case FE_NOT:
// 		printf("TYPE NOT\n");
// 		print_fexpr("e:", e, -1);
		t = create_tmpsatvar();
		build_cnf_tseytin_not(e, t);
		break;
	default:
		perror("Expression not a propositional logic formula.");
	}
}

/*
 * build CNF-clauses for a fexpr of type AND
 */
static void build_cnf_tseytin_and(struct fexpr *e, struct fexpr *t)
{
	assert(e->type == FE_AND);
	
	struct fexpr *left = get_fexpr_tseytin(e->left);
	struct fexpr *right = get_fexpr_tseytin(e->right);
	
// 	print_fexpr("Left:", left, -1);
// 	print_fexpr("Right:", right, -1);
	
	int a = left->type == FE_NOT ? -(left->left->satval) : left->satval;
	int b = right->type == FE_NOT ? -(right->left->satval) : right->satval;
	int c = t->satval;
	struct gstr empty_string = str_new();
	
	/* -A v -B v C */
	build_cnf_clause(&empty_string, 3, -a,-b, c);
	/* A v -C */
	build_cnf_clause(&empty_string, 2, a, -c);
	/* B v -C */
	build_cnf_clause(&empty_string, 2, b , -c);
}

/*
 * build CNF-clauses for a fexpr of type OR
 */
static void build_cnf_tseytin_or(struct fexpr *e, struct fexpr *t)
{
	assert(e->type == FE_OR);

	struct fexpr *left = get_fexpr_tseytin(e->left);
	struct fexpr *right = get_fexpr_tseytin(e->right);
	
// 	print_fexpr("Left:", left, -1);
// 	print_fexpr("Right:", right, -1);
	
	int a = left->type == FE_NOT ? -(left->left->satval) : left->satval;
	int b = right->type == FE_NOT ? -(right->left->satval) : right->satval;
	int c = t->satval;
	struct gstr empty_string = str_new();
	
	/* A v B v -C */
	build_cnf_clause(&empty_string, 3, a, b, -c);
	/* -A v C */
	build_cnf_clause(&empty_string, 2, -a, c);
	/* -B v C */
	build_cnf_clause(&empty_string, 2, -b , c);
}

/*
 * build CNF-clauses for a fexpr of type NOT
 */
static void build_cnf_tseytin_not(struct fexpr *e, struct fexpr *t)
{
	assert(e->type == FE_NOT);

	struct fexpr *left = get_fexpr_tseytin(e->left);
	
// 	print_fexpr("Left:", left, -1);
	
	int a = left->type == FE_NOT ? -(left->left->satval) : left->satval;
	int c = t->satval;
	struct gstr empty_string = str_new();
	
	/* -A v -C */
	build_cnf_clause(&empty_string, 2, -a, -c);
	/* A v C */
	build_cnf_clause(&empty_string, 2, a, c);
}


static struct fexpr * get_fexpr_tseytin_not(struct fexpr *e)
{
	if (fexpr_is_symbol_or_neg_atom(e))
		return e;
	
	struct fexpr *t = create_tmpsatvar();
	build_cnf_tseytin_not(e, t);
	
	return t;
}

static struct fexpr * get_fexpr_tseytin(struct fexpr *e)
{
	if (!e) perror("Empty fexpr.");
	
// 	printf("e type: %d\n", e->type);
	
	struct fexpr *t;
	switch (e->type) {
	case FE_SYMBOL:
	case FE_TRUE:
	case FE_FALSE:
	case FE_NONBOOL:
	case FE_CHOICE:
		return e;
	case FE_AND:
		t = create_tmpsatvar();
		build_cnf_tseytin_and(e, t);
		return t;
	case FE_OR:
		t = create_tmpsatvar();
		build_cnf_tseytin_or(e, t);
		return t;
	case FE_NOT:
		return get_fexpr_tseytin_not(e);
	default:
		perror("Not in CNF, FE_EQUALS.");
		return false;
	}
}



/*
 * check, if a CNF-clause is a tautology
 */
bool cnf_is_tautology(struct cnf_clause *cl)
{
	struct cnf_literal *lit, *lit2;
	unsigned int i, j;
	int curr;
	for (i = 0; i < cl->lits->len; i++) {
		lit = g_array_index(cl->lits, struct cnf_literal *, i);
		
		/* exception for the 2 unit clauses for the True/False constants */
		if (cl->lits->len == 1 && (lit->val == const_true->satval || lit->val == -(const_false->satval))) 
			return false;
		
		/* clause is tautology, if a constant evaluates to true */
		if (lit->val == -(const_false->satval) || lit->val == const_true->satval)
			return true;
		
		/* given X, check if -X is in the clause as well */
		// TODO
		curr = lit->val;
		for (j = i + 1; j < cl->lits->len; j++) {
			lit2 = g_array_index(cl->lits, struct cnf_literal *, j);
			
			if (curr == -(lit2->val)) return true;
		}
	}
	
	return false;
}

// static void build_cnf(struct fexpr *e)
// {
// 	if (!e)
// 		return;
// 	
// 	switch (e->type) {
// 	case FE_SYMBOL:
// 		break;
// 	case FE_OR:
// 		if (fexpr_is_symbol_or_not(e->left) && fexpr_is_symbol_or_not(e->right)) {
// 			int a = e->left->type == FE_NOT ? -e->left->left->satval : e->left->satval;
// 			int b = e->right->type == FE_NOT ? -e->right->left->satval : e->right->satval;
// 			struct gstr reason = str_new();
// 			fexpr_as_char_short(e, &reason, -1);
// 			build_cnf_clause(&reason, 2, a, b);
// 		} else {
// 			build_cnf_tseytin_or(create_tmp_sat_var(NULL), e);
// 		}
// 		break;
// 	case FE_AND:
// 		if (fexpr_is_symbol(e->left) && fexpr_is_symbol(e->right)) {
// 			int a = e->left->type == FE_NOT ? -e->left->left->satval : e->left->satval;
// 			int b = e->right->type == FE_NOT ? -e->right->left->satval : e->right->satval;
// 			struct gstr reason = str_new();
// 			fexpr_as_char_short(e, &reason, -1);
// 			build_cnf_clause(&reason, 1, a);
// 			build_cnf_clause(&reason, 1, b);
// 		} else {
// 			build_cnf_tseytin_and(create_tmp_sat_var(NULL), e);
// 		}
// 	default:
// 		break;
// 	}
// 	
// }
// 
// /*
//  * create a struct for a temporary SAT variable
//  */
// static struct tmp_sat_variable * create_tmp_sat_var(struct tmp_sat_variable *parent)
// {
// 	struct tmp_sat_variable *tvar = malloc(sizeof(struct tmp_sat_variable));
// 	tvar->nr = tmp_variable_nr++;
// 	tvar->satval = sat_variable_nr++;
// 	strcpy(tvar->sval, get_tmp_var_as_char(tvar->nr));
// 	tvar->parent = parent;
// 	tvar->next = tmp_sat_vars;
// 	tmp_sat_vars = tvar;
// 	
// 	TODO
// 	struct gstr tstr = str_new();
// 	str_append(&tstr, tvar->sval);
// 	/* add it to the satmap */
// 	g_hash_table_insert(satmap, &tvar->satval, strdup(str_get(&tstr)));
// 	
// 	return tvar;
// }
// 
// static void build_cnf_tseytin_t(struct tmp_sat_variable *t, struct fexpr *e)
// {
// 	if (!e || t == NULL)
// 		return;
// 	
// 	switch (e->type) {
// 	case FE_OR:
// 		build_cnf_tseytin_or(t, e);
// 		break;
// 	case FE_AND:
// 		build_cnf_tseytin_and(t, e);
// 		break;
// 	case FE_NOT:
// 		build_cnf_tseytin_not(t, e);
// 		break;
// 	default:
// 		printf("SHOULD NOT HAPPEN, ONLY OR/AND/NONBOOL\n");
// 	}
// }
// 
// 
// static void build_cnf_tseytin_or(struct tmp_sat_variable *t, struct fexpr *e)
// {
// 	struct gstr reason = str_new();
// 	str_append(&reason, "(#): ");
// 	str_append(&reason, t->sval);
// 	str_append(&reason, " = ");
// 	fexpr_as_char_short(e, &reason, -1);
// 	
// 	if (fexpr_is_symbol(e->left) && fexpr_is_symbol(e->right)) {
// 		int a = e->left->satval;
// 		int b = e->right->satval;
// 		/* A v B v -T */
// 		build_cnf_clause(&reason, 3, a, b, -t->satval);
// 		/* -A v T */
// 		build_cnf_clause(&reason, 2, -a, t->satval);
// 		/* -B v T */
// 		build_cnf_clause(&reason, 2, -b, t->satval);
// 		
// 	} else if (!fexpr_is_symbol(e->left) && !fexpr_is_symbol(e->right)) {
// 
// 		struct tmp_sat_variable *t_a = create_tmp_sat_var(t);
// 		struct tmp_sat_variable *t_b = create_tmp_sat_var(t);
// 		/* A v B v -T */
// 		build_cnf_clause(&reason, 3, t_a->satval, t_b->satval, -t->satval);
// 		/* -A v T */
// 		build_cnf_clause(&reason, 2, -t_a->satval, t->satval);
// 		/* -B v T */
// 		build_cnf_clause(&reason, 2, -t_b->satval, t->satval);
// 
// 		build_cnf_tseytin_t(t_a, e->left);
// 		build_cnf_tseytin_t(t_b, e->right);
// 	} else {
// 		int a;
// 		struct tmp_sat_variable *b = create_tmp_sat_var(t);
// 		if (fexpr_is_symbol(e->left)) {
// 			a = e->left->satval;
// 		} else {
// 			a = e->right->satval;
// 		}
// 		/* A v B v -T */
// 		build_cnf_clause(&reason, 3, a, b->satval, -t->satval);
// 		/* -A v T */
// 		build_cnf_clause(&reason, 2, -a, t->satval);
// 		/* -B v T */
// 		build_cnf_clause(&reason, 2, -b->satval, t->satval);
// 
// 		build_cnf_tseytin_t(b, fexpr_is_symbol(e->left) ? e->right : e->left);
// 	}
// 	
// }
// 
// static void build_cnf_tseytin_and(struct tmp_sat_variable *t, struct fexpr *e)
// {
// 	struct gstr reason = str_new();
// 	str_append(&reason, "(#): ");
// 	str_append(&reason, t->sval);
// 	str_append(&reason, " = ");
// 	fexpr_as_char_short(e, &reason, -1);
// 	
// 	if (fexpr_is_symbol(e->left) && fexpr_is_symbol(e->right)) {
// 		int a = e->left->satval;
// 		int b = e->right->satval;
// 		/* -A v -B v T */
// 		build_cnf_clause(&reason, 3, -a, -b, t->satval);
// 		/* A v -T */
// 		build_cnf_clause(&reason, 2, a, -t->satval);
// 		/* B v -T */
// 		build_cnf_clause(&reason, 2, b, -t->satval);
// 		
// 	} else if (!fexpr_is_symbol(e->left) && !fexpr_is_symbol(e->right)) {
// 		struct tmp_sat_variable *t_a = create_tmp_sat_var(t);
// 		struct tmp_sat_variable *t_b = create_tmp_sat_var(t);
// 		/* -A v -B v T */
// 		build_cnf_clause(&reason, 3, -t_a->satval, -t_b->satval, t->satval);
// 		/* A v -T */
// 		build_cnf_clause(&reason, 2, t_a->satval, -t->satval);
// 		/* B v -T */
// 		build_cnf_clause(&reason, 2, t_b->satval, -t->satval);
// 		
// 		build_cnf_tseytin_t(t_a, e->left);
// 		build_cnf_tseytin_t(t_b, e->right);
// 	} else {
// 		int a;
// 		struct tmp_sat_variable *b = create_tmp_sat_var(t);
// 		if (fexpr_is_symbol(e->left)) {
// 			a = e->left->satval;
// 		} else {
// 			a = e->right->satval;
// 		}
// 		/* -A v -B v T */
// 		build_cnf_clause(&reason, 3, -a, -b->satval, t->satval);
// 		/* -A v T */
// 		build_cnf_clause(&reason, 2, a, -t->satval);
// 		/* -B v T */
// 		build_cnf_clause(&reason, 2, b->satval, -t->satval);
// 		
// 		build_cnf_tseytin_t(b, fexpr_is_symbol(e->left) ? e->right : e->left);
// 	}
// }
// 
// static void build_cnf_tseytin_not(struct tmp_sat_variable *t, struct fexpr *e)
// {
// 	struct gstr reason = str_new();
// 	str_append(&reason, "(#): ");
// 	str_append(&reason, t->sval);
// 	str_append(&reason, " = ");
// 	fexpr_as_char_short(e, &reason, -1);
// 	
// 	if (fexpr_is_symbol(e->left)) {
// 		int a = e->left->satval;
// 		/* -A v -T */
// 		build_cnf_clause(&reason, 2, -a, -t->satval);
// 		/* A v T */
// 		build_cnf_clause(&reason, 2, a, t->satval);
// 	} else {
// 		assert(fexpr_is_symbol(e->left));
// 	}
// }

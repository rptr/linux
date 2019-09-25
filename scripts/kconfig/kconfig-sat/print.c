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
#include "print.h"
#include "fexpr.h"
#include "utils.h"

/*
 * print all symbols, just for debugging
 */
void print_all_symbols(void)
{
	unsigned int i;
	struct symbol *sym;
	
	printf("\n");
	
	for_all_symbols(i, sym) {
		if (sym_get_type(sym) == S_UNKNOWN)
			continue;
		
		print_symbol(sym);

		if (sym->constraints->arr->len > 0) {
			printf("Constraints:\n");
			print_sym_constraint(sym);
			printf("\n");
		}
	}
}

void print_symbol(struct symbol *sym)
{
	printf("Symbol: ");
	struct property *p;

	if (sym_is_boolean(sym)) {
		printf("name %s, type %s, fexpr_y %d", sym->name, sym_type_name(sym->type), sym->fexpr_y->satval);
		if (sym_get_type(sym) == S_TRISTATE)
			printf(", fexpr_m %d", sym->fexpr_m->satval);
	} else {
		printf("name %s, type %s", sym->name, sym_type_name(sym->type));
	}
	printf("\n");

	printf("\t.config: %s\n", sym_get_string_value(sym));
	

	/* print default values */
	for_all_defaults(sym, p)
		print_default(sym, p);

	/* print select statements */
	for_all_properties(sym, p, P_SELECT)
		print_select(sym, p);

	/* print reverse dependencies */
	if (sym->rev_dep.expr) {
		printf("\tselected if ");
		print_expr(sym->rev_dep.expr, E_NONE);
		printf("  (reverse dep.)\n");
	}

	/* print imply statements */
	for_all_properties(sym, p, P_IMPLY)
		print_imply(sym, p);

	/* print weak reverse denpencies */
	if (sym->implied.expr) {
		printf("\timplied if ");
		print_expr(sym->implied.expr, E_NONE);
		printf("  (weak reverse dep.)\n");
	}

	/* print dependencies */
	if (sym->dir_dep.expr) {
		printf("\tdepends on ");
		print_expr(sym->dir_dep.expr, E_NONE);
		printf("\n");
	}

	printf("\n");

}

/*
 * print a default value for a symbol
 */
void print_default(struct symbol *sym, struct property *p)
{
	assert(p->type == P_DEFAULT);
	printf("\tdefault %u", p->expr->left.sym->curr.tri);
	if (p->visible.expr) {
		printf(" if ");
		print_expr(p->visible.expr, E_NONE);
	}
	printf("\n");
}

/*
 * print a select statement for a symbol
 */
void print_select(struct symbol *sym, struct property *p)
{
	assert(p->type == P_SELECT);
	struct expr *e = p->expr;

	printf("\tselect ");
	print_expr(e, E_NONE);
	if (p->visible.expr) {
		printf(" if ");
		print_expr(p->visible.expr, E_NONE);
	}
	printf("\n");
}

/*
 * print an imply statement for a symbol
 */
void print_imply(struct symbol *sym, struct property *p)
{
	assert(p->type == P_IMPLY);
	struct expr *e = p->expr;

	printf("\timply ");
	print_expr(e, E_NONE);
	if (p->visible.expr) {
		printf(" if ");
		print_expr(p->visible.expr, E_NONE);
	}
	printf("\n");
}

/*
 * print an expr
 */
void print_expr(struct expr *e, int prevtoken)
{
	if (!e)
		return;

	switch (e->type) {
	case E_SYMBOL:
		if (e->left.sym->name)
			printf("%s", e->left.sym->name);
		else
			printf("left was null\n");
		break;
	case E_NOT:
		printf("!");
		print_expr(e->left.expr, E_NOT);
		break;
	case E_AND:
		printf("(");
		print_expr(e->left.expr, E_AND);
		printf(" && ");
		print_expr(e->right.expr, E_AND);
		printf(")");
		break;
	case E_OR:
		printf("(");
		print_expr(e->left.expr, E_OR);
		printf(" || ");
		print_expr(e->right.expr, E_OR);
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

/*
 * print a kexpr
 */
void print_kexpr(struct k_expr *e)
{
	if (!e)
		return;

	switch (e->type) {
	case KE_SYMBOL:
		printf("%s", e->sym->name);
		if (e->tristate == mod)
			printf("_m");
		break;
	case KE_AND:
		printf("(");
		print_kexpr(e->left);
		printf(" && ");
		print_kexpr(e->right);
		printf(")");
		break;
	case KE_OR:
		printf("(");
		print_kexpr(e->left);
		printf(" || ");
		print_kexpr(e->right);
		printf(")");
		break;
	case KE_NOT:
		printf("!");
		print_kexpr(e->left);
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

/*
 * print a fexpr
 */
void print_fexpr(struct fexpr *e, int parent)
{
	if (!e)
		return;
	
	switch (e->type) {
	case FE_SYMBOL:
		printf("%s", str_get(&e->name));
		break;
	case FE_AND:
		/* need this hack for the FeatureExpr parser */
		if (parent != FE_AND)
			printf("(");
		print_fexpr(e->left, FE_AND);
		printf(" && ");
		print_fexpr(e->right, FE_AND);
		if (parent != FE_AND)
			printf(")");
		break;
	case FE_OR:
		printf("(");
		print_fexpr(e->left, FE_OR);
		printf(" || ");
		print_fexpr(e->right, FE_OR);
		printf(")");
		break;
	case FE_NOT:
		printf("!");
		print_fexpr(e->left, FE_NOT);
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
	case FE_NONBOOL:
		printf("%s", str_get(&e->name));
		break;
	}
}

/*
 * print some debug info about the tree structure of k_expr
 */
void debug_print_kexpr(struct k_expr *e)
{
	if (!e)
		return;
	
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
	if (!e)
		return;
	
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
 * write a fexpr into a string (format needed for testing)
 */
void fexpr_as_char(struct fexpr *e, struct gstr *s, int parent)
{
	if (!e)
		return;
	
	switch (e->type) {
	case FE_SYMBOL:
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

void fexpr_as_char_short(struct fexpr *e, struct gstr *s, int parent)
{
	if (!e)
		return;
	
	switch (e->type) {
	case FE_SYMBOL:
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
 * print a CNF clause
 */
void print_cnf_clause(struct cnf_clause *cl)
{
	int i;
	struct cnf_literal *lit;
	printf("\t");
	for (i = 0; i < cl->lits->len; i++) {
		lit = g_array_index(cl->lits, struct cnf_literal *, i);
		printf("%s", str_get(&lit->name));
		if (i < cl->lits->len - 1)
			printf(" v ");
	}
	
	#if PRINT_CNF_REASONS
		printf("\t");
		printf("%s", str_get(&cl->reason));
		printf(" ");
	#endif
	printf("\n");
}

/*
 * print all CNF clauses
 */
void print_all_cnf_clauses(GArray *cnf_clauses)
{
	int i;
	struct cnf_clause *cl;
	for (i = 0; i < cnf_clauses->len; i++) {
		cl = g_array_index(cnf_clauses, struct cnf_clause *, i);
		print_cnf_clause(cl);
	}
}

/*
 * print all constraints for a symbol
 */
void print_sym_constraint(struct symbol* sym)
{
	struct fexpr *e;
	int i;
	for (i = 0; i < sym->constraints->arr->len; i++) {
		e = g_array_index(sym->constraints->arr, struct fexpr *, i);
		print_fexpr(e, -1);
		#if PRINT_ALL_CNF
			printf("\nCNF: ");
			convert_fexpr_to_cnf(e);
			print_fexpr(e, -1);
		#endif
		printf("\n");
	}
}

/*
 * print the satmap
 */
void print_satmap(gpointer key, gpointer value, gpointer userData)
{
	struct fexpr *e = (struct fexpr *) value;
	printf( "%d => %s\n", *((int *) key), str_get(&e->name));
}

/*
 * print all variables from the satmap in DIMACS-format
 */
static void print_satmap_dimacs(gpointer key, gpointer value, gpointer fd)
{
	fprintf((FILE *) fd, "c %d %s\n", *((int *) key), (char *) value);
}


/*
 * write the CNF-clauses into a file in DIMACS-format
 */
void write_cnf_to_file(GArray *cnf_clauses, int sat_variable_nr, int nr_of_clauses)
{
	FILE *fd = fopen(OUTFILE_DIMACS, "w");

	/* symbols, constants & tmp vars */
	g_hash_table_foreach(satmap, print_satmap_dimacs, fd);

	/* stats */
	fprintf(fd, "p cnf %d %d\n", sat_variable_nr - 1, nr_of_clauses);
	
	/* clauses */
	int i, j;
	struct cnf_clause *cl;
	struct cnf_literal *lit;
	for (i = 0; i < cnf_clauses->len; i++) {
		cl = g_array_index(cnf_clauses, struct cnf_clause *, i);
		for (j = 0; j < cl->lits->len; j++) {
			lit = g_array_index(cl->lits, struct cnf_literal *, j);
			fprintf(fd, "%d ", lit->val);
			if (j == cl->lits->len - 1)
				fprintf(fd, "0\n");
		}
	}

	fclose(fd);
}

/*
 * write all constraints into a file for testing purposes
 */
void write_constraints_to_file(void)
{
	FILE *fd = fopen(OUTFILE_FEXPR, "w");
	unsigned int i;
	struct symbol *sym;

	for_all_symbols(i, sym) {
		if (sym_get_type(sym) == S_UNKNOWN) continue;
		
		struct fexpr *e;
		int j;
		for (j = 0; j < sym->constraints->arr->len; j++) {
			e = g_array_index(sym->constraints->arr, struct fexpr *, j);
			struct gstr s = str_new();
			fexpr_as_char(e, &s, -1);
			fprintf(fd, "%s\n", str_get(&s));
		}
	}
	fclose(fd);
}

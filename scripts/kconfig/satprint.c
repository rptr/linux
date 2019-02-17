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

#define LKC_DIRECT_LINK
#include "lkc.h"
#include "satconfig.h"
#include "satprint.h"

const char *symbol_type[] = {"S_UNKNOWN", "S_BOOLEAN", "S_TRISTATE", "S_INT", "S_HEX", "S_STRING", "S_OTHER"};

void print_symbol(struct symbol *sym)
{
	printf("Symbol: ");
	struct property *p;

	printf("name %s, type %s, sat variable %d\n", sym->name, symbol_type[sym->type], sym->sat_variable_nr);

	printf("\t.config: %s\n", sym_get_string_value(sym));

	/* print default value */
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

void print_default(struct symbol *sym, struct property *p)
{
	assert(p->type == P_DEFAULT);
	printf("\tdefault %s", sym_get_string_default(sym));
	if (p->visible.expr) {
		printf(" if ");
		print_expr(p->visible.expr, E_NONE);
	}
	printf("\n");
}

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

void print_kexpr(struct k_expr *e)
{
	if (!e)
		return;

	switch (e->type) {
	case KE_SYMBOL:
		printf("%s", e->sym->name);
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
		print_kexpr(e->child);
		break;
	}
}


void print_cnf_clauses(struct cnf_clause *cnf_clauses)
{
	struct cnf_clause *cl = cnf_clauses;
	while (cl != NULL) {
		struct cnf_literal *lit = cl->lit;
		printf("\t");
		while (lit != NULL) {
			printf("%s", lit->sval);
			if (lit->next)
				printf(" v ");
			lit = lit->next;
		}
		#if PRINT_CNF_REASONS
			printf("\t");
			printf("%s", cl->reason);
			printf(" ");
		#endif
		printf("\n");
		cl = cl->next;
	}
}

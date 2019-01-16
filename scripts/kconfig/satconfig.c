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

static int sat_variable_nr = 1;
static struct symbol *sat_map;

static void create_sat_variables(struct symbol *sym);

static void print_symbol(struct symbol *sym);
static void print_default(struct symbol *sym, struct property *p);

static void print_select(struct symbol *sym, struct property *p);
static void build_cnf_select(struct symbol *sym, struct property *p);

static void print_imply(struct symbol *sym, struct property *p);
static void print_expr(struct expr *e, int prevtoken);

static void print_cnf(struct symbol *sym);


const char *expr_type[] = {
	"E_NONE", "E_OR", "E_AND", "E_NOT",
	"E_EQUAL", "E_UNEQUAL", "E_LTH", "E_LEQ", "E_GTH", "E_GEQ",
	"E_LIST", "E_SYMBOL", "E_RANGE"
};
const char *prop_type[] = { "P_UNKNOWN",
	"P_PROMPT",   /* prompt "foo prompt" or "BAZ Value" */
	"P_COMMENT",  /* text associated with a comment */
	"P_MENU",     /* prompt associated with a menu or menuconfig symbol */
	"P_DEFAULT",  /* default y */
	"P_CHOICE",   /* choice value */
	"P_SELECT",   /* select BAR */
	"P_IMPLY",    /* imply BAR */
	"P_RANGE",    /* range 7..100 (for a symbol) */
	"P_SYMBOL"    /* where a symbol is defined */
};
const char *symbol_type[] = {"S_UNKNOWN", "S_BOOLEAN", "S_TRISTATE", "S_INT", "S_HEX", "S_STRING", "S_OTHER"};
const char *tristate_type[] = {"no", "mod", "yes"};

int main(int argc, char *argv[])
{
	printf("\nHello satconfig!\n\n");
	
	// parse Kconfig-file & read .config
	const char *Kconfig_file = argv[1];
	conf_parse(Kconfig_file);
	conf_read(NULL);

	unsigned int i;
	struct symbol *sym;
	
	// create sat_map
	for_all_symbols(i, sym)
		create_sat_variables(sym);
	
	sat_map = calloc(sat_variable_nr, sizeof(struct symbol));

	// print all symbols
	for_all_symbols(i, sym) {
		sat_map[sym->sat_variable_nr] = *sym;
		print_symbol(sym);
	}
	
	// print all CNFs
	printf("All CNFs:\n");
	for_all_symbols(i, sym)
		print_cnf(sym);
	
	return EXIT_SUCCESS;
}

static void create_sat_variables(struct symbol *sym)
{
	switch (sym->type) {
	case S_BOOLEAN:
		sym->sat_variable_nr = sat_variable_nr;
		sat_variable_nr += 1;
		break;
	case S_TRISTATE:
		sym->sat_variable_nr = sat_variable_nr;
		sat_variable_nr += 2;
		break;
	default:
		break;
	}
}

static void print_symbol(struct symbol *sym)
{
	printf("Symbol: ");
	struct property *p;
	
	printf("name %s, type %s, sat variable %d\n", sym->name, symbol_type[sym->type], sym->sat_variable_nr);
	
	printf("\t.config: %s\n", sym_get_string_value(sym));
	
	// print default value
	for_all_defaults(sym, p)
		print_default(sym, p);
	
	// print select statements
	for_all_properties(sym, p, P_SELECT)
		print_select(sym, p);
	
	// print reverse dependencies
	if (sym->rev_dep.expr) {
		printf("\tselected if ");
		print_expr(sym->rev_dep.expr, E_NONE);
		printf("  (reverse dep.)\n");
	}
	
	// print imply statements
	for_all_properties(sym, p, P_IMPLY)
		print_imply(sym, p);
	
	// print weak reverse denpencies
	if (sym->implied.expr) {
		printf("\timplied if ");
		print_expr(sym->implied.expr, E_NONE);
		printf("  (weak reverse dep.)\n");
	}
	
	// print dependencies
	if (sym->dir_dep.expr) {
		printf("\tdepends on ");
		print_expr(sym->dir_dep.expr, E_NONE);
		printf("\n");
	}
	
	// build CNF clauses for select statements
	for_all_properties(sym, p, P_SELECT)
		build_cnf_select(sym, p);
	
	// print CNF-clauses
	if (sym->clauses) {
		printf("CNF:");
		print_cnf(sym);
	}

	printf("\n");

}

static void print_default(struct symbol *sym, struct property *p)
{
	assert(p->type == P_DEFAULT);
	printf("\tdefault %s", sym_get_string_default(sym));
	if (p->visible.expr) {
		printf(" if ");
		print_expr(p->visible.expr, E_NONE);
	}
	printf("\n");
}

static void print_select(struct symbol *sym, struct property *p)
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

static void build_cnf_select(struct symbol *sym, struct property *p)
{
	assert(p->type == P_SELECT);
	struct expr *e = p->expr;
	
	struct cnf_clause *cl = malloc(sizeof(struct cnf_clause));
	
	struct cnf_literal *lit1 = malloc(sizeof(struct cnf_literal));
	lit1->val = -(sym->sat_variable_nr);
	strcpy(lit1->sval, "-");
	strcat(lit1->sval, sym->name);
	
	struct cnf_literal *lit2 = malloc(sizeof(struct cnf_literal));
	lit2->val = e->left.sym->sat_variable_nr;
	strcpy(lit2->sval, e->left.sym->name);
	
	lit1->next = lit2;
	cl->lit = lit1;
	
	cl->next = sym->clauses;

	sym->clauses = cl;
}


static void print_imply(struct symbol *sym, struct property *p)
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

static void print_expr(struct expr *e, int prevtoken)
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

static void print_cnf(struct symbol *sym)
{
	struct cnf_clause *c = sym->clauses;
	while (c != NULL) {
		struct cnf_literal *lit = c->lit;
		printf("\t");
		while (lit != NULL) {
			printf("%s", lit->sval);
			if (lit->next)
				printf(" v ");
			lit = lit->next;
		}
		printf("\n");
		
		c = c->next;
	}
}


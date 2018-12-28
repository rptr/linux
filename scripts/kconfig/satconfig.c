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

void print_symbol(struct symbol *sym);
void print_default(struct symbol *sym, struct property *p);
void print_select(struct symbol *sym, struct property *p);
void print_imply(struct symbol *sym, struct property *p);
void print_expr(struct expr *e, int prevtoken);

int main(int argc, char *argv[])
{
	printf("\nHello satconfig!\n\n");
	
	// parse Kconfig-file
	const char *Kconfig_file = "Kconfig";
	conf_parse(Kconfig_file);

	unsigned int i;
	struct symbol *sym;
	for_all_symbols(i, sym) {
		print_symbol(sym);
	}
	
	return EXIT_SUCCESS;
}

void print_symbol(struct symbol* sym)
{
	printf("Symbol: ");
	struct property *p;
	
	for_all_prompts(sym, p) {
		printf("prompt %s\n", p->text);
	}
	
	printf("\tname %s, type %s\n", sym->name, symbol_type[sym->type]);
	
	int has_default = false;
	for_all_defaults(sym, p) {
		has_default = true;
		print_default(sym, p);
	}
	if (!has_default)
		printf("\tno default\n");
	
	for_all_properties(sym, p, P_SELECT)
		print_select(sym, p);
	
	for_all_properties(sym, p, P_IMPLY)
		print_imply(sym, p);
}

void print_default(struct symbol* sym, struct property* p)
{
	assert(p->type == P_DEFAULT);
	struct expr *e = p->expr;
	printf("\tdefault ");
	print_expr(e, E_NONE);
	if (p->visible.expr) {
		printf(" if ");
		print_expr(p->visible.expr, E_NONE);
	}
	printf("\n");
}


void print_select(struct symbol* sym, struct property* p)
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

void print_imply(struct symbol* sym, struct property* p)
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

void print_expr(struct expr* e, int prevtoken)
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
	default:
		break;
	}
	
}

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

#define OUTFILE_DIMACS "out_cnf.dimacs"

static int sat_variable_nr = 1;
static struct symbol *sat_map;
static struct cnf_clause *cnf_clauses;
static int nr_of_clauses = 0;

static void create_sat_variables(struct symbol *sym);
static void create_tristate_constraint_clause(struct symbol *sym);

static void build_cnf_select(struct symbol *sym, struct property *p);

static void write_to_file(void);


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

const char *tristate_type[] = {"no", "mod", "yes"};

int main(int argc, char *argv[])
{
	printf("\nHello satconfig!\n\n");
	
	/* parse Kconfig-file & read .config */
	const char *Kconfig_file = argv[1];
	conf_parse(Kconfig_file);
	conf_read(NULL);

	unsigned int i;
	struct symbol *sym;
	
	/* create sat_map & tristate clauses */
	for_all_symbols(i, sym)
		create_sat_variables(sym);
	
	sat_map = calloc(sat_variable_nr, sizeof(struct symbol));

	for_all_symbols(i, sym) {
		sat_map[sym->sat_variable_nr] = *sym;
		if (sym->type == S_TRISTATE)
			create_tristate_constraint_clause(sym);
	}

	/* print all symbols */
	for_all_symbols(i, sym) {
		print_symbol(sym);
		
		/* build CNF clauses for select statements */
		struct property *p;
		for_all_properties(sym, p, P_SELECT)
			build_cnf_select(sym, p);
	}
		
	/* print all CNFs */
	printf("All CNFs:\n");
	print_cnf_clauses(cnf_clauses);
	
	write_to_file();
	
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

/*
 * Enforce tristate constraints
 * n -> (0,0)
 * y -> (1,0)
 * m -> (0,1)
 * (1,1) is not allowed, which translates to (-X v -X_m)
 */
static void create_tristate_constraint_clause(struct symbol *sym)
{
	assert(sym->type == S_TRISTATE);
	
	struct cnf_clause *cl = malloc(sizeof(struct cnf_clause));
	
	struct cnf_literal *lit1 = malloc(sizeof(struct cnf_literal));
	lit1->val = -(sym->sat_variable_nr );
	strcpy(lit1->sval, "-");
	strcat(lit1->sval, sym->name);
	
	struct cnf_literal *lit2 = malloc(sizeof(struct cnf_literal));
	lit2->val = -(sym->sat_variable_nr + 1);
	strcpy(lit2->sval, "-");
	strcat(lit2->sval, sym->name);
	strcat(lit2->sval, "_m");
	
	lit1->next = lit2;
	cl->lit = lit1;
	
	cl->next = cnf_clauses;
	cnf_clauses = cl;
	
	nr_of_clauses++;
}


/*
 * A_bool select B_tri translates to
 * (-A v B) -- already done
 * (-B v -B_m) -- already done
 * (-A v B v B_m)
 * if mod == 1, then it's the module variable of a
 */
static void build_cnf_select_bool_tri(struct symbol *a, struct symbol *b, int mod)
{
	assert((a->type == S_BOOLEAN && mod == 0) || a->type == S_TRISTATE);
	assert(b->type == S_TRISTATE);
	
	struct cnf_clause *cl = malloc(sizeof(struct cnf_clause));
	
	struct cnf_literal *lit1 = malloc(sizeof(struct cnf_literal));
	lit1->val = mod == 0 ? -(a->sat_variable_nr) : -(a->sat_variable_nr + 1);
	strcpy(lit1->sval, "-");
	strcat(lit1->sval, a->name);
	if (mod == 1)
		strcat(lit1->sval, "_m");
	
	struct cnf_literal *lit2 = malloc(sizeof(struct cnf_literal));
	lit2->val = b->sat_variable_nr;
	strcpy(lit2->sval, b->name);
	
	struct cnf_literal *lit3 = malloc(sizeof(struct cnf_literal));
	lit3->val = b->sat_variable_nr + 1;
	strcpy(lit3->sval, b->name);
	strcat(lit3->sval, "_m");
	
	lit2->next = lit3;
	lit1->next = lit2;
	cl->lit = lit1;
	
	cl->next = cnf_clauses;
	cnf_clauses = cl;
	
	nr_of_clauses++;
}

/*
 * A_tri select B_bool translates to
 * (-A v B) -- already done
 * (-A v -A_m) -- already done
 * (-A_m v B)
 */
static void build_cnf_select_tri_bool(struct symbol *a, struct symbol *b)
{
	assert(a->type == S_TRISTATE);
	assert(b->type == S_BOOLEAN);
	
	struct cnf_clause *cl = malloc(sizeof(struct cnf_clause));
	
	struct cnf_literal *lit1 = malloc(sizeof(struct cnf_literal));
	lit1->val = -(a->sat_variable_nr + 1);
	strcpy(lit1->sval, "-");
	strcat(lit1->sval, a->name);
	strcat(lit1->sval, "_m");
	
	struct cnf_literal *lit2 = malloc(sizeof(struct cnf_literal));
	lit2->val = b->sat_variable_nr;
	strcpy(lit2->sval, b->name);

	lit1->next = lit2;
	cl->lit = lit1;
	
	cl->next = cnf_clauses;
	cnf_clauses = cl;
	
	nr_of_clauses++;
}


/*
 * Encode the select statement as CNF
 * "A select B" translates to (-A v B)
 * A -> lit1
 * B -> lit2
 */
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
	
	cl->next = cnf_clauses;
	cnf_clauses = cl;
	
	nr_of_clauses++;
	
	// take care of tristate modules
	if (sym->type == S_BOOLEAN && e->left.sym->type == S_TRISTATE)
		build_cnf_select_bool_tri(sym, e->left.sym, 0);
	if (sym->type == S_TRISTATE && e->left.sym->type == S_BOOLEAN)
		build_cnf_select_tri_bool(sym, e->left.sym);
	if (sym->type == S_TRISTATE && e->left.sym->type == S_TRISTATE) {
		build_cnf_select_bool_tri(sym, e->left.sym, 0);
		build_cnf_select_bool_tri(sym, e->left.sym, 1);
	}
}


static void write_to_file(void)
{
	FILE *fd = fopen(OUTFILE_DIMACS, "w");
	int i;
	struct symbol *sym;
	for (i = 1; i < sat_variable_nr; i++) {
		sym = &sat_map[i];
		fprintf(fd, "c %d %s\n", i, sym->name);
		if (sym->type == S_TRISTATE)
			fprintf(fd, "c %d %s_MODULE\n", ++i, sym->name);
	}
	fprintf(fd, "p CNF %d %d\n", sat_variable_nr - 1, nr_of_clauses);

	struct cnf_clause *cl = cnf_clauses;
	while (cl != NULL) {
		struct cnf_literal *lit = cl->lit;
		while (lit != NULL) {
			fprintf(fd, "%d ", lit->val);
			if (!lit->next)
				fprintf(fd, "0\n");
			lit = lit->next;
		}
		cl = cl->next;
	}

	fclose(fd);
}

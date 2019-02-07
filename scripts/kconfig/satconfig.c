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
static struct cnf_clause *cnf_clauses; /* linked list with all CNF-clauses */
static int nr_of_clauses = 0; /* number of CNF-clauses */


static void create_sat_variables(struct symbol *sym);
static void create_tristate_constraint_clause(struct symbol *sym);

static void build_cnf_select(struct symbol *sym, struct property *p);
static void build_cnf_dependencies(struct symbol *sym);

static void build_cnf_simple_dependency(struct symbol *sym, struct k_expr *e);
static void build_cnf_expr(struct symbol *sym, struct k_expr *e);

static void add_literal_to_clause(struct cnf_clause *cl, struct symbol *sym, int sign, int mod);

static struct cnf_clause * create_cnf_clause(void);

static void write_cnf_to_file(void);


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
const char *kexpr_type[] = {
	"PLT_SYMBOL",
	"PLT_AND",
	"PLT_OR",
	"PLT_NOT"
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

	/* print all symbols and build CNF-clauses */
	for_all_symbols(i, sym) {
		print_symbol(sym);
		
		/* build CNF clauses for select statements */
		struct property *p;
		for_all_properties(sym, p, P_SELECT)
			build_cnf_select(sym, p);
		
		/* build CNF clauses for dependencies */
		if (sym->dir_dep.expr)
			build_cnf_dependencies(sym);
			
	}
		
	/* print all CNFs */
	printf("All CNFs:\n");
	print_cnf_clauses(cnf_clauses);
	
	/* write CNF-clauses in DIMACS-format to file */
	write_cnf_to_file();
	
	return EXIT_SUCCESS;
}

/*
 * bool-symbols have 1 variable (X)
 * tristate-symbols have 2 variables (X, X_m)
 */
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
	
	struct cnf_clause *cl = create_cnf_clause();
	
	add_literal_to_clause(cl, sym, -1, 0);
	add_literal_to_clause(cl, sym, -1, 1);
	
	cl->next = cnf_clauses;
	cnf_clauses = cl;
	
	nr_of_clauses++;
}

/*
 * A_bool select B_tri translates to
 * (-A v B) -- already done
 * (-B v -B_m) -- already done
 * (-A v B v B_m)
 * if mod == 1, then it's A_m instead of A (needed if A_tri select B_tri)
 */
static void build_cnf_bool_dep_tri(struct symbol *a, struct symbol *b, int mod)
{
	assert((a->type == S_BOOLEAN && mod == 0) || a->type == S_TRISTATE);
	assert(b->type == S_TRISTATE);
	
	struct cnf_clause *cl = create_cnf_clause();
	
	add_literal_to_clause(cl, a, -1, mod);
	add_literal_to_clause(cl, b, 0, 0);
	add_literal_to_clause(cl, b, 0, 1);
	
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
static void build_cnf_tri_dep_bool(struct symbol *a, struct symbol *b)
{
	assert(a->type == S_TRISTATE);
	assert(b->type == S_BOOLEAN);
	
	struct cnf_clause *cl = create_cnf_clause();
	
	add_literal_to_clause(cl, a, -1, 1);
	add_literal_to_clause(cl, b, 0, 0);
	
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
	
	struct cnf_clause *cl = create_cnf_clause();
	
	add_literal_to_clause(cl, sym, -1, 0);
	add_literal_to_clause(cl, e->left.sym, 0, 0);
	
	cl->next = cnf_clauses;
	cnf_clauses = cl;
	
	nr_of_clauses++;
	
	/* take care of tristate modules */
	if (sym->type == S_BOOLEAN && e->left.sym->type == S_TRISTATE)
		      build_cnf_bool_dep_tri(sym, e->left.sym, 0);
	if (sym->type == S_TRISTATE && e->left.sym->type == S_BOOLEAN)
		      build_cnf_tri_dep_bool(sym, e->left.sym);
	if (sym->type == S_TRISTATE && e->left.sym->type == S_TRISTATE) {
		      build_cnf_bool_dep_tri(sym, e->left.sym, 0);
		      build_cnf_bool_dep_tri(sym, e->left.sym, 1);
	}
}

/*
 * Encode "A implies B" with A and B being tristate
 * (-A v -A_m) -- already done
 * (-B v -B_m) -- already done
 * (-A v B) -- already done
 * (-A v B v -B_m)
 * (-A_m v B v B_m)
 */
static void build_cnf_tri_dep_tri(struct symbol *a, struct symbol *b) 
{
	struct cnf_clause *cl = create_cnf_clause();
	
	add_literal_to_clause(cl, a, -1, 0);
	add_literal_to_clause(cl, b, 0, 0);
	add_literal_to_clause(cl, b, -1, 1);
	
	cl->next = cnf_clauses;
	cnf_clauses = cl;
	
	
	struct cnf_clause *cl2 = create_cnf_clause();
	
	add_literal_to_clause(cl2, a, -1, 1);
	add_literal_to_clause(cl2, b, 0, 0);
	add_literal_to_clause(cl2, b, 0, 1);
	
	cl2->next = cnf_clauses;
	cnf_clauses = cl2;
	
	nr_of_clauses += 2;
}

/*
 * Encode a dependency as CNF
 * "A depends on B" translates to A implies B => (-A v B)
 * A -> lit1
 * B -> lit2
 */
static void build_cnf_simple_dependency(struct symbol *sym, struct k_expr *e)
{
	/* take care of tristate modules */
	if (sym->type == S_BOOLEAN && e->sym->type == S_TRISTATE) {
		build_cnf_bool_dep_tri(sym, e->sym, 0);
		return;
	}
	if (sym->type == S_TRISTATE && e->sym->type == S_BOOLEAN)
		build_cnf_tri_dep_bool(sym, e->sym);
	if (sym->type == S_TRISTATE && e->sym->type == S_TRISTATE)
		build_cnf_tri_dep_tri(sym, e->sym);
	
	struct cnf_clause *cl = create_cnf_clause();
	
	add_literal_to_clause(cl, sym, -1, 0);
	add_literal_to_clause(cl, e->sym, 0, 0);
	
	cl->next = cnf_clauses;
	cnf_clauses = cl;
	
	nr_of_clauses++;
}

/*
 * Encode A implies (B or C) => (-A v B v C)
 * A -> lit1
 * B -> lit2
 * C -> lit3
 */
static void build_cnf_simple_or(struct symbol *sym, struct k_expr *e1, struct k_expr *e2)
{
	struct cnf_clause *cl = create_cnf_clause();
	
	add_literal_to_clause(cl, sym, -1, 0);
	add_literal_to_clause(cl, e1->sym, 0, 0);
	add_literal_to_clause(cl, e2->sym, 0, 0);
	
	cl->next = cnf_clauses;
	cnf_clauses = cl;
	
	nr_of_clauses++;
}

/*
 * Encode A implies !B ==> (-A v -B)
 * A -> lit1
 * B -> lit2
 */
static void build_cnf_simple_not(struct symbol *sym, struct k_expr *e)
{
	struct cnf_clause *cl = create_cnf_clause();
	
	add_literal_to_clause(cl, sym, -1, 0);
	add_literal_to_clause(cl, e->sym, -1, 0);
	
	cl->next = cnf_clauses;
	cnf_clauses = cl;
	
	nr_of_clauses++;
}

/*
 * print some debug info about the tree structure of k_expr 
 */
static void debug_print_kexpr(struct k_expr *e)
{
	if (!e)
		return;
	
	printf("e-type: %s", kexpr_type[e->type]);
	if (e->parent)
		printf(", parent %s", kexpr_type[e->parent->type]);
	printf("\n");
	switch (e->type) {
	case KET_SYMBOL:
		printf("name %s\n", e->sym->name);
		break;
	case KET_AND:
	case KET_OR:
		printf("left child: %s\n", kexpr_type[e->left->type]);
		printf("right child: %s\n", kexpr_type[e->right->type]);
		debug_print_kexpr(e->left);
		debug_print_kexpr(e->right);
		break;
	case KET_NOT:
	default:
		printf("child: %s\n", kexpr_type[e->child->type]);
		debug_print_kexpr(e->child);
		break;
	}
}

/*
 * build the CNF-clauses for (SYM implies E)
 */ 
static void build_cnf_imply(struct symbol *sym, struct k_expr *e)
{
	if (e->type == KET_SYMBOL) {
		build_cnf_simple_dependency(sym, e);
		return;
	}
	if (e->type == KET_AND) {
		build_cnf_imply(sym, e->left);
		build_cnf_imply(sym, e->right);
		return;
	}
	if (e->type == KET_OR) {
		/* base case */
		if (e->left->type == KET_SYMBOL && e->right->type == KET_SYMBOL)
			build_cnf_simple_or(sym, e->left, e->right);
		return;
	}
	
}

/*
 * build the CNF-clauses for an expression
 */
static void build_cnf_expr(struct symbol *sym, struct k_expr *e)
{
	if (!e)
		return;
	
	//debug_print_kexpr(e);
	//printf("\n");
	
	switch (e->type) {
	case KET_SYMBOL:
		/* base case */
		if (!e->parent)
			build_cnf_simple_dependency(sym, e);
		break;
	case KET_AND:
		/* base case */
		if (!e->parent) {
			build_cnf_imply(sym, e->left);
			build_cnf_imply(sym, e->right);
		}
		break;
	case KET_OR:
		/* base case */
		if (!e->parent && e->left->type == KET_SYMBOL && e->right->type == KET_SYMBOL)
			build_cnf_simple_or(sym, e->left, e->right);
		break;
	case KET_NOT:
		/* base case */
		if (!e->parent && e->child->type == KET_SYMBOL)
			build_cnf_simple_not(sym, e->child);
		break;
	}
}

/*
 * parse an expr as a k_expr 
 */
static struct k_expr * parse_expr(struct expr *e, struct k_expr *parent)
{
	struct k_expr *ke = malloc(sizeof(struct k_expr));
	ke->parent = parent;

	switch (e->type) {
	case E_SYMBOL:
		ke->type = KET_SYMBOL;
		ke->sym = e->left.sym;
		return ke;
	case E_AND:
		ke->type = KET_AND;
		ke->left = parse_expr(e->left.expr, ke);
		ke->right = parse_expr(e->right.expr, ke);
		return ke;
	case E_OR:
		ke->type = KET_OR;
		ke->left = parse_expr(e->left.expr, ke);
		ke->right = parse_expr(e->right.expr, ke);
		return ke;
	case E_NOT:
		ke->type = KET_NOT;
		ke->child = parse_expr(e->left.expr, ke);
		return ke;
	default:
		return NULL;
	}
}

/*
 * build the CNF-clauses for a dependency
 */
static void build_cnf_dependencies(struct symbol* sym)
{
	assert(sym->dir_dep.expr);
	
	struct k_expr *e = parse_expr(sym->dir_dep.expr, NULL);
	build_cnf_expr(sym, e);
}

/*
 * adds a literal to a CNF-clause
 * sign 0, mod 0 -> X
 * sign -1, mod 0 -> -X
 * sign 0, mod 1 -> X_m
 * sign -1, mod 1 -> -X_m
 */
static void add_literal_to_clause(struct cnf_clause *cl, struct symbol *sym, int sign, int mod)
{
	struct cnf_literal *lit = malloc(sizeof(struct cnf_literal));

	lit->val = mod == 0 ? sym->sat_variable_nr : (sym->sat_variable_nr + 1);
	lit->val *= (sign == 0 ? 1 : -1);
	strcpy(lit->sval, sign == 0 ? "" : "-");
	strcat(lit->sval, sym->name);
	if (mod != 0)
		strcat(lit->sval, "_m");
	
	lit->next = cl->lit ? cl->lit : NULL;
	cl->lit = lit;
}

static struct cnf_clause * create_cnf_clause(void)
{
	struct cnf_clause *cl = malloc(sizeof(struct cnf_clause));
	cl->lit = NULL;
	cl->next = NULL;
	return cl;
}

/*
 * writes the CNF-clauses into a file in DIMACS-format
 */
static void write_cnf_to_file(void)
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

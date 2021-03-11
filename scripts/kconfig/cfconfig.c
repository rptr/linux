/* SPDX-License-Identifier: GPL-2.0 */
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


/* -------------------------------------- */

int main(int argc, char *argv[])
{
	printf("\nHello configfix!\n\n");
	
	run_satconf_cli(argv[1]);
// 	init_config(argv[1]);
// 	printf("Init..."); init_data();
// 	create_constants();
// 	assign_sat_variables();
// 	get_constraints(); printf("done.\n");
// 	
// 	struct symbol *sym = sym_find("S");
// 	
// 	print_sym_name(sym);
// 	print_sym_constraint(sym);
// 	
// 	fexpr_list_print("S:", sym->nb_vals);
// 	struct fexpr_node *node = sym->nb_vals->head->next;
// 	struct fexpr *e = node->elem;
// 	
// 	fexpr_list_delete(sym->nb_vals, node);
// 	fexpr_list_print("S:", sym->nb_vals);
// 	
// 	fexpr_list_delete(sym->nb_vals, sym->nb_vals->head);
// 	fexpr_list_print("S:", sym->nb_vals);
// 	
// 	fexpr_list_delete(sym->nb_vals, sym->nb_vals->tail);
// 	fexpr_list_print("S:", sym->nb_vals);
// 	
// 	fexpr_list_delete(sym->nb_vals, sym->nb_vals->tail);
// 	fexpr_list_print("S:", sym->nb_vals);
// 	
// 	fexpr_list_delete(sym->nb_vals, sym->nb_vals->tail);
// 	fexpr_list_print("S:", sym->nb_vals);
// 	
// 	fexpr_print("e:", node->elem, -1);
// 	if (node == NULL)
// 		printf("is null\n.");
	
	return EXIT_SUCCESS;
}


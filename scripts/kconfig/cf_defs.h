/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Patrick Franz <patfra71@gmail.com>
 */

#ifndef DEFS_H
#define DEFS_H

/* external variables */
extern unsigned int sat_variable_nr;
extern unsigned int tmp_variable_nr;
extern GHashTable *satmap;

extern GArray *sdv_symbols; /* array with conflict-symbols */
extern bool stop_rangefix;
extern struct fexpr *const_false;
extern struct fexpr *const_true;
extern struct fexpr *symbol_yes_fexpr;
extern struct fexpr *symbol_mod_fexpr;
extern struct fexpr *symbol_no_fexpr;

/* wrapper for GArray */
struct garray_wrapper {
	/* GArray */
	GArray *arr;
};

/* different types for k_expr */
enum kexpr_type {
	KE_SYMBOL,
	KE_AND,
	KE_OR,
	KE_NOT,
	KE_EQUAL,
	KE_UNEQUAL,
	KE_CONST_FALSE,
	KE_CONST_TRUE
};

/* struct for an expression, built like a tree structure */
struct k_expr {
	/* parent k_expr */
	struct k_expr *parent;
	
	/* type of the k_expr */
	enum kexpr_type type;
	
	/* temporary SAT variable associated with this k_expr */
	struct tmp_sat_variable *t;

	union {
		/* symbol */
		struct {
			struct symbol *sym;
			tristate tri;
		};
		/* AND, OR, NOT */
		struct {
			struct k_expr *left;
			struct k_expr *right;
		};
		/* EQUAL, UNEQUAL */
		struct {
			struct symbol *eqsym;
			struct symbol *eqvalue;
		};
	};
};

/* different types for f_expr */
enum fexpr_type {
	FE_SYMBOL,
	FE_AND,
	FE_OR,
	FE_NOT,
	FE_EQUALS,
	FE_NPC, /* no prompt condition */
	FE_TRUE,  /* constant of value True */
	FE_FALSE,  /* constant of value False */
	FE_NONBOOL,  /* for all non-(boolean/tristate) known values */
	FE_CHOICE, /* symbols of type choice */
	FE_SELECT, /* auxiliary variable for selected symbols */
	FE_TMPSATVAR /* temporary sat-variable (Tseytin) */ 
};

/* struct for a propositional logic formula */
struct fexpr {
	/* name of the feature expr */
	struct gstr name;
	
	/* associated symbol */
	struct symbol *sym;
	
	/* integer value for the SAT solver */
	int satval;
	
	/* assumption in the last call to PicoSAT */
	bool assumption;
	
	/* type of the fexpr */
	enum fexpr_type type;
	
	union {
		/* symbol */
		struct {
			tristate tri;
		};
		/* AND, OR, NOT */
		struct {
			struct fexpr *left;
			struct fexpr *right; /* not used for NOT */
		};
		/* EQUALS */
		struct {
			struct symbol *eqsym;
			struct symbol *eqvalue;
		};
		/* HEX, INTEGER, STRING */
		struct {
			struct gstr nb_val;
		};
	};
	
};

struct fexpr_list {
	struct fexpr_node *head, *tail;
	unsigned int size;
};

struct fexpr_node {
	struct fexpr *elem;
	struct fexpr_node *next, *prev;
};

struct fexl_list {
	struct fexl_node *head, *tail;
	unsigned int size;
};

struct fexl_node {
	struct fexpr_list *elem;
	struct fexl_node *next, *prev;
};

enum pexpr_type {
	PE_SYMBOL,
	PE_AND,
	PE_OR,
	PE_NOT
};

union pexpr_data {
	struct pexpr *pexpr;
	struct fexpr *fexpr;
};

struct pexpr {
	enum pexpr_type type;
	union pexpr_data left, right;
};

struct pexpr_list {
	struct pexpr_node *head, *tail;
	unsigned int size;
};

struct pexpr_node {
	struct pexpr *elem;
	struct pexpr_node *next, *prev;
};

struct default_map {
	struct fexpr *val;
	
	struct pexpr *e;
};

enum symboldv_type {
	SDV_BOOLEAN,	/* boolean/tristate */
	SDV_NONBOOLEAN	/* string/int/hex */
};

struct symbol_dvalue {
	struct symbol *sym;
	
	enum symboldv_type type;
	
	union {
		/* boolean/tristate */
		tristate tri;
		
		/* string/int/hex */
		struct gstr nb_val;
	};
};

struct sdv_list {
	struct sdv_node *head, *tail;
	unsigned int size;
};

struct sdv_node {
	struct symbol_dvalue *elem;
	struct sdv_node *next, *prev;
};

enum symbolfix_type {
	SF_BOOLEAN,	/* boolean/tristate */
	SF_NONBOOLEAN,	/* string/int/hex */
	SF_DISALLOWED	/* disallowed non-boolean values */
};

struct symbol_fix {
	struct symbol *sym;
	
	enum symbolfix_type type;
	
	union {
		/* boolean/tristate */
		tristate tri;
		
		/* string/int/hex */
		struct gstr nb_val;
		
		/* disallowed non-boolean values */
		struct gstr disallowed;
	};
};

struct sfix_list {
	struct sfix_node *head, *tail;
	unsigned int size;
};

struct sfix_node {
	struct symbol_fix *elem;
	struct sfix_node *next, *prev;
};

struct sfl_list {
	struct sfl_node *head, *tail;
	unsigned int size;
};

struct sfl_node {
	struct sfix_list *elem;
	struct sfl_node *next, *prev;
};

#endif

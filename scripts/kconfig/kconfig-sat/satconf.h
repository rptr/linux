#ifndef SATCONF_H
#define SATCONF_H

#define CNF_REASON_LENGTH 512
#define STRING_LENGTH 100
#define PRINT_ALL_CNF false
#define PRINT_CNF_REASONS true
#define SATVARS_PER_SYMBOL 3

/* external functions */
int run_satconf(const char *Kconfig_file);
char * get_test_char(void);

/* external variables */
extern unsigned int sat_variable_nr;
extern unsigned int tmp_variable_nr;
extern GHashTable *satmap;
extern GArray *cnf_clauses; /* array with all CNF-clauses */
extern struct tmp_sat_variable *tmp_sat_vars;
extern unsigned int nr_of_clauses; /* number of CNF-clauses */
extern struct fexpr *const_false;
extern struct fexpr *const_true;

/* struct for a CNF-clause */
struct cnf_clause {
	/* array of all literals */
	GArray *lits;

	/* reason - string refers to a specific boolean law that produced this clause */
	struct gstr reason;
};

/* struct for a literal in a CNF-clause */
struct cnf_literal {
	/* integer value needed by the SAT-solver */
	int val;

	/* string representation for debugging */
	struct gstr name;
};

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
			tristate tristate;
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

/* different types for k_expr */
enum fexpr_type {
	FE_SYMBOL,
	FE_AND,
	FE_OR,
	FE_NOT,
	FE_EQUALS,
	FE_TRUE,  /* constant of value True */
	FE_FALSE,  /* constant of value False */
	FE_NONBOOL  /* for all non-(boolean/tristate) constants */
};

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
			tristate tristate;
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

/* temporary SAT variables */
struct tmp_sat_variable {
	/* temporary variable number */
	int nr;
	
	/* integer value for the SAT solver */
	int satval;
	
	/* string representation for debugging */
	char sval[STRING_LENGTH];
	
	/* parent temporary SAT variable */
	struct tmp_sat_variable *parent;
	
	/* next temporary SAT variable - null if last */
	struct tmp_sat_variable *next;
};

struct default_map {
	struct gstr val;
	
	struct fexpr *e;
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

#endif

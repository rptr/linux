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
#include "lkc.h"

#include "kconfig-sat/satconf.h"


/* -------------------------------------- */


int main(int argc, char *argv[])
{
	printf("\nHello satconfig!\n");

	run_satconf(argv[1]);
	
	return EXIT_SUCCESS;
}


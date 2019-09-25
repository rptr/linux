#ifndef SATUTILS_H
#define SATUTILS_H

PicoSAT * initialize_picosat(void);
void picosat_add_clauses(PicoSAT *pico);
void picosat_solve(PicoSAT *pico);

#endif

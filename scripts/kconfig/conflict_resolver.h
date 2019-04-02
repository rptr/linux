/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 Ibrahim Fayaz <phayax@gmail.com>
 */

#ifndef CONFLICT_RESOLVER_H
#define CONFLICT_RESOLVER_H

#include <qstring.h>
#include "expr.h"

#ifdef __cplusplus
extern "C" {
#endif

enum symbol_status {
    UNSATISFIED,
    SATISFIED
};

typedef struct
{
    QString symbol;
    QString change_needed;
    enum symbol_status status;
    tristate req; // change requested
} Constraint ;

QList<Constraint> get_constraints();
QString tristate_value_to_string(tristate x);







#ifdef __cplusplus
}
#endif

#endif
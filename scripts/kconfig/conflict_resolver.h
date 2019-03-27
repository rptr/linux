/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 Ibrahim Fayaz <phayax@gmail.com>
 */

#ifndef CONFLICT_RESOLVER_H
#define CONFLICT_RESOLVER_H

#include <qstring.h>

#ifdef __cplusplus
extern "C" {
#endif

enum symbol_status {
    UNSATISFIED,
    SATISFIED
};
enum change_req {
    NO,
    YES,
    MODULE,
};

typedef struct
{
    QString symbol;
    QString change_needed;
    enum symbol_status status;
    enum change_req req;
} Constraint ;

QList<Constraint> get_constraints();







#ifdef __cplusplus
}
#endif

#endif
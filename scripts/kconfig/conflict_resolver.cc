/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 Ibrahim Fayaz <phayax@gmail.com>
 */
#include <iostream>
#include <QList>
#include "conflict_resolver.h"

QList<Constraint> get_constraints()
{
    Constraint tc;
    tc.symbol = "hello";
    tc.change_needed = "hello";
    tc.status = UNSATISFIED;
    QList<Constraint> x = {tc};
    return x;

}

/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 Ibrahim Fayaz <phayax@gmail.com>
 */
#include <iostream>
#include <QList>
#include "conflict_resolver.h"

QList<Constraint> get_constraints()
{
    /*
    	- Example candidate symbols:
		  -DMI_SYSFS (tristate example)
		  	- under Firmware drivers -> DMI table support in sysfs
		  -DMIID (boolean example)
		  	- under Firmware drivers -> Export DMI identification
		  - EDD (tristate example with a hierarchy within )
		  - NET_KEY_MIGRATE and XFRM_MIGRATE
    */
    Constraint tc;
    tc.symbol = "DMI_SYSFS";
    tc.change_needed = "change to yes";
    tc.status = UNSATISFIED;
    tc.req = yes;

    Constraint tc2;
    tc2.symbol = "DMIID";
    tc2.change_needed = "change to no";
    tc2.status = UNSATISFIED;
    tc2.req = no;

    Constraint tc3;
    tc3.symbol = "EDD";
    tc3.change_needed = "change to module";
    tc3.status = UNSATISFIED;
    tc3.req = mod;

    QList<Constraint> x = {tc, tc2, tc3};
    return x;

}

QString tristate_value_to_string(tristate x)
{
    switch (x)
    {
        case no:
            return  QString::fromStdString("NO");
            break;
        case yes:
            return  QString::fromStdString("YES");
            break;
        case mod:
            return  QString::fromStdString("MODULE");
            break;

        default:
            break;
    }

}
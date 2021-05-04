// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Patrick Franz <patfra71@gmail.com>
 */

#include <iostream>
#include <QList>
#include "conflict_resolver.h"

QString tristate_value_to_string(tristate val)
{
	switch (val) {
	case yes:
		return QString::fromStdString("YES");
	case mod:
		return QString::fromStdString("MODULE");
	case no:
		return QString::fromStdString("NO");
	default:
		return QString::fromStdString("");
	}
}
tristate string_value_to_tristate(QString s)
{
	if (s == "YES")
		return tristate::yes;
	else if (s == "MODULE")
		return tristate::mod;
	else if (s == "NO")
		return tristate::no;
	else
		return tristate::no;
}

// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>
 * Copyright (C) 2015 Boris Barbulovski <bbarbulovski@gmail.com>
 */

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QDesktopWidget>
#include <QFileDialog>
#include <QLabel>
#include <QLayout>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QToolBar>
#include <QListWidget>
#include <QComboBox>
#include <QTableWidget>
#include <QHBoxLayout>

#include <stdlib.h>

#include "lkc.h"
#include "qconf.h"

#include "images.h"
#include <iostream>

#include <QAbstractItemView>
#include <QMimeData>
#include <QBrush>
#include <QColor>
#include <future>
#include <memory>

/* ----- start cf_testing.h ----- */
#ifdef CONFIGFIX_TEST

#include <algorithm>    // std::next_permutation
#include <random>
#include <QTextStream>
#include <QTimer>
#include <dirent.h>
#include <time.h>

#include "configfix.h"
#include "cf_utils.h"

// iterate fixes in a diagnosis with and without index
#define for_all_fixes(diag, fix) \
	int fix_idx; for (fix_idx = 0; fix_idx < diag->len; fix_idx++) for (fix = g_array_index(diag, struct symbol_fix *, fix_idx);fix;fix=NULL)
#define for_every_fix(diag, i, fix) \
	for (i = 0; i < diag->len; i++) for (fix = g_array_index(diag, struct symbol_fix *, i);fix;fix=NULL)

// testing modes
#define RANDOM_TESTING 1
#define MANUAL_TESTING 2
static int testing_mode = RANDOM_TESTING; // default

/* Total number of generated conflicts = 
   (MAX_CONFLICT_SIZE - MIN_CONFLICT_SIZE + 1) x NO_CONFLICTS */
// minimum no. symbols in a random conflict
#define MIN_CONFLICT_SIZE 1 //4
// maximum no. symbols in a random conflict
#define MAX_CONFLICT_SIZE 10 //10
// number of random conflicts of each size
#define NO_CONFLICTS 5 //5

#define BASE_CONFIG ".config.base"
#define RESULTS_FILE "results.csv"

/* Persistent configuration statistics */
static int sym_count = 0;
// tristates in configuration space
static bool tristates = false;
// no. symbols with the value YES or MOD
static int no_enabled_symbols = 0;
// no. symbols that conflict with current config
static int no_conflict_candidates = 0;

// default conflict size
static int conflict_size = 1;
// directory where generated conflict is saved
static char *conflict_dir;
// result string to be written to RESULTS_FILE
static gstr result_string = str_new(); // TODO remove call?


// Mersenne Twister random number generator, initialised once
static auto seed = std::random_device{}();
static std::mt19937 rng(seed);


// Print test environment details
static void print_setup(const char *name);
static void print_config_stats(ConfigList *list);
static void print_sample_stats();


// Backup, compare, and reset configuration options
/* // hash table element
typedef struct sym_hash_elem {
	// symbol name
	char *name;
	// its value as string
	char *value;
	// chain elements in case of hash collisions
	struct sym_hash_elem *next;
} SymHash;

// hash table size
#define HASHSIZE 20000 */

// backups of the base config and the given configuration sample
static GHashTable *base_config, *initial_config;
static GHashTable* config_backup(void);
// static SymHash *base_config[HASHSIZE], *initial_config[HASHSIZE];
// static SymHash *config_backup(void);
// static void config_backup_base(void);
// static void config_backup_initial(void);
static int config_compare(GHashTable *backup);
static void config_reset(void);

// Symbol properties
static bool sym_has_conflict(struct symbol *sym);
static int sym_has_blocked_values(struct symbol *sym);
static bool sym_enabled_in_base_config(struct symbol *sym);
static const char* sym_get_type_name(struct symbol *sym);

// Configuration conflicts & conflict fixes
static ConfigItem* get_conflict_item(int index, ConfigList *configList);
static /*struct*/ symbol* get_conflict_sym(int index);
static tristate random_blocked_value(struct symbol *sym);
static symbol_fix* get_symbol_fix(struct symbol *sym, GArray *diag);
static const char* sym_fix_get_string_value(struct symbol_fix *sym_fix);
static bool diag_dependencies_met(GArray *diag);
static bool symbol_has_changed(struct symbol *sym, GHashTable *backup);
// static GArray* rearrange_diagnosis(GArray *diag, int fix_idxs[]);
static sfix_list* rearrange_solution(sfix_list *diag);
static bool free_permutation(sfix_list *permutation);
// determines diagnosis order in the above function
static bool reverse_diag_order = false;
static void save_diagnosis(struct sfix_list *diag, char* file_prefix, bool valid_diag);
static bool verify_solution(int i, const char *result_prefix, struct sfix_list *solution/*GArray *diag*/, QTableWidget* conflictsTable);
static bool verify_fix_target_values(GArray *diag);
static bool verify_resolution(QTableWidget* conflictsTable);
static bool verify_changed_symbols(GArray *diag);
//static void print_diagnosis_symbol(struct sfix_list *diag_sym)


// Manipulate result string
static void append_result(char *str);
static void output_result();

// Filenames
static char *get_config_dir(void);
static char *get_conflict_dir(void);
static char *get_results_file(void);
#endif
/* ----- end cf_testing.h ----- */

static QApplication *configApp;
static ConfigSettings *configSettings;

QAction *ConfigMainWindow::saveAction;

// TODO is this needed?
static inline QString qgettext(const char* str)
{
	return QString::fromLocal8Bit(str);
}

ConfigSettings::ConfigSettings()
	: QSettings("kernel.org", "qconf")
{
}

/**
 * Reads a list of integer values from the application settings.
 */
QList<int> ConfigSettings::readSizes(const QString& key, bool *ok)
{
	QList<int> result;

	if (contains(key))
	{
		QStringList entryList = value(key).toStringList();
		QStringList::Iterator it;

		for (it = entryList.begin(); it != entryList.end(); ++it)
			result.push_back((*it).toInt());

		*ok = true;
	}
	else
		*ok = false;

	return result;
}

/**
 * Writes a list of integer values to the application settings.
 */
bool ConfigSettings::writeSizes(const QString& key, const QList<int>& value)
{
	QStringList stringList;
	QList<int>::ConstIterator it;

	for (it = value.begin(); it != value.end(); ++it)
		stringList.push_back(QString::number(*it));
	setValue(key, stringList);

	return true;
}

QIcon ConfigItem::symbolYesIcon;
QIcon ConfigItem::symbolModIcon;
QIcon ConfigItem::symbolNoIcon;
QIcon ConfigItem::choiceYesIcon;
QIcon ConfigItem::choiceNoIcon;
QIcon ConfigItem::menuIcon;
QIcon ConfigItem::menubackIcon;

/*
 * set the new data
 * TODO check the value
 */
void ConfigItem::okRename(int col)
{
}

/*
 * update the displayed of a menu entry
 */
void ConfigItem::updateMenu(void)
{
	ConfigList* list;
	struct symbol *sym;
	struct property *prop;
	QString prompt;
	int type;
	tristate expr;

	list = listView();
	if (goParent) {
		setIcon(promptColIdx, menubackIcon);
		prompt = "..";
		goto set_prompt;
	}

	sym = menu->sym;
	prop = menu->prompt;
	prompt = menu_get_prompt(menu);

	if (prop) switch (prop->type) {
	case P_MENU:
		if (list->mode == singleMode || list->mode == symbolMode) {
			/* a menuconfig entry is displayed differently
			 * depending whether it's at the view root or a child.
			 */
			if (sym && list->rootEntry == menu)
				break;
			setIcon(promptColIdx, menuIcon);
		} else {
			if (sym)
				break;
			setIcon(promptColIdx, QIcon());
		}
		goto set_prompt;
	case P_COMMENT:
		setIcon(promptColIdx, QIcon());
		goto set_prompt;
	default:
		;
	}
	if (!sym)
		goto set_prompt;

	setText(nameColIdx, sym->name);

	type = sym_get_type(sym);
	switch (type) {
	case S_BOOLEAN:
	case S_TRISTATE:
		char ch;

		if (!sym_is_changeable(sym) && list->optMode == normalOpt) {
			setIcon(promptColIdx, QIcon());
			setText(noColIdx, QString());
			setText(modColIdx, QString());
			setText(yesColIdx, QString());
			break;
		}
		expr = sym_get_tristate_value(sym);
		switch (expr) {
		case yes:
			if (sym_is_choice_value(sym) && type == S_BOOLEAN)
				setIcon(promptColIdx, choiceYesIcon);
			else
				setIcon(promptColIdx, symbolYesIcon);
			setText(yesColIdx, "Y");
			ch = 'Y';
			break;
		case mod:
			setIcon(promptColIdx, symbolModIcon);
			setText(modColIdx, "M");
			ch = 'M';
			break;
		default:
			if (sym_is_choice_value(sym) && type == S_BOOLEAN)
				setIcon(promptColIdx, choiceNoIcon);
			else
				setIcon(promptColIdx, symbolNoIcon);
			setText(noColIdx, "N");
			ch = 'N';
			break;
		}
		if (expr != no)
			setText(noColIdx, sym_tristate_within_range(sym, no) ? "_" : 0);
		if (expr != mod)
			setText(modColIdx, sym_tristate_within_range(sym, mod) ? "_" : 0);
		if (expr != yes)
			setText(yesColIdx, sym_tristate_within_range(sym, yes) ? "_" : 0);

		setText(dataColIdx, QChar(ch));
		break;
	case S_INT:
	case S_HEX:
	case S_STRING:
		const char* data;

		data = sym_get_string_value(sym);

		setText(dataColIdx, data);
		if (type == S_STRING)
			prompt = QString("%1: %2").arg(prompt).arg(data);
		else
			prompt = QString("(%2) %1").arg(prompt).arg(data);
		break;
	}
	if (!sym_has_value(sym) && visible)
		prompt += " (NEW)";
set_prompt:
	setText(promptColIdx, prompt);
}

void ConfigItem::testUpdateMenu(bool v)
{
	ConfigItem* i;

	visible = v;
	if (!menu)
		return;

	sym_calc_value(menu->sym);
	if (menu->flags & MENU_CHANGED) {
		/* the menu entry changed, so update all list items */
		menu->flags &= ~MENU_CHANGED;
		for (i = (ConfigItem*)menu->data; i; i = i->nextItem)
			i->updateMenu();
	} else if (listView()->updateAll)
		updateMenu();
}


/*
 * construct a menu entry
 */
void ConfigItem::init(void)
{
	if (menu) {
		ConfigList* list = listView();
		nextItem = (ConfigItem*)menu->data;
		menu->data = this;

		if (list->mode != fullMode)
			setExpanded(true);
		sym_calc_value(menu->sym);
	}
	updateMenu();
}

/*
 * destruct a menu entry
 */
ConfigItem::~ConfigItem(void)
{
	if (menu) {
		ConfigItem** ip = (ConfigItem**)&menu->data;
		for (; *ip; ip = &(*ip)->nextItem) {
			if (*ip == this) {
				*ip = nextItem;
				break;
			}
		}
	}
}

ConfigLineEdit::ConfigLineEdit(ConfigView* parent)
	: Parent(parent)
{
	connect(this, SIGNAL(editingFinished()), SLOT(hide()));
}

void ConfigLineEdit::show(ConfigItem* i)
{
	item = i;
	if (sym_get_string_value(item->menu->sym))
		setText(sym_get_string_value(item->menu->sym));
	else
		setText(QString());
	Parent::show();
	setFocus();
}

void ConfigLineEdit::keyPressEvent(QKeyEvent* e)
{
	switch (e->key()) {
	case Qt::Key_Escape:
		break;
	case Qt::Key_Return:
	case Qt::Key_Enter:
		sym_set_string_value(item->menu->sym, text().toLatin1());
		parent()->updateList();
		break;
	default:
		Parent::keyPressEvent(e);
		return;
	}
	e->accept();
	parent()->list->setFocus();
	hide();
}

ConfigList::ConfigList(ConfigView* p, const char *name)
	: Parent(p),
	  updateAll(false),
	  showName(false), showRange(false), showData(false), mode(singleMode), optMode(normalOpt),
	  rootEntry(0), headerPopup(0)
{
	setObjectName(name);
	setSortingEnabled(false);
	setRootIsDecorated(true);

	setVerticalScrollMode(ScrollPerPixel);
	setHorizontalScrollMode(ScrollPerPixel);

	setHeaderLabels(QStringList() << "Option" << "Name" << "N" << "M" << "Y" << "Value");

	connect(this, SIGNAL(itemSelectionChanged(void)),
		SLOT(updateSelection(void)));

	if (name) {
		configSettings->beginGroup(name);
		showName = configSettings->value("/showName", false).toBool();
		showRange = configSettings->value("/showRange", false).toBool();
		showData = configSettings->value("/showData", false).toBool();
		optMode = (enum optionMode)configSettings->value("/optionMode", 0).toInt();
		configSettings->endGroup();
		connect(configApp, SIGNAL(aboutToQuit()), SLOT(saveSettings()));
	}

	showColumn(promptColIdx);

	reinit();
}

bool ConfigList::menuSkip(struct menu *menu)
{
	if (optMode == normalOpt && menu_is_visible(menu))
		return false;
	if (optMode == promptOpt && menu_has_prompt(menu))
		return false;
	if (optMode == allOpt)
		return false;
	return true;
}

void ConfigList::reinit(void)
{
	hideColumn(dataColIdx);
	hideColumn(yesColIdx);
	hideColumn(modColIdx);
	hideColumn(noColIdx);
	hideColumn(nameColIdx);

	if (showName)
		showColumn(nameColIdx);
	if (showRange) {
		showColumn(noColIdx);
		showColumn(modColIdx);
		showColumn(yesColIdx);
	}
	if (showData)
		showColumn(dataColIdx);

	updateListAll();
}

void ConfigList::setOptionMode(QAction *action)
{
	if (action == showNormalAction)
		optMode = normalOpt;
	else if (action == showAllAction)
		optMode = allOpt;
	else
		optMode = promptOpt;

	updateListAll();
}

void ConfigList::saveSettings(void)
{
	if (!objectName().isEmpty()) {
		configSettings->beginGroup(objectName());
		configSettings->setValue("/showName", showName);
		configSettings->setValue("/showRange", showRange);
		configSettings->setValue("/showData", showData);
		configSettings->setValue("/optionMode", (int)optMode);
		configSettings->endGroup();
	}
}

ConfigItem* ConfigList::findConfigItem(struct menu *menu)
{
	ConfigItem* item = (ConfigItem*)menu->data;

	for (; item; item = item->nextItem) {
		if (this == item->listView())
			break;
	}

	return item;
}

void ConfigList::updateSelection(void)
{
	struct menu *menu;
	enum prop_type type;

	if (selectedItems().count() == 0)
		return;

	emit selectedChanged(selectedItems());
	ConfigItem* item = (ConfigItem*)selectedItems().first();
	if (!item)
		return;

	menu = item->menu;
	emit menuChanged(menu);
	if (!menu)
		return;
	type = menu->prompt ? menu->prompt->type : P_UNKNOWN;
	if (mode == menuMode && type == P_MENU)
		emit menuSelected(menu);
}

void ConfigList::updateList()
{
	ConfigItem* last = 0;
	ConfigItem *item;

	if (!rootEntry) {
		if (mode != listMode)
			goto update;
		QTreeWidgetItemIterator it(this);

		while (*it) {
			item = (ConfigItem*)(*it);
			if (!item->menu)
				continue;
			item->testUpdateMenu(menu_is_visible(item->menu));

			++it;
		}
		return;
	}

	if (rootEntry != &rootmenu && (mode == singleMode ||
	    (mode == symbolMode && rootEntry->parent != &rootmenu))) {
		item = (ConfigItem *)topLevelItem(0);
		if (!item)
			item = new ConfigItem(this, 0, true);
		last = item;
	}
	if ((mode == singleMode || (mode == symbolMode && !(rootEntry->flags & MENU_ROOT))) &&
	    rootEntry->sym && rootEntry->prompt) {
		item = last ? last->nextSibling() : nullptr;
		if (!item)
			item = new ConfigItem(this, last, rootEntry, true);
		else
			item->testUpdateMenu(true);

		updateMenuList(item, rootEntry);
		update();
		resizeColumnToContents(0);
		return;
	}
update:
	updateMenuList(rootEntry);
	update();
	resizeColumnToContents(0);
}

void ConfigList::setValue(ConfigItem* item, tristate val)
{
	struct symbol *sym;
	int type;
	tristate oldval;

	sym = item->menu ? item->menu->sym : 0;
	if (!sym)
		return;

	type = sym_get_type(sym);
	switch (type) {
	case S_BOOLEAN:
	case S_TRISTATE:
		oldval = sym_get_tristate_value(sym);

		if (!sym_set_tristate_value(sym, val))
			return;
		if (oldval == no && item->menu->list)
			item->setExpanded(true);
		parent()->updateList();
		break;
	}
}

void ConfigList::changeValue(ConfigItem* item)
{
	struct symbol *sym;
	struct menu *menu;
	int type, oldexpr, newexpr;

	menu = item->menu;
	if (!menu)
		return;
	sym = menu->sym;
	if (!sym) {
		if (item->menu->list)
			item->setExpanded(!item->isExpanded());
		return;
	}

	type = sym_get_type(sym);
	switch (type) {
	case S_BOOLEAN:
	case S_TRISTATE:
		oldexpr = sym_get_tristate_value(sym);
		newexpr = sym_toggle_tristate_value(sym);
		if (item->menu->list) {
			if (oldexpr == newexpr)
				item->setExpanded(!item->isExpanded());
			else if (oldexpr == no)
				item->setExpanded(true);
		}
		if (oldexpr != newexpr)
			parent()->updateList();
			emit UpdateConflictsViewColorization();
		break;
	case S_INT:
	case S_HEX:
	case S_STRING:
		parent()->lineEdit->show(item);
		break;
	}
}

void ConfigList::setRootMenu(struct menu *menu)
{
	enum prop_type type;

	if (rootEntry == menu)
		return;
	type = menu && menu->prompt ? menu->prompt->type : P_UNKNOWN;
	if (type != P_MENU)
		return;
	updateMenuList(0);
	rootEntry = menu;
	updateListAll();
	if (currentItem()) {
		setSelected(currentItem(), hasFocus());
		scrollToItem(currentItem());
	}
}

void ConfigList::setParentMenu(void)
{
	ConfigItem* item;
	struct menu *oldroot;

	oldroot = rootEntry;
	if (rootEntry == &rootmenu)
		return;
	setRootMenu(menu_get_parent_menu(rootEntry->parent));

	QTreeWidgetItemIterator it(this);
	while (*it) {
		item = (ConfigItem *)(*it);
		if (item->menu == oldroot) {
			setCurrentItem(item);
			scrollToItem(item);
			break;
		}

		++it;
	}
}

/*
 * update all the children of a menu entry
 *   removes/adds the entries from the parent widget as necessary
 *
 * parent: either the menu list widget or a menu entry widget
 * menu: entry to be updated
 */
void ConfigList::updateMenuList(ConfigItem *parent, struct menu* menu)
{
	struct menu* child;
	ConfigItem* item;
	ConfigItem* last;
	bool visible;
	enum prop_type type;

	if (!menu) {
		while (parent->childCount() > 0)
		{
			delete parent->takeChild(0);
		}

		return;
	}

	last = parent->firstChild();
	if (last && !last->goParent)
		last = 0;
	for (child = menu->list; child; child = child->next) {
		item = last ? last->nextSibling() : parent->firstChild();
		type = child->prompt ? child->prompt->type : P_UNKNOWN;

		switch (mode) {
		case menuMode:
			if (!(child->flags & MENU_ROOT))
				goto hide;
			break;
		case symbolMode:
			if (child->flags & MENU_ROOT)
				goto hide;
			break;
		default:
			break;
		}

		visible = menu_is_visible(child);
		if (!menuSkip(child)) {
			if (!child->sym && !child->list && !child->prompt)
				continue;
			if (!item || item->menu != child)
				item = new ConfigItem(parent, last, child, visible);
			else
				item->testUpdateMenu(visible);

			if (mode == fullMode || mode == menuMode || type != P_MENU)
				updateMenuList(item, child);
			else
				updateMenuList(item, 0);
			last = item;
			continue;
		}
hide:
		if (item && item->menu == child) {
			last = parent->firstChild();
			if (last == item)
				last = 0;
			else while (last->nextSibling() != item)
				last = last->nextSibling();
			delete item;
		}
	}
}

void ConfigList::updateMenuList(struct menu *menu)
{
	struct menu* child;
	ConfigItem* item;
	ConfigItem* last;
	bool visible;
	enum prop_type type;

	if (!menu) {
		while (topLevelItemCount() > 0)
		{
			delete takeTopLevelItem(0);
		}

		return;
	}

	last = (ConfigItem *)topLevelItem(0);
	if (last && !last->goParent)
		last = 0;
	for (child = menu->list; child; child = child->next) {
		item = last ? last->nextSibling() : (ConfigItem *)topLevelItem(0);
		type = child->prompt ? child->prompt->type : P_UNKNOWN;

		switch (mode) {
		case menuMode:
			if (!(child->flags & MENU_ROOT))
				goto hide;
			break;
		case symbolMode:
			if (child->flags & MENU_ROOT)
				goto hide;
			break;
		default:
			break;
		}

		visible = menu_is_visible(child);
		if (!menuSkip(child)) {
			if (!child->sym && !child->list && !child->prompt)
				continue;
			if (!item || item->menu != child)
				item = new ConfigItem(this, last, child, visible);
			else
				item->testUpdateMenu(visible);

			if (mode == fullMode || mode == menuMode || type != P_MENU)
				updateMenuList(item, child);
			else
				updateMenuList(item, 0);
			last = item;
			continue;
		}
hide:
		if (item && item->menu == child) {
			last = (ConfigItem *)topLevelItem(0);
			if (last == item)
				last = 0;
			else while (last->nextSibling() != item)
				last = last->nextSibling();
			delete item;
		}
	}
}

void ConfigList::keyPressEvent(QKeyEvent* ev)
{
	QTreeWidgetItem* i = currentItem();
	ConfigItem* item;
	struct menu *menu;
	enum prop_type type;

	if (ev->key() == Qt::Key_Escape && mode != fullMode && mode != listMode) {
		emit parentSelected();
		ev->accept();
		return;
	}

	if (!i) {
		Parent::keyPressEvent(ev);
		return;
	}
	item = (ConfigItem*)i;

	switch (ev->key()) {
	case Qt::Key_Return:
	case Qt::Key_Enter:
		if (item->goParent) {
			emit parentSelected();
			break;
		}
		menu = item->menu;
		if (!menu)
			break;
		type = menu->prompt ? menu->prompt->type : P_UNKNOWN;
		if (type == P_MENU && rootEntry != menu &&
		    mode != fullMode && mode != menuMode) {
			if (mode == menuMode)
				emit menuSelected(menu);
			else
				emit itemSelected(menu);
			break;
		}
	case Qt::Key_Space:
		changeValue(item);
		break;
	case Qt::Key_N:
		setValue(item, no);
		break;
	case Qt::Key_M:
		setValue(item, mod);
		break;
	case Qt::Key_Y:
		setValue(item, yes);
		break;
	default:
		Parent::keyPressEvent(ev);
		return;
	}
	ev->accept();
}

void ConfigList::mousePressEvent(QMouseEvent* e)
{
	//QPoint p(contentsToViewport(e->pos()));
	//printf("contentsMousePressEvent: %d,%d\n", p.x(), p.y());
	Parent::mousePressEvent(e);
}

void ConfigList::mouseReleaseEvent(QMouseEvent* e)
{
	QPoint p = e->pos();
	ConfigItem* item = (ConfigItem*)itemAt(p);
	struct menu *menu;
	enum prop_type ptype;
	QIcon icon;
	int idx, x;

	if (!item)
		goto skip;

	menu = item->menu;
	x = header()->offset() + p.x();
	idx = header()->logicalIndexAt(x);
	switch (idx) {
	case promptColIdx:
		icon = item->icon(promptColIdx);
		if (!icon.isNull()) {
			int off = header()->sectionPosition(0) + visualRect(indexAt(p)).x() + 4; // 4 is Hardcoded image offset. There might be a way to do it properly.
			if (x >= off && x < off + icon.availableSizes().first().width()) {
				if (item->goParent) {
					emit parentSelected();
					break;
				} else if (!menu)
					break;
				ptype = menu->prompt ? menu->prompt->type : P_UNKNOWN;
				if (ptype == P_MENU && rootEntry != menu &&
				    mode != fullMode && mode != menuMode &&
                                    mode != listMode)
					emit menuSelected(menu);
				else
					changeValue(item);
			}
		}
		break;
	case noColIdx:
		setValue(item, no);
		break;
	case modColIdx:
		setValue(item, mod);
		break;
	case yesColIdx:
		setValue(item, yes);
		break;
	case dataColIdx:
		changeValue(item);
		break;
	}

skip:
	//printf("contentsMouseReleaseEvent: %d,%d\n", p.x(), p.y());
	Parent::mouseReleaseEvent(e);
}

void ConfigList::mouseMoveEvent(QMouseEvent* e)
{
	//QPoint p(contentsToViewport(e->pos()));
	//printf("contentsMouseMoveEvent: %d,%d\n", p.x(), p.y());
	Parent::mouseMoveEvent(e);
}

void ConfigList::mouseDoubleClickEvent(QMouseEvent* e)
{
	QPoint p = e->pos();
	ConfigItem* item = (ConfigItem*)itemAt(p);
	struct menu *menu;
	enum prop_type ptype;

	if (!item)
		goto skip;
	if (item->goParent) {
		emit parentSelected();
		goto skip;
	}
	menu = item->menu;
	if (!menu)
		goto skip;
	ptype = menu->prompt ? menu->prompt->type : P_UNKNOWN;
	if (ptype == P_MENU && mode != listMode) {
		if (mode == singleMode)
			emit itemSelected(menu);
		else if (mode == symbolMode)
			emit menuSelected(menu);
	} else if (menu->sym)
		changeValue(item);

skip:
	//printf("contentsMouseDoubleClickEvent: %d,%d\n", p.x(), p.y());
	Parent::mouseDoubleClickEvent(e);
}

void ConfigList::focusInEvent(QFocusEvent *e)
{
	struct menu *menu = NULL;

	Parent::focusInEvent(e);

	ConfigItem* item = (ConfigItem *)currentItem();
	if (item) {
		setSelected(item, true);
		menu = item->menu;
	}
	emit gotFocus(menu);
}

void ConfigList::contextMenuEvent(QContextMenuEvent *e)
{
	if (!headerPopup) {
		QAction *action;

		headerPopup = new QMenu(this);
		action = new QAction("Show Name", this);
		action->setCheckable(true);
		connect(action, SIGNAL(toggled(bool)),
			parent(), SLOT(setShowName(bool)));
		connect(parent(), SIGNAL(showNameChanged(bool)),
			action, SLOT(setChecked(bool)));
		action->setChecked(showName);
		headerPopup->addAction(action);

		action = new QAction("Show Range", this);
		action->setCheckable(true);
		connect(action, SIGNAL(toggled(bool)),
			parent(), SLOT(setShowRange(bool)));
		connect(parent(), SIGNAL(showRangeChanged(bool)),
			action, SLOT(setChecked(bool)));
		action->setChecked(showRange);
		headerPopup->addAction(action);

		action = new QAction("Show Data", this);
		action->setCheckable(true);
		connect(action, SIGNAL(toggled(bool)),
			parent(), SLOT(setShowData(bool)));
		connect(parent(), SIGNAL(showDataChanged(bool)),
			action, SLOT(setChecked(bool)));
		action->setChecked(showData);
		headerPopup->addAction(action);
	}

	headerPopup->exec(e->globalPos());
	e->accept();
}

ConfigView*ConfigView::viewList;
QAction *ConfigList::showNormalAction;
QAction *ConfigList::showAllAction;
QAction *ConfigList::showPromptAction;
QAction *ConfigList::addSymbolsFromContextMenu;

ConfigView::ConfigView(QWidget* parent, const char *name)
	: Parent(parent)
{
	setObjectName(name);
	QVBoxLayout *verticalLayout = new QVBoxLayout(this);
	verticalLayout->setContentsMargins(0, 0, 0, 0);

	list = new ConfigList(this);
	list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	list->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(list, SIGNAL(customContextMenuRequested(const QPoint &)),
        this, SLOT(ShowContextMenu(const QPoint &)));
	verticalLayout->addWidget(list);
	lineEdit = new ConfigLineEdit(this);
	lineEdit->hide();
	verticalLayout->addWidget(lineEdit);

	this->nextView = viewList;
	viewList = this;
}
void ConfigView::ShowContextMenu(const QPoint& pos){
   QMenu contextMenu(tr("Context menu"), this);

   contextMenu.addAction(ConfigList::addSymbolsFromContextMenu);
   contextMenu.exec(mapToGlobal(pos));
}

ConfigView::~ConfigView(void)
{
	ConfigView** vp;

	for (vp = &viewList; *vp; vp = &(*vp)->nextView) {
		if (*vp == this) {
			*vp = nextView;
			break;
		}
	}
}

void ConfigView::setShowName(bool b)
{
	if (list->showName != b) {
		list->showName = b;
		list->reinit();
		emit showNameChanged(b);
	}
}

void ConfigView::setShowRange(bool b)
{
	if (list->showRange != b) {
		list->showRange = b;
		list->reinit();
		emit showRangeChanged(b);
	}
}

void ConfigView::setShowData(bool b)
{
	if (list->showData != b) {
		list->showData = b;
		list->reinit();
		emit showDataChanged(b);
	}
}

void ConfigList::setAllOpen(bool open)
{
	QTreeWidgetItemIterator it(this);

	while (*it) {
		(*it)->setExpanded(open);

		++it;
	}
}

void ConfigView::updateList()
{
	ConfigView* v;

	for (v = viewList; v; v = v->nextView)
		v->list->updateList();
}

void ConfigView::updateListAll(void)
{
	ConfigView* v;

	for (v = viewList; v; v = v->nextView)
		v->list->updateListAll();
}

ConflictsView::ConflictsView(QWidget* parent, const char *name)
	: Parent(parent)
{
	currentSelectedMenu = nullptr;
	setObjectName(name);
	QHBoxLayout *horizontalLayout = new QHBoxLayout(this);
	QVBoxLayout *verticalLayout = new QVBoxLayout();
	verticalLayout->setContentsMargins(0, 0, 0, 0);
	conflictsToolBar = new QToolBar("ConflictTools", this);

	// toolbar buttons [n] [m] [y] [calculate fixes] [remove]
	QAction *addSymbol = new QAction("Add Symbol");
	QAction *setConfigSymbolAsNo = new QAction("N");
	QAction *setConfigSymbolAsModule = new QAction("M");
	QAction *setConfigSymbolAsYes = new QAction("Y");
	fixConflictsAction_ = new QAction("Calculate Fixes");
	QAction *removeSymbol = new QAction("Remove Symbol");
#ifdef CONFIGFIX_TEST
	QAction *testConflictAction = new QAction("Test Random Conflict");
#endif

	// If you change the order of buttons here, change the code where
	// module button was disabled if symbol is boolean, selecting module
	// button depends on a specific index in list of action
	fixConflictsAction_->setCheckable(false);
	conflictsToolBar->addAction(addSymbol);
	conflictsToolBar->addAction(setConfigSymbolAsNo);
	conflictsToolBar->addAction(setConfigSymbolAsModule);
	conflictsToolBar->addAction(setConfigSymbolAsYes);
	conflictsToolBar->addAction(fixConflictsAction_);
	conflictsToolBar->addAction(removeSymbol);
#ifdef CONFIGFIX_TEST
	conflictsToolBar->addAction(testConflictAction);
#endif
	verticalLayout->addWidget(conflictsToolBar);

	connect(addSymbol, SIGNAL(triggered(bool)), SLOT(addSymbol()));
	connect(setConfigSymbolAsNo, SIGNAL(triggered(bool)), SLOT(changeToNo()));
	connect(setConfigSymbolAsModule, SIGNAL(triggered(bool)), SLOT(changeToModule()));
	connect(setConfigSymbolAsYes, SIGNAL(triggered(bool)), SLOT(changeToYes()));
	connect(removeSymbol, SIGNAL(triggered(bool)), SLOT(removeSymbol()));
	connect(this, SIGNAL(resultsReady()), SLOT(updateResults()));
	// connect clicking 'calculate fixes' to 'change all symbol values to fix all conflicts'
	// no longer used anymore for now.
	connect(fixConflictsAction_, SIGNAL(triggered(bool)), SLOT(calculateFixes()));
#ifdef CONFIGFIX_TEST
	connect(fixConflictsAction_, SIGNAL(triggered(bool)), SLOT(switchTestingMode()));
	connect(testConflictAction, SIGNAL(triggered(bool)), SLOT(testRandomConflict()));
#endif

	conflictsTable = (QTableWidget*) new droppableView(this);
	conflictsTable->setRowCount(0);
	conflictsTable->setColumnCount(3);
	conflictsTable->setSelectionBehavior(QAbstractItemView::SelectRows);

	conflictsTable->setHorizontalHeaderLabels(QStringList()  << "Option" << "Wanted value" << "Current value" );
	verticalLayout->addWidget(conflictsTable);

	conflictsTable->setDragDropMode(QAbstractItemView::DropOnly);
	setAcceptDrops(true);

	connect(conflictsTable, SIGNAL(cellClicked(int, int)), SLOT(cellClicked(int,int)));
	horizontalLayout->addLayout(verticalLayout);

	// populate the solution view on the right hand side:
	QVBoxLayout *solutionLayout = new QVBoxLayout();
	solutionLayout->setContentsMargins(0, 0, 0, 0);
	solutionSelector = new QComboBox();
	connect(solutionSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
		[=](int index){ changeSolutionTable(index); });
	solutionTable = new QTableWidget();
	solutionTable->setRowCount(0);
	solutionTable->setColumnCount(2);
	solutionTable->setHorizontalHeaderLabels(QStringList()  << "Symbol" << "New Value" );

	applyFixButton = new QPushButton("Apply Selected solution");
	connect(applyFixButton, SIGNAL(clicked(bool)), SLOT(applyFixButtonClick()));

	numSolutionLabel = new QLabel("Solutions:");
	solutionLayout->addWidget(numSolutionLabel);
	solutionLayout->addWidget(solutionSelector);
	solutionLayout->addWidget(solutionTable);
	solutionLayout->addWidget(applyFixButton);

	horizontalLayout->addLayout(solutionLayout);

}
void QTableWidget::dropEvent(QDropEvent *event)
{
}

void ConflictsView::changeToNo()
{
	QItemSelectionModel *select = conflictsTable->selectionModel();

	if (select->hasSelection()) {
		QModelIndexList rows = select->selectedRows();

		for (int i = 0;i < rows.count(); i++) {
			conflictsTable->item(rows[i].row(),1)->setText("NO");
		}
	}
}

void ConflictsView::applyFixButtonClick()
{
	signed int solution_number = solutionSelector->currentIndex();

	if (solution_number == -1 || solution_output == NULL) {
		return;
	}

	struct sfix_list * selected_solution = select_solution(solution_output, solution_number);

	apply_fix(selected_solution);
	ConfigView::updateListAll();
	UpdateConflictsViewColorization();
}

void ConflictsView::changeToYes()
{
	QItemSelectionModel *select = conflictsTable->selectionModel();

	if (select->hasSelection()) {
		QModelIndexList rows = select->selectedRows();

		for (int i = 0;i < rows.count(); i++) {
			conflictsTable->item(rows[i].row(),1)->setText("YES");
		}
	}

}
void ConflictsView::changeToModule()
{
	QItemSelectionModel *select = conflictsTable->selectionModel();

	if (select->hasSelection()) {
		QModelIndexList rows = select->selectedRows();

		for (int i = 0;i < rows.count(); i++) {
			conflictsTable->item(rows[i].row(),1)->setText("MODULE");
		}
	}

}

void ConflictsView::menuChanged1(struct menu *m)
{
	currentSelectedMenu = m;
}

void ConflictsView::addSymbol()
{
	addSymbol(currentSelectedMenu);
}

void ConflictsView::selectedChanged(QList<QTreeWidgetItem *> selection)
{
	currentSelection = selection;

}

void ConflictsView::addSymbol(struct menu *m)
{
	if (m != nullptr) {
		if (m->sym != nullptr) {
			struct symbol *sym = m->sym;
			tristate currentval = sym_get_tristate_value(sym);
			// if symbol is not added yet:
			QAbstractItemModel* tableModel = conflictsTable->model();
			QModelIndexList matches = tableModel->match(tableModel->index(0,0), Qt::DisplayRole, sym->name );

			if (matches.isEmpty()) {
				conflictsTable->insertRow(conflictsTable->rowCount());
				conflictsTable->setItem(conflictsTable->rowCount()-1,0,new QTableWidgetItem(sym->name));
				conflictsTable->setItem(conflictsTable->rowCount()-1,1,new QTableWidgetItem(tristate_value_to_string(currentval)));
				conflictsTable->setItem(conflictsTable->rowCount()-1,2,new QTableWidgetItem(tristate_value_to_string(currentval)));
			} else {
				conflictsTable->item(matches[0].row(),2)->setText(tristate_value_to_string(currentval));
			}
			conflictsTable->resizeColumnsToContents();
		}
	}
}

void ConflictsView::addSymbolFromContextMenu()
{
	struct menu *menu;

	if (currentSelection.count() == 0)
		return;

	for (auto el: currentSelection) {
		ConfigItem* item = (ConfigItem*)el;

		if (!item) {
			continue;
		}

		menu = item->menu;
		addSymbol(menu);
	}
}

void ConflictsView::removeSymbol()
{
	QItemSelectionModel *select = conflictsTable->selectionModel();
	QAbstractItemModel *itemModel = select->model();

	if (select->hasSelection()) {
		QModelIndexList rows = select->selectedRows();
		itemModel->removeRows(rows[0].row(),rows.size());
	}
}

void ConflictsView::cellClicked(int row, int column)
{
	auto itemText = conflictsTable->item(row,0)->text().toUtf8().data();


	struct symbol *sym = sym_find(itemText);
	if (sym == NULL) {
		return;
	}
	struct property* prop = sym->prop;
	struct menu* men = prop->menu;
	// uncommenting following like somehow disables click signal of 'apply selected solution'
	if (sym->type == symbol_type::S_BOOLEAN) {
		conflictsToolBar->actions()[2]->setDisabled(true);
	} else {
		conflictsToolBar->actions()[2]->setDisabled(false);
	}
	emit(conflictSelected(men));
}

void ConflictsView::changeSolutionTable(int solution_number)
{
	if (solution_output == nullptr || solution_number < 0) {
		return;
	}

	struct sfix_list* selected_solution = select_solution(solution_output, solution_number);
	current_solution_number = solution_number;
	solutionTable->setRowCount(0);

	for (unsigned int i = 0; i <selected_solution->size; i++) {
		solutionTable->insertRow(solutionTable->rowCount());

		struct symbol_fix* cur_symbol = select_symbol(selected_solution,i);
		QTableWidgetItem* symbol_name = new QTableWidgetItem(cur_symbol->sym->name);

		solutionTable->setItem(solutionTable->rowCount()-1,0,symbol_name);

		if (cur_symbol->type == symbolfix_type::SF_BOOLEAN) {
			QTableWidgetItem* symbol_value =
			   new QTableWidgetItem(
				 tristate_value_to_string(cur_symbol->tri));
			solutionTable->setItem(
			      solutionTable->rowCount() - 1,
			      1,
			      symbol_value);
		} else if (cur_symbol->type == symbolfix_type::SF_NONBOOLEAN) {
			QTableWidgetItem* symbol_value =
			   new QTableWidgetItem(cur_symbol->nb_val.s);
			solutionTable->setItem(
			      solutionTable->rowCount() - 1,
			      1,
			      symbol_value);
		} else {
			QTableWidgetItem* symbol_value =
			   new QTableWidgetItem(cur_symbol->disallowed.s);
			solutionTable->setItem(
			      solutionTable->rowCount() - 1,
			      1,
			      symbol_value);
		}
	}
	solutionTable->resizeColumnsToContents();
	UpdateConflictsViewColorization();
}

void ConflictsView::UpdateConflictsViewColorization(void)
{
	auto green = QColor(0, 170, 0);
	auto red = QColor(255, 0, 0);
	auto grey = QColor(180, 180, 180);

	if (solutionTable->rowCount() == 0 || current_solution_number < 0)
		return;

	for (int i = 0; i < solutionTable->rowCount(); i++) {
		QTableWidgetItem *symbol = solutionTable->item(i, 0);
		struct sfix_list * selected_solution =
		   select_solution(solution_output, current_solution_number);
		struct symbol_fix* cur_symbol = select_symbol(selected_solution,i);

		// Red: symbol is editable but value is not from solution
		// Green: symbol is editable and value is from solution
		// Grey: symbol is not editable and value is not target value
		// Green: symbol is not editable and value is target value
		auto editable = sym_string_within_range(cur_symbol->sym,
		      tristate_value_to_string(cur_symbol->tri).toStdString().c_str());
		auto _symbol = solutionTable->item(i,0)->text().toUtf8().data();
		struct symbol* sym_ = sym_find(_symbol);

		tristate current_value_of_symbol = sym_get_tristate_value(sym_);
		tristate target_value_of_symbol = string_value_to_tristate(solutionTable->item(i,1)->text());
		bool symbol_value_same_as_target = current_value_of_symbol == target_value_of_symbol;

		if (editable && !symbol_value_same_as_target) {
			symbol->setForeground(red);
		} else if (editable && symbol_value_same_as_target) {
			symbol->setForeground(green);
		} else if (!editable && !symbol_value_same_as_target) {
			symbol->setForeground(grey);
		} else if (!editable && symbol_value_same_as_target) {
			symbol->setForeground(green);
		}
	}
}

void ConflictsView::runSatConfAsync()
{
	// loop through the rows in conflicts table adding each row into the array:
	struct symbol_dvalue* p = nullptr;

	p = static_cast<struct symbol_dvalue*>(calloc(conflictsTable->rowCount(),sizeof(struct symbol_dvalue)));

	if (!p)
		return;

	struct sdv_list *wanted_symbols = sdv_list_init();

	for (int i = 0; i < conflictsTable->rowCount(); i++) {

		struct symbol_dvalue *tmp = (p+i);
		auto _symbol = conflictsTable->item(i,0)->text().toUtf8().data();
		struct symbol* sym = sym_find(_symbol);

		tmp->sym = sym;
		tmp->type = static_cast<symboldv_type>(sym->type == symbol_type::S_BOOLEAN?0:1);
		tmp->tri = string_value_to_tristate(conflictsTable->item(i,1)->text());
		sdv_list_add(wanted_symbols,tmp);
	}

	fixConflictsAction_->setText("Cancel");
#ifdef CONFIGFIX_TEST
	clock_t start, end;
	double time = 0.0;
	start = clock();
#endif
	struct sfl_list *ret = run_satconf(wanted_symbols);
#ifdef CONFIGFIX_TEST
	end = clock();
	time = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("Conflict resolution time = %.6f secs.\n\n", time);
	// result column 10 - Resolution time
	str_printf(&result_string, "%.6f,", time); 
#endif
	solution_output = ret;
	// sfix_list_free(ret);
	free(p);

	emit resultsReady();
#ifdef CONFIGFIX_TEST
	if (solution_output == nullptr || solution_output->size == 0)
		// result column 11 - No. diagnoses (0 in this case)
		append_result("0,");
	else
		// result columns 11 - No. diagnoses (non-zero), 12 - Comment
		str_printf(&result_string, "%i,,", solution_output->size);
#endif
	//DEBUG
	printf("Results ready:\n");
	sfl_node *node;
	sfl_list_for_each(node, ret)
		print_diagnosis_symbol(node->elem);
	//DEBUG
	{
		std::lock_guard<std::mutex> lk{satconf_mutex};
		satconf_cancelled = true;
	}

	satconf_cancellation_cv.notify_one();
}

void ConflictsView::updateResults(void)
{
	fixConflictsAction_->setText("Calculate Fixes");

	// TODO solution_output = nullptr, solution_number = -1
	if (!(solution_output == nullptr || solution_output->size == 0)) {
		solutionSelector->clear();

		for (unsigned int i = 0; i < solution_output->size; i++) {
			solutionSelector->addItem(QString::number(i+1));
		}

		numSolutionLabel->setText(QString("Solutions: (%1) found").arg(solution_output->size));
		changeSolutionTable(0);
	}

	if (runSatConfAsyncThread->joinable()) {
		runSatConfAsyncThread->join();
		delete runSatConfAsyncThread;
		runSatConfAsyncThread  = nullptr;
	}
}

void ConflictsView::calculateFixes(void)
{
	if (conflictsTable->rowCount() == 0)
		return;

	if (runSatConfAsyncThread == nullptr) {
		std::unique_lock<std::mutex> lk{satconf_mutex};

		numSolutionLabel->setText(QString("Solutions: "));
		solutionSelector->clear();
		solutionTable->setRowCount(0);
		satconf_cancelled = false;
		runSatConfAsyncThread = new std::thread(&ConflictsView::runSatConfAsync,this);
	} else {
		interrupt_rangefix();
		std::unique_lock<std::mutex> lk{satconf_mutex};
		satconf_cancellation_cv.wait(lk,[this] {
			return satconf_cancelled == true;
		});
	}
	// FIXME measure conflict resolution time
	// FIXME add no. fixes to result
}


/* ---- start qconf.cc additions ---- */
void ConflictsView::switchTestingMode()
{
/*
 * Keeping this slot empty for 'make xconfig'
 * since I cannot figure out how to set
 * -DCONFIGFIX_TEST during qconf.moc compilation.
 */
#ifdef CONFIGFIX_TEST
	/*
	 * Switch to MANUAL_TESTING mode if solution 
	 * for manually created conflict is found.
	 */
	if (solution_output != nullptr && solution_output->size > 0) {
		testing_mode = MANUAL_TESTING;
		testConflictAction->setText("Verify Fixes");
	}
#endif
}

/*
 * In RANDOM_TESTING mode, generate random conflict
 * and calculate fixes for it.
 * In both RANDOM_TESTING and MANUAL_TESTING mode
 * verify fixes, if they are present.
 */
void ConflictsView::testRandomConflict(void)
{
/*
 * Keeping this slot empty for 'make xconfig'
 * since I cannot figure out how to set
 * -DCONFIGFIX_TEST during qconf.moc compilation.
 */
#ifdef CONFIGFIX_TEST

	/* In MANUAL_TESTING mode, only verify already calculated diagnoses */
	if (testing_mode == MANUAL_TESTING) {
		verifyDiagnoses(""); // result prefix not needed
		/* 
	     * If the conflict was created manually and resolved
	 	 * by clicking the 'Verify Fixes' button, switch to 
		 * RANDOM_TESTING mode.
		 */
		testConflictAction->setText("Test Random Conflict");
		testing_mode = RANDOM_TESTING;
		return;
	} 

	/* 
	 * In RANDOM_TESTING mode, generate and resolve a number
	 * of random conflicts, verify fixes, and save results to file.
	 */
 	if (testing_mode == RANDOM_TESTING) {

		// generate conflists with MIN_CONFLICT_SIZE..MAX_CONFLICT_SIZE symbols,
		for (conflict_size=MIN_CONFLICT_SIZE; conflict_size<=MAX_CONFLICT_SIZE; conflict_size++) {
		// re-use a uniform distribution of conflict candidates for every conflict size
		std::uniform_int_distribution<int> dist(1, no_conflict_candidates);
		// NO_CONFLICTS of each size
		for (int i=0; i<NO_CONFLICTS; i++) 
		{

			/* If the current conflict_dir exists (conflict.txt was saved 
			   there), this call will re-calculate that value */
			conflict_dir = get_conflict_dir();

			// initialise result string
			result_string = str_new();
			// column 1 - Architecture
			append_result(getenv("ARCH"));
			// column 2 - Configuration sample
			append_result((char*) conf_get_configname());
			// column 3 - KCONFIG_PROBABILITY used to generate the sample
			append_result(getenv("CONFIGFIX_TEST_PROBABILITY"));
			// column 4 - Symbol count
			str_printf(&result_string, "%i,", sym_count); 
			// column 5 - Tristates
			if (tristates)
				append_result("YES");
			else
				append_result("NO");
			// column 6 - No. symbols = YES | MOD
			str_printf(&result_string, "%i,", no_enabled_symbols); 
			// column 7 - No. conflict candidates
			str_printf(&result_string, "%i,", no_conflict_candidates); 

			// generate conflict & find fixes
			generateConflict(dist); //);
			saveConflict();
			calculateFixes();
			// output result and continue if no solution found
			if (solution_output == nullptr || solution_output->size == 0) {
				// set remaining result columns to "-"
				// column 13 - Diag. index
				append_result("-");
				// column 14 - Diag. size
				append_result("-");
				// column 15 - Resolved
				append_result("-");
				// column 16 - Applied
				append_result("-");

				output_result();
			} 
			// otherwise verify diagnoses
			else {
				// common result prefix for all diagnoses
				gstr result_prefix = str_new();
				str_append(&result_prefix, str_get(&result_string));
				str_free(&result_string);

				// verify diagnoses
				verifyDiagnoses(str_get(&result_prefix));
				str_free(&result_prefix);
			}

		}
		}
	
		printf("Test run has finished\n");
	}



	// output result and return if no solution found
	// if (solution_output == nullptr || solution_output->len == 0) {
	// 	output_result();
	// 	return;
	// } 
	// otherwise verify diagnoses - both RANDOM_TESTING and MANUAL_TESTING
	// else {
	// 	// common result prefix for all diagnoses
	// 	gstr result_prefix = str_new();
	// 	str_append(&result_prefix, str_get(&result_string));
	// 	str_free(&result_string);

	// 	// both RANDOM_TESTING and MANUAL_TESTING - verify diagnoses
	// 	verifyDiagnoses(str_get(&result_prefix));

	// 	// reset configuration
	// 	printf("Restoring initial configuration... ");
	// 	emit(refreshMenu());
	// 	config_reset();
	// 	emit(refreshMenu());
	// 	if (config_compare(initial_config) != 0)
	// 		printf("ERROR: configuration and backup mismatch\n");
	// 	else 
	// 		printf("OK\n");

	// 	// FIXME move to verifyDiagnoses?
	// 	// output_result();
	// }

#endif
}

void ConflictsView::generateConflict(std::uniform_int_distribution<int> dist) //void)
{
/*
 * Keeping this slot empty for 'make xconfig'
 * since I cannot figure out how to set
 * -DCONFIGFIX_TEST during qconf.moc compilation.
 */
#ifdef CONFIGFIX_TEST
	// clear conflicts table
	conflictsTable->clearContents();
	conflictsTable->setRowCount(0);

	ConfigItem* item;
	struct symbol *sym;
	// // while (conflictsTable->rowCount() < conflict_size) 
	// // {
	// // }

	int index = 0; // = no_conflict_candidates / 2;
	// for (int z=0;z<20;z++) {
	// index = dist(gen);
	// sym = get_conflict_sym(index);
	// printf("Symbol with index %i %s %s conflict\n",
	// 	index, sym_get_name(sym), 
	// 	sym_has_conflict(sym) ? "HAS" : "DOES NOT HAVE");
	// }
	// getchar();

	while (conflictsTable->rowCount() < conflict_size) 
	{
		// // iterate menu items
		// QTreeWidgetItemIterator it(configList);
		// ConfigItem* item;
		// struct symbol *sym;

		// while (*it) 
		// {
		// 	item = (ConfigItem*)(*it);
		// 	// skip items without menus or symbols
		// 	if (!item->menu) {
		// 		++it;
		// 		continue;
		// 	}
		// 	sym = item->menu->sym;
		// 	if (!sym) {
		// 		++it;
		// 		continue;
		// 	}

		// 	// consider only conflicting items
		// 	if (sym_has_conflict(sym)) { 


				// FIXME - "prefectly random selection"
				// if (rand() < 1000000) {

		index = dist(rng);
		//DEBUG
		printf("Random index = %i\n",index);
		//DEBUG
		item = get_conflict_item(index, configList);
		addSymbol(item->menu);
		//DEBUG
		// getchar();
		//DEBUG
		// set target value (one of the currently blocked)
		tristate current = sym_get_tristate_value(item->menu->sym);
		tristate target = random_blocked_value(item->menu->sym); //current == yes ? no : yes;
		conflictsTable->setItem(conflictsTable->rowCount()-1, 1, 
			new QTableWidgetItem(tristate_value_to_string(target)));
			// 	}
			// }

			// if (conflictsTable->rowCount() == conflict_size)
			// 	break;
			// else
			// 	++it;
		// }
	}

	conflictsTable->resizeColumnsToContents();
	conflictsTable->repaint();

	//DEBUG
	// printf("\n");
	// getchar();
	//DEBUG
	if (conflictsTable->rowCount() == 0) {
		printf("ERROR: No conflict could be generated\n");
		return;	
	} else {
		// print conflict
		printf("Conflict (%i symbols)\n", conflictsTable->rowCount());
		printf("------------------------------\n");
		for (int i = 0; i < conflictsTable->rowCount(); i++) {
			auto _symbol = conflictsTable->item(i,0)->text().toUtf8().data();
			struct symbol *sym = sym_find(_symbol);

			if (!sym) {
				printf("ERROR: conflict symbol %s not found\n", _symbol);
				return;
			} else if (!sym->name) {
				printf("ERROR: conflict symbol %s not found\n", _symbol);
				return;
			}

			printf("%s: %s => %s\n", 
				sym->name, 
				sym_get_string_value(sym),
				tristate_get_char(string_value_to_tristate(
					conflictsTable->item(i,1)->text())));
		}
	}
#endif
}

void ConflictsView::saveConflict(void)
{
/*
 * Keeping this slot empty for 'make xconfig'
 * since I cannot figure out how to set
 * -DCONFIGFIX_TEST during qconf.moc compilation.
 */
#ifdef CONFIGFIX_TEST
	
	// create directory
	QDir().mkpath(conflict_dir);

	// construct filename
	// char filename[
	// 	strlen(conflict_dir) 
	//     + strlen("conflict.txt")  + 1];
	// sprintf(filename, "%sconflict.txt", conflict_dir);
	gstr filename = str_new();
	str_append(&filename, conflict_dir);
	str_append(&filename, "conflict.txt");

	// create file
    FILE* f = fopen(str_get(&filename), "w");
    if(!f) {
        printf("ERROR: could not create conflict file\n");
		str_free(&filename);
		return;
    }

	// compare conlict table with conflict_size
	if (conflictsTable->rowCount() != conflict_size) 
		printf("WARNING: conlicts table row count and conflict_size parameter mismatch");

	// iterate conflicts table, write symbols to file
	for (int i = 0; i < conflictsTable->rowCount(); i++)
	{
		auto _symbol = conflictsTable->item(i,0)->text().toUtf8().data();
		struct symbol *sym = sym_find(_symbol);

		if (!sym) {
			printf("ERROR: conflict symbol %s not found\n", _symbol);
			return;
		} else if (!sym->name) {
			printf("ERROR: conflict symbol %s not found\n", _symbol);
			return;
		}

		// output current and target values
		fprintf(f, "%s: %s => %s\n", 
 			sym->name, 
			sym_get_string_value(sym),
			tristate_get_char(string_value_to_tristate(
				conflictsTable->item(i,1)->text())));

		// output direct dependencies
		gstr dir_deps = str_new();
		str_append(&dir_deps, "    Direct dependencies:  ");
		expr_gstr_print(sym->dir_dep.expr, &dir_deps);
		str_append(&dir_deps, "\n");
		fprintf(f, str_get(&dir_deps));
		str_free(&dir_deps);

		// output reverese dependencies
		if (sym->rev_dep.expr) {
			gstr rev_deps = str_new();
			str_append(&rev_deps, "    Reverse dependencies: ");
		    expr_gstr_print(sym->rev_dep.expr, &rev_deps);
			str_append(&rev_deps, "\n");
			fprintf(f, str_get(&rev_deps));
			str_free(&rev_deps);
		}

		fprintf(f, "\n");
	}
	
	// result column 8 - Conflict filename
	append_result((char*) str_get(&filename));
	// result column 9 - Conflict size
	str_printf(&result_string, "%i,", conflictsTable->rowCount()); 

	fclose(f);
	printf("\n#\n# conflict saved to %s\n#\n\n", str_get(&filename));
	str_free(&filename);

#endif
}

/*
 * Verify all present diagnoses. 
 * 
 * For every diagnosis, construct and output a result string 
 * assuming that values common to all diagnoses (columns 1-12) 
 * are supplied in the 'result_prefix'.
 */
void ConflictsView::verifyDiagnoses(const char *result_prefix) // const char*?
{
/*
 * Keeping this slot empty for 'make xconfig'
 * since I cannot figure out how to set
 * -DCONFIGFIX_TEST during qconf.moc compilation.
 */
#ifdef CONFIGFIX_TEST

	// GArray *diag, *permutation;
	// int size, perm_count;
	// bool valid_diag;

	// int j;
	for (int i=0; i < solution_output->size; i++) {

		// FIXME conflict path 

		// verify i-th solution
		verify_solution(
			i+1, result_prefix, 
			//g_array_index(solution_output, GArray*, i),
			select_solution(solution_output, i),
			conflictsTable);

		// reset configuration
		printf("\nRestoring initial configuration... ");
		emit(refreshMenu());
		config_reset();
		emit(refreshMenu());
		if (config_compare(initial_config) != 0)
			printf("ERROR: could not reset configuration after verifying diagnosis\n");
		else
			printf("OK\n");
		
	}
#endif
}
/* ---- end qconf.cc additions ---- */

ConfigInfoView::ConfigInfoView(QWidget* parent, const char *name)
	: Parent(parent), sym(0), _menu(0)
{
	setObjectName(name);
	setOpenLinks(false);

	if (!objectName().isEmpty()) {
		configSettings->beginGroup(objectName());
		setShowDebug(configSettings->value("/showDebug", false).toBool());
		configSettings->endGroup();
		connect(configApp, SIGNAL(aboutToQuit()), SLOT(saveSettings()));
	}

	contextMenu = createStandardContextMenu();
	QAction *action = new QAction("Show Debug Info", contextMenu);

	action->setCheckable(true);
	connect(action, SIGNAL(toggled(bool)), SLOT(setShowDebug(bool)));
	connect(this, SIGNAL(showDebugChanged(bool)), action, SLOT(setChecked(bool)));
	action->setChecked(showDebug());
	contextMenu->addSeparator();
	contextMenu->addAction(action);
}

void ConfigInfoView::saveSettings(void)
{
	if (!objectName().isEmpty()) {
		configSettings->beginGroup(objectName());
		configSettings->setValue("/showDebug", showDebug());
		configSettings->endGroup();
	}
}

void ConfigInfoView::setShowDebug(bool b)
{
	if (_showDebug != b) {
		_showDebug = b;
		if (_menu)
			menuInfo();
		else if (sym)
			symbolInfo();
		emit showDebugChanged(b);
	}
}

void ConfigInfoView::setInfo(struct menu *m)
{
	if (_menu == m)
		return;
	_menu = m;
	sym = NULL;
	if (!_menu)
		clear();
	else
		menuInfo();
}

void ConfigInfoView::symbolInfo(void)
{
	QString str;

	str += "<big>Symbol: <b>";
	str += print_filter(sym->name);
	str += "</b></big><br><br>value: ";
	str += print_filter(sym_get_string_value(sym));
	str += "<br>visibility: ";
	str += sym->visible == yes ? "y" : sym->visible == mod ? "m" : "n";
	str += "<br>";
	str += debug_info(sym);

	setText(str);
}

void ConfigInfoView::menuInfo(void)
{
	struct symbol *sym;
	QString info;
	QTextStream stream(&info);

	sym = _menu->sym;
	if (sym) {
		if (_menu->prompt) {
			stream << "<big><b>";
			stream << print_filter(_menu->prompt->text);
			stream << "</b></big>";
			if (sym->name) {
				stream << " (";
				if (showDebug())
					stream << "<a href=\"s" << sym->name << "\">";
				stream << print_filter(sym->name);
				if (showDebug())
					stream << "</a>";
				stream << ")";
			}
		} else if (sym->name) {
			stream << "<big><b>";
			if (showDebug())
				stream << "<a href=\"s" << sym->name << "\">";
			stream << print_filter(sym->name);
			if (showDebug())
				stream << "</a>";
			stream << "</b></big>";
		}
		stream << "<br><br>";

		if (showDebug())
			stream << debug_info(sym);

		struct gstr help_gstr = str_new();

		menu_get_ext_help(_menu, &help_gstr);
		stream << print_filter(str_get(&help_gstr));
		str_free(&help_gstr);
	} else if (_menu->prompt) {
		stream << "<big><b>";
		stream << print_filter(_menu->prompt->text);
		stream << "</b></big><br><br>";
		if (showDebug()) {
			if (_menu->prompt->visible.expr) {
				stream << "&nbsp;&nbsp;dep: ";
				expr_print(_menu->prompt->visible.expr,
					   expr_print_help, &stream, E_NONE);
				stream << "<br><br>";
			}

			stream << "defined at " << _menu->file->name << ":"
			       << _menu->lineno << "<br><br>";
		}
	}

	setText(info);
}

QString ConfigInfoView::debug_info(struct symbol *sym)
{
	QString debug;
	QTextStream stream(&debug);

	stream << "type: ";
	stream << print_filter(sym_type_name(sym->type));
	if (sym_is_choice(sym))
		stream << " (choice)";
	debug += "<br>";
	if (sym->rev_dep.expr) {
		stream << "reverse dep: ";
		expr_print(sym->rev_dep.expr, expr_print_help, &stream, E_NONE);
		stream << "<br>";
	}
	for (struct property *prop = sym->prop; prop; prop = prop->next) {
		switch (prop->type) {
		case P_PROMPT:
		case P_MENU:
			stream << "prompt: <a href=\"m" << sym->name << "\">";
			stream << print_filter(prop->text);
			stream << "</a><br>";
			break;
		case P_DEFAULT:
		case P_SELECT:
		case P_RANGE:
		case P_COMMENT:
		case P_IMPLY:
		case P_SYMBOL:
			stream << prop_get_type_name(prop->type);
			stream << ": ";
			expr_print(prop->expr, expr_print_help,
				   &stream, E_NONE);
			stream << "<br>";
			break;
		case P_CHOICE:
			if (sym_is_choice(sym)) {
				stream << "choice: ";
				expr_print(prop->expr, expr_print_help,
					   &stream, E_NONE);
				stream << "<br>";
			}
			break;
		default:
			stream << "unknown property: ";
			stream << prop_get_type_name(prop->type);
			stream << "<br>";
		}
		if (prop->visible.expr) {
			stream << "&nbsp;&nbsp;&nbsp;&nbsp;dep: ";
			expr_print(prop->visible.expr, expr_print_help,
				   &stream, E_NONE);
			stream << "<br>";
		}
	}
	stream << "<br>";

	return debug;
}

QString ConfigInfoView::print_filter(const QString &str)
{
	QRegExp re("[<>&\"\\n]");
	QString res = str;
	for (int i = 0; (i = res.indexOf(re, i)) >= 0;) {
		switch (res[i].toLatin1()) {
		case '<':
			res.replace(i, 1, "&lt;");
			i += 4;
			break;
		case '>':
			res.replace(i, 1, "&gt;");
			i += 4;
			break;
		case '&':
			res.replace(i, 1, "&amp;");
			i += 5;
			break;
		case '"':
			res.replace(i, 1, "&quot;");
			i += 6;
			break;
		case '\n':
			res.replace(i, 1, "<br>");
			i += 4;
			break;
		}
	}
	return res;
}

void ConfigInfoView::expr_print_help(void *data, struct symbol *sym, const char *str)
{
	QTextStream *stream = reinterpret_cast<QTextStream *>(data);

	if (sym && sym->name && !(sym->flags & SYMBOL_CONST)) {
		*stream << "<a href=\"s" << sym->name << "\">";
		*stream << print_filter(str);
		*stream << "</a>";
	} else {
		*stream << print_filter(str);
	}
}

void ConfigInfoView::clicked(const QUrl &url)
{
	QByteArray str = url.toEncoded();
	const std::size_t count = str.size();
	char *data = new char[count + 1];
	struct symbol **result;
	struct menu *m = NULL;

	if (count < 1) {
		delete[] data;
		return;
	}

	memcpy(data, str.constData(), count);
	data[count] = '\0';

	/* Seek for exact match */
	data[0] = '^';
	strcat(data, "$");
	result = sym_re_search(data);
	if (!result) {
		delete[] data;
		return;
	}

	sym = *result;

	/* Seek for the menu which holds the symbol */
	for (struct property *prop = sym->prop; prop; prop = prop->next) {
		    if (prop->type != P_PROMPT && prop->type != P_MENU)
			    continue;
		    m = prop->menu;
		    break;
	}

	if (!m) {
		/* Symbol is not visible as a menu */
		symbolInfo();
		emit showDebugChanged(true);
	} else {
		emit menuSelected(m);
	}

	free(result);
	delete[] data;
}

void ConfigInfoView::contextMenuEvent(QContextMenuEvent *event)
{
	contextMenu->popup(event->globalPos());
	event->accept();
}

ConfigSearchWindow::ConfigSearchWindow(ConfigMainWindow *parent)
	: Parent(parent), result(NULL)
{
	setObjectName("search");
	setWindowTitle("Search Config");

	QVBoxLayout* layout1 = new QVBoxLayout(this);
	layout1->setContentsMargins(11, 11, 11, 11);
	layout1->setSpacing(6);

	QHBoxLayout* layout2 = new QHBoxLayout();
	layout2->setContentsMargins(0, 0, 0, 0);
	layout2->setSpacing(6);
	layout2->addWidget(new QLabel("Find:", this));
	editField = new QLineEdit(this);
	connect(editField, SIGNAL(returnPressed()), SLOT(search()));
	layout2->addWidget(editField);
	searchButton = new QPushButton("Search", this);
	searchButton->setAutoDefault(false);
	connect(searchButton, SIGNAL(clicked()), SLOT(search()));
	layout2->addWidget(searchButton);
	layout1->addLayout(layout2);

	split = new QSplitter(this);
	split->setOrientation(Qt::Vertical);
	list = new ConfigView(split, "search");
	list->list->mode = listMode;
	info = new ConfigInfoView(split, "search");
	connect(list->list, SIGNAL(menuChanged(struct menu *)),
		info, SLOT(setInfo(struct menu *)));
	connect(list->list, SIGNAL(menuChanged(struct menu *)),
		parent, SLOT(setMenuLink(struct menu *)));
	connect(list->list, SIGNAL(menuChanged(struct menu *)),
		parent, SLOT(conflictSelected(struct menu *)));

	connect(list->list,SIGNAL(UpdateConflictsViewColorization()),SLOT(UpdateConflictsViewColorizationFowarder()));
	layout1->addWidget(split);

	QVariant x, y;
	int width, height;
	bool ok;

	configSettings->beginGroup("search");
	width = configSettings->value("/window width", parent->width() / 2).toInt();
	height = configSettings->value("/window height", parent->height() / 2).toInt();
	resize(width, height);
	x = configSettings->value("/window x");
	y = configSettings->value("/window y");
	if (x.isValid() && y.isValid())
		move(x.toInt(), y.toInt());
	QList<int> sizes = configSettings->readSizes("/split", &ok);
	if (ok)
		split->setSizes(sizes);
	configSettings->endGroup();
	connect(configApp, SIGNAL(aboutToQuit()), SLOT(saveSettings()));
}

void ConfigSearchWindow::UpdateConflictsViewColorizationFowarder(void){
	emit UpdateConflictsViewColorization();
}
void ConfigSearchWindow::saveSettings(void)
{
	if (!objectName().isEmpty()) {
		configSettings->beginGroup(objectName());
		configSettings->setValue("/window x", pos().x());
		configSettings->setValue("/window y", pos().y());
		configSettings->setValue("/window width", size().width());
		configSettings->setValue("/window height", size().height());
		configSettings->writeSizes("/split", split->sizes());
		configSettings->endGroup();
	}
}

void ConfigSearchWindow::search(void)
{
	struct symbol **p;
	struct property *prop;
	ConfigItem *lastItem = NULL;

	free(result);
	list->list->clear();
	info->clear();

	result = sym_re_search(editField->text().toLatin1());
	if (!result)
		return;
	for (p = result; *p; p++) {
		for_all_prompts((*p), prop)
			lastItem = new ConfigItem(list->list, lastItem, prop->menu,
						  menu_is_visible(prop->menu));
	}
}

/*
 * Construct the complete config widget
 */
ConfigMainWindow::ConfigMainWindow(void)
	: searchWindow(0)
{
	bool ok = true;
	QVariant x, y;
	int width, height;
	char title[256];

	QDesktopWidget *d = configApp->desktop();
	snprintf(title, sizeof(title), "%s%s",
		rootmenu.prompt->text,
	#ifdef CONFIGFIX_TEST
		" (ConfigFix testing)"
	#else
		""
	#endif
		);
	setWindowTitle(title);

	width = configSettings->value("/window width", d->width() - 64).toInt();
	height = configSettings->value("/window height", d->height() - 64).toInt();
	resize(width, height);
	x = configSettings->value("/window x");
	y = configSettings->value("/window y");
	if ((x.isValid())&&(y.isValid()))
		move(x.toInt(), y.toInt());

	// set up icons
	ConfigItem::symbolYesIcon = QIcon(QPixmap(xpm_symbol_yes));
	ConfigItem::symbolModIcon = QIcon(QPixmap(xpm_symbol_mod));
	ConfigItem::symbolNoIcon = QIcon(QPixmap(xpm_symbol_no));
	ConfigItem::choiceYesIcon = QIcon(QPixmap(xpm_choice_yes));
	ConfigItem::choiceNoIcon = QIcon(QPixmap(xpm_choice_no));
	ConfigItem::menuIcon = QIcon(QPixmap(xpm_menu));
	ConfigItem::menubackIcon = QIcon(QPixmap(xpm_menuback));

	QWidget *widget = new QWidget(this);
	QVBoxLayout *layout = new QVBoxLayout(widget);
	setCentralWidget(widget);

	split1 = new QSplitter(widget);
	split1->setOrientation(Qt::Horizontal);
	split1->setChildrenCollapsible(false);

	menuView = new ConfigView(widget, "menu");
	menuList = menuView->list;

	split2 = new QSplitter(widget);
	split2->setChildrenCollapsible(false);
	split2->setOrientation(Qt::Vertical);

	// create config tree
	configView = new ConfigView(widget, "config");
	configList = configView->list;

	helpText = new ConfigInfoView(widget, "help");

	layout->addWidget(split2);
	split2->addWidget(split1);
	split1->addWidget(configView);
	split1->addWidget(menuView);
	split2->addWidget(helpText);

	split3 = new QSplitter(split2);
	split3->setOrientation(Qt::Vertical);
	conflictsView = new ConflictsView(split3, "help");
	/* conflictsSelected signal in conflictsview triggers when a conflict is selected
		 in the view. this line connects that event to conflictselected event in mainwindow
		 which updates the selection to match (in the configlist) the symbol that was selected.
	*/
	connect(conflictsView,SIGNAL(conflictSelected(struct menu *)),SLOT(conflictSelected(struct menu *)));
	connect(conflictsView,SIGNAL(refreshMenu()),SLOT(refreshMenu()));
	connect(menuList,SIGNAL(UpdateConflictsViewColorization()),conflictsView,SLOT(UpdateConflictsViewColorization()));
	connect(configList,SIGNAL(UpdateConflictsViewColorization()),conflictsView,SLOT(UpdateConflictsViewColorization()));
	setTabOrder(configList, helpText);

	configList->setFocus();
#ifdef CONFIGFIX_TEST
	conflictsView->configList = configList;
#endif

	// menu = menuBar();
	toolBar = new QToolBar("Tools", this);
	addToolBar(toolBar);


	backAction = new QAction(QPixmap(xpm_back), "Back", this);
	connect(backAction, SIGNAL(triggered(bool)), SLOT(goBack()));

	QAction *quitAction = new QAction("&Quit", this);
	quitAction->setShortcut(Qt::CTRL + Qt::Key_Q);
	connect(quitAction, SIGNAL(triggered(bool)), SLOT(close()));

	QAction *loadAction = new QAction(QPixmap(xpm_load), "&Load", this);
	loadAction->setShortcut(Qt::CTRL + Qt::Key_L);
	connect(loadAction, SIGNAL(triggered(bool)), SLOT(loadConfig()));

	saveAction = new QAction(QPixmap(xpm_save), "&Save", this);
	saveAction->setShortcut(Qt::CTRL + Qt::Key_S);
	connect(saveAction, SIGNAL(triggered(bool)), SLOT(saveConfig()));

	conf_set_changed_callback(conf_changed);

	// Set saveAction's initial state
	conf_changed();
	configname = xstrdup(conf_get_configname());

	QAction *saveAsAction = new QAction("Save &As...", this);
	  connect(saveAsAction, SIGNAL(triggered(bool)), SLOT(saveConfigAs()));
	QAction *searchAction = new QAction("&Find", this);
	searchAction->setShortcut(Qt::CTRL + Qt::Key_F);
	  connect(searchAction, SIGNAL(triggered(bool)), SLOT(searchConfig()));
	singleViewAction = new QAction(QPixmap(xpm_single_view), "Single View", this);
	singleViewAction->setCheckable(true);
	  connect(singleViewAction, SIGNAL(triggered(bool)), SLOT(showSingleView()));
	splitViewAction = new QAction(QPixmap(xpm_split_view), "Split View", this);
	splitViewAction->setCheckable(true);
	  connect(splitViewAction, SIGNAL(triggered(bool)), SLOT(showSplitView()));
	fullViewAction = new QAction(QPixmap(xpm_tree_view), "Full View", this);
	fullViewAction->setCheckable(true);
	  connect(fullViewAction, SIGNAL(triggered(bool)), SLOT(showFullView()));



	QAction *showNameAction = new QAction("Show Name", this);
	  showNameAction->setCheckable(true);
	  connect(showNameAction, SIGNAL(toggled(bool)), configView, SLOT(setShowName(bool)));
	  showNameAction->setChecked(configView->showName());
	QAction *showRangeAction = new QAction("Show Range", this);
	  showRangeAction->setCheckable(true);
	  connect(showRangeAction, SIGNAL(toggled(bool)), configView, SLOT(setShowRange(bool)));
	QAction *showDataAction = new QAction("Show Data", this);
	  showDataAction->setCheckable(true);
	  connect(showDataAction, SIGNAL(toggled(bool)), configView, SLOT(setShowData(bool)));

	QActionGroup *optGroup = new QActionGroup(this);
	optGroup->setExclusive(true);
	connect(optGroup, SIGNAL(triggered(QAction*)), configList,
		SLOT(setOptionMode(QAction *)));
	connect(optGroup, SIGNAL(triggered(QAction *)), menuList,
		SLOT(setOptionMode(QAction *)));

	ConfigList::showNormalAction = new QAction("Show Normal Options", optGroup);
	ConfigList::showNormalAction->setCheckable(true);
	ConfigList::showAllAction = new QAction("Show All Options", optGroup);
	ConfigList::showAllAction->setCheckable(true);
	ConfigList::showPromptAction = new QAction("Show Prompt Options", optGroup);
	ConfigList::showPromptAction->setCheckable(true);
	ConfigList::addSymbolsFromContextMenu = new QAction("Add symbol from context menu");
	connect(ConfigList::addSymbolsFromContextMenu, SIGNAL(triggered()),conflictsView, SLOT(addSymbolFromContextMenu()));

	QAction *showDebugAction = new QAction("Show Debug Info", this);
	  showDebugAction->setCheckable(true);
	  connect(showDebugAction, SIGNAL(toggled(bool)), helpText, SLOT(setShowDebug(bool)));
	  showDebugAction->setChecked(helpText->showDebug());

	QAction *showIntroAction = new QAction("Introduction", this);
	  connect(showIntroAction, SIGNAL(triggered(bool)), SLOT(showIntro()));
	QAction *showAboutAction = new QAction("About", this);
	  connect(showAboutAction, SIGNAL(triggered(bool)), SLOT(showAbout()));

	// init tool bar
	QToolBar *toolBar = addToolBar("Tools");
	toolBar->addAction(backAction);
	toolBar->addSeparator();
	toolBar->addAction(loadAction);
	toolBar->addAction(saveAction);
	toolBar->addSeparator();
	toolBar->addAction(singleViewAction);
	toolBar->addAction(splitViewAction);
	toolBar->addAction(fullViewAction);
	toolBar->addSeparator();

	// create file menu
	QMenu *menu = menuBar()->addMenu("&File");
	menu->addAction(loadAction);
	menu->addAction(saveAction);
	menu->addAction(saveAsAction);
	menu->addSeparator();
	menu->addAction(quitAction);

	// create edit menu
	menu = menuBar()->addMenu("&Edit");
	menu->addAction(searchAction);

	// create options menu
	menu = menuBar()->addMenu("&Option");
	menu->addAction(showNameAction);
	menu->addAction(showRangeAction);
	menu->addAction(showDataAction);
	menu->addSeparator();
	menu->addActions(optGroup->actions());
	menu->addSeparator();
	menu->addAction(showDebugAction);

	// create help menu
	menu = menuBar()->addMenu("&Help");
	menu->addAction(showIntroAction);
	menu->addAction(showAboutAction);

	connect (helpText, SIGNAL (anchorClicked (const QUrl &)),
		 helpText, SLOT (clicked (const QUrl &)) );

	connect(configList, SIGNAL(menuChanged(struct menu *)),
		helpText, SLOT(setInfo(struct menu *)));
	connect(configList, SIGNAL(menuSelected(struct menu *)),
		SLOT(changeMenu(struct menu *)));
	connect(configList, SIGNAL(itemSelected(struct menu *)),
		SLOT(changeItens(struct menu *)));
	connect(configList, SIGNAL(parentSelected()),
		SLOT(goBack()));
	connect(menuList, SIGNAL(menuChanged(struct menu *)),
		helpText, SLOT(setInfo(struct menu *)));
	connect(menuList, SIGNAL(menuSelected(struct menu *)),
		SLOT(changeMenu(struct menu *)));

	//pass the list of selected items in configList to conflictsView so that
	//when right click 'add symbols to conflict' is clicked it will be added
	//to the list
	connect(configList, SIGNAL(selectedChanged(QList<QTreeWidgetItem*>)),
		conflictsView, SLOT(selectedChanged(QList<QTreeWidgetItem*>)));
	connect(configList, SIGNAL(menuChanged(struct menu *)),
		conflictsView, SLOT(menuChanged1(struct menu *)));
	connect(configList, SIGNAL(gotFocus(struct menu *)),
		helpText, SLOT(setInfo(struct menu *)));
	connect(menuList, SIGNAL(gotFocus(struct menu *)),
		helpText, SLOT(setInfo(struct menu *)));
	connect(menuList, SIGNAL(gotFocus(struct menu *)),
		SLOT(listFocusChanged(void)));
	connect(helpText, SIGNAL(menuSelected(struct menu *)),
		SLOT(setMenuLink(struct menu *)));

	QString listMode = configSettings->value("/listMode", "symbol").toString();
	if (listMode == "single")
		showSingleView();
	else if (listMode == "full")
		showFullView();
	else /*if (listMode == "split")*/
		showSplitView();

	// UI setup done, restore splitter positions
	QList<int> sizes = configSettings->readSizes("/split1", &ok);
	if (ok)
		split1->setSizes(sizes);

	sizes = configSettings->readSizes("/split2", &ok);
	if (ok)
		split2->setSizes(sizes);
}

void ConfigMainWindow::loadConfig(void)
{
	QString str;
	QByteArray ba;
	const char *name;

	str = QFileDialog::getOpenFileName(this, "", configname);
	if (str.isNull())
		return;

	ba = str.toLocal8Bit();
	name = ba.data();

	if (conf_read(name))
		QMessageBox::information(this, "qconf", "Unable to load configuration!");

	free(configname);
	configname = xstrdup(name);

	ConfigView::updateListAll();
}

bool ConfigMainWindow::saveConfig(void)
{
	if (conf_write(configname)) {
		QMessageBox::information(this, "qconf", "Unable to save configuration!");
		return false;
	}
	conf_write_autoconf(0);

	return true;
}

void ConfigMainWindow::saveConfigAs(void)
{
	QString str;
	QByteArray ba;
	const char *name;

	str = QFileDialog::getSaveFileName(this, "", configname);
	if (str.isNull())
		return;

	ba = str.toLocal8Bit();
	name = ba.data();

	if (conf_write(name)) {
		QMessageBox::information(this, "qconf", "Unable to save configuration!");
	}
	conf_write_autoconf(0);

	free(configname);
	configname = xstrdup(name);
}

void ConfigMainWindow::searchConfig(void)
{
	if (!searchWindow){
		searchWindow = new ConfigSearchWindow(this);
		connect(searchWindow,SIGNAL(UpdateConflictsViewColorization()),conflictsView,SLOT(UpdateConflictsViewColorization()));
	}
	searchWindow->show();
}

void ConfigMainWindow::changeItens(struct menu *menu)
{
	configList->setRootMenu(menu);
}

void ConfigMainWindow::changeMenu(struct menu *menu)
{
	menuList->setRootMenu(menu);
}

void ConfigMainWindow::setMenuLink(struct menu *menu)
{
	struct menu *parent;
	ConfigList* list = NULL;
	ConfigItem* item;

	if (configList->menuSkip(menu))
		return;

	switch (configList->mode) {
	case singleMode:
		list = configList;
		parent = menu_get_parent_menu(menu);
		if (!parent)
			return;
		list->setRootMenu(parent);
		break;
	case menuMode:
		if (menu->flags & MENU_ROOT) {
			menuList->setRootMenu(menu);
			configList->clearSelection();
			list = configList;
		} else {
			parent = menu_get_parent_menu(menu->parent);
			if (!parent)
				return;

			/* Select the config view */
			item = configList->findConfigItem(parent);
			if (item) {
				configList->setSelected(item, true);
				configList->scrollToItem(item);
			}

			menuList->setRootMenu(parent);
			menuList->clearSelection();
			list = menuList;
		}
		break;
	case fullMode:
		list = configList;
		break;
	default:
		break;
	}

	if (list) {
		item = list->findConfigItem(menu);
		if (item) {
			list->setSelected(item, true);
			list->scrollToItem(item);
			list->setFocus();
			helpText->setInfo(menu);
		}
	}
}

void ConfigMainWindow::listFocusChanged(void)
{
	if (menuList->mode == menuMode)
		configList->clearSelection();
}

void ConfigMainWindow::goBack(void)
{
	if (configList->rootEntry == &rootmenu)
		return;

	configList->setParentMenu();
}

void ConfigMainWindow::showSingleView(void)
{
	singleViewAction->setEnabled(false);
	singleViewAction->setChecked(true);
	splitViewAction->setEnabled(true);
	splitViewAction->setChecked(false);
	fullViewAction->setEnabled(true);
	fullViewAction->setChecked(false);

	backAction->setEnabled(true);

	menuView->hide();
	menuList->setRootMenu(0);
	configList->mode = singleMode;
	if (configList->rootEntry == &rootmenu)
		configList->updateListAll();
	else
		configList->setRootMenu(&rootmenu);
	configList->setFocus();
}

void ConfigMainWindow::showSplitView(void)
{
	singleViewAction->setEnabled(true);
	singleViewAction->setChecked(false);
	splitViewAction->setEnabled(false);
	splitViewAction->setChecked(true);
	fullViewAction->setEnabled(true);
	fullViewAction->setChecked(false);

	backAction->setEnabled(false);

	configList->mode = menuMode;
	if (configList->rootEntry == &rootmenu)
		configList->updateListAll();
	else
		configList->setRootMenu(&rootmenu);
	configList->setAllOpen(true);
	configApp->processEvents();
	menuList->mode = symbolMode;
	menuList->setRootMenu(&rootmenu);
	menuList->setAllOpen(true);
	menuView->show();
	menuList->setFocus();
}

void ConfigMainWindow::conflictSelected(struct menu * men)
{
	configList->clearSelection();
	menuList->clearSelection();
	emit(setMenuLink(men));
}

void ConfigMainWindow::showFullView(void)
{
	singleViewAction->setEnabled(true);
	singleViewAction->setChecked(false);
	splitViewAction->setEnabled(true);
	splitViewAction->setChecked(false);
	fullViewAction->setEnabled(false);
	fullViewAction->setChecked(true);

	backAction->setEnabled(false);

	menuView->hide();
	menuList->setRootMenu(0);
	configList->mode = fullMode;
	if (configList->rootEntry == &rootmenu)
		configList->updateListAll();
	else
		configList->setRootMenu(&rootmenu);
	configList->setFocus();
}

/*
 * ask for saving configuration before quitting
 */
void ConfigMainWindow::closeEvent(QCloseEvent* e)
{
	if (!conf_get_changed()) {
		e->accept();
		return;
	}
	QMessageBox mb("qconf", "Save configuration?", QMessageBox::Warning,
			QMessageBox::Yes | QMessageBox::Default, QMessageBox::No, QMessageBox::Cancel | QMessageBox::Escape);
	mb.setButtonText(QMessageBox::Yes, "&Save Changes");
	mb.setButtonText(QMessageBox::No, "&Discard Changes");
	mb.setButtonText(QMessageBox::Cancel, "Cancel Exit");
	switch (mb.exec()) {
	case QMessageBox::Yes:
		if (saveConfig())
			e->accept();
		else
			e->ignore();
		break;
	case QMessageBox::No:
		e->accept();
		break;
	case QMessageBox::Cancel:
		e->ignore();
		break;
	}
}

void ConfigMainWindow::showIntro(void)
{
	static const QString str = "Welcome to the qconf graphical configuration tool.\n\n"
		"For each option, a blank box indicates the feature is disabled, a check\n"
		"indicates it is enabled, and a dot indicates that it is to be compiled\n"
		"as a module.  Clicking on the box will cycle through the three states.\n\n"
		"If you do not see an option (e.g., a device driver) that you believe\n"
		"should be present, try turning on Show All Options under the Options menu.\n"
		"Although there is no cross reference yet to help you figure out what other\n"
		"options must be enabled to support the option you are interested in, you can\n"
		"still view the help of a grayed-out option.\n\n"
		"Toggling Show Debug Info under the Options menu will show the dependencies,\n"
		"which you can then match by examining other options.\n\n";

	QMessageBox::information(this, "qconf", str);
}

void ConfigMainWindow::showAbout(void)
{
	static const QString str = "qconf is Copyright (C) 2002 Roman Zippel <zippel@linux-m68k.org>.\n"
		"Copyright (C) 2015 Boris Barbulovski <bbarbulovski@gmail.com>.\n\n"
		"Bug reports and feature request can also be entered at http://bugzilla.kernel.org/\n";

	QMessageBox::information(this, "qconf", str);
}

void ConfigMainWindow::saveSettings(void)
{
	configSettings->setValue("/window x", pos().x());
	configSettings->setValue("/window y", pos().y());
	configSettings->setValue("/window width", size().width());
	configSettings->setValue("/window height", size().height());

	QString entry;
	switch(configList->mode) {
	case singleMode :
		entry = "single";
		break;

	case symbolMode :
		entry = "split";
		break;

	case fullMode :
		entry = "full";
		break;

	default:
		break;
	}
	configSettings->setValue("/listMode", entry);

	configSettings->writeSizes("/split1", split1->sizes());
	configSettings->writeSizes("/split2", split2->sizes());
}

void ConfigMainWindow::conf_changed(void)
{
	if (saveAction)
		saveAction->setEnabled(conf_get_changed());
}
void ConfigMainWindow::refreshMenu(void)
{
	configList->updateListAll();
}

void fixup_rootmenu(struct menu *menu)
{
	struct menu *child;
	static int menu_cnt = 0;

	menu->flags |= MENU_ROOT;
	for (child = menu->list; child; child = child->next) {
		if (child->prompt && child->prompt->type == P_MENU) {
			menu_cnt++;
			fixup_rootmenu(child);
			menu_cnt--;
		} else if (!menu_cnt)
			fixup_rootmenu(child);
	}
}

static const char *progname;

static void usage(void)
{
	printf("%s [-s] <config>\n", progname);
	exit(0);
}

int main(int ac, char** av)
{
	ConfigMainWindow* v;
	const char *name;

	progname = av[0];
	configApp = new QApplication(ac, av);
	if (ac > 1 && av[1][0] == '-') {
		switch (av[1][1]) {
		case 's':
			conf_set_message_callback(NULL);
			break;
		case 'h':
		case '?':
			usage();
		}
		name = av[2];
	} else
		name = av[1];
	if (!name)
		usage();

	conf_parse(name);
	fixup_rootmenu(&rootmenu);
#ifdef CONFIGFIX_TEST
	// backup base configuration
	conf_read(BASE_CONFIG);
	base_config = config_backup();
#endif
	conf_read(NULL);
	//zconfdump(stdout);

	configSettings = new ConfigSettings();
	configSettings->beginGroup("/kconfig/qconf");
	v = new ConfigMainWindow();

	//zconfdump(stdout);
	configApp->connect(configApp, SIGNAL(lastWindowClosed()), SLOT(quit()));
	configApp->connect(configApp, SIGNAL(aboutToQuit()), v, SLOT(saveSettings()));
#ifdef CONFIGFIX_TEST
	
	// show detailed information in config view by default
	v->showFullView(); 
	ConfigView *configView = v->getConfigView();
	configView->setShowName(true);
	configView->setShowRange(true);
	configView->setShowData(true);
	// show prompt options
	configView->list->setOptionMode(configView->list->showPromptAction);
	configView->list->showPromptAction->setChecked(true);
	// auto-resize columns in conflicts view 
	ConflictsView *conflictsView = v->getConflictsView();
	conflictsView->conflictsTable->resizeColumnsToContents();

	conflict_dir = get_conflict_dir();
	print_setup(name);
	print_config_stats(configView->list);
	print_sample_stats();

	initial_config = config_backup();
#endif
	v->show();
	configApp->exec();

	configSettings->endGroup();
	delete configSettings;
	delete v;
	delete configApp;

	return 0;
}

void droppableView::dropEvent(QDropEvent *event)
{
	event->acceptProposedAction();
}

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


/* ----- start cf_testing.c ----- */
#ifdef CONFIGFIX_TEST

/*
 * Prints the environment variables and parameters 
 * that affect ConfigFix testing
 */
static void print_setup(const char* name)
{
	printf("\nConfigfix testing enabled:\n");
	printf("---------------------------\n");
	printf("%-27s %s\n", "Working directory:", getenv("PWD"));
	printf("%-27s %s\n", "$CC:", getenv("CC"));
	printf("%-27s %s\n", "$CC_VERSION_TEXT:", getenv("CC_VERSION_TEXT"));
	printf("%-27s %s\n", "$KERNELVERSION:", getenv("KERNELVERSION"));
	printf("%-27s %s\n", "$ARCH:", getenv("ARCH"));
	printf("%-27s %s\n", "$SRCARCH:", getenv("SRCARCH"));
	printf("%-27s %s\n", "$srctree:", getenv("srctree"));
	printf("%-27s %s\n", "Kconfig file:", name);
	if (rootmenu.prompt)
		printf("%-27s %s\n\n", "Root menu prompt:", rootmenu.prompt->text);
	
	printf("%-27s %s\n", 
		"CONFIGFIX_PATH:", getenv("CONFIGFIX_PATH"));	
	printf("%-27s %s\n", 
		"CONFIGFIX_TEST_PATH:", getenv("CONFIGFIX_TEST_PATH"));
	printf("%-27s %s\n", "Results file:", get_results_file());
	printf("%-27s %s\n", 
		"CONFIGFIX_TEST_CONFIG_DIR:", getenv("CONFIGFIX_TEST_CONFIG_DIR"));
	printf("%-27s %s\n", "Base configuration:", BASE_CONFIG);
	printf("%-27s %s\n", "Configuration sample:", conf_get_configname());
	printf("%-27s %s\n", 
		"CONFIGFIX_TEST_PROBABILITY:", getenv("CONFIGFIX_TEST_PROBABILITY"));
		// conflict_dir = get_conflict_dir();
	printf("%-27s %s\n", 
		"Next conflict directory:", conflict_dir);
	printf("%-27s %d\n", "Default conflict size:", conflict_size);
}

/*
 * Prints ConfigItem and symbol statistics for given ConfigList.
 */
static void print_config_stats(ConfigList *list) 
{
	printf("\nConfiguration statistics:\n");
	printf("---------------------------\n");

	// iterate menus
	QTreeWidgetItemIterator it(list);
	ConfigItem* item;
	struct symbol *sym;

	// collect statistics
	int count=0, menuless=0, invisible=0, unknown=0,
		symbolless=0, nonchangeable=0, promptless=0,
		conf_item_candidates=0;

	while (*it) {
		item = (ConfigItem*)(*it);
		count++;
		if (!item->menu) {
			menuless++;
			++it;
			continue;
		}

		if (!menu_has_prompt(item->menu))
			promptless++;

		if (!menu_is_visible(item->menu))
			invisible++;
		
		sym = item->menu->sym;
		if (!sym) {
			symbolless++;
			++it;
			continue;
		}

		if (sym_get_type(sym) == S_UNKNOWN)
			unknown++;

		if (!sym_is_changeable(sym))
			nonchangeable++;
		
		if (sym_has_conflict(sym)) //(sym_has_prompt(sym) && !sym_is_changeable(sym))
			// conflictsView->candidate_symbols++;
			conf_item_candidates++;

		//DEBUG - print choices
		// if (sym_is_choice(sym)) {
		// 	printf("Choice %s = %s", 
		// 		sym_get_name(sym), sym_has_value(sym) ? sym_get_string_value(sym) : "no value");
		// 	if (sym_has_value(sym) && sym->curr.val) {
		// 		printf(" (value is %s)", sym_get_name((struct symbol*)sym->curr.val));
		// 	}
		// 	printf("\n");
		// }
		// if (sym_is_choice_value(sym))
		// 	printf("Choice value %s = %s\n", 
		// 		sym_get_name(sym), sym_get_string_value(sym));
		//DEBUG
		++it;
	}

	printf("%i ConfigItems: %i menu-less, %i prompt-less, %i invisible, %i symbol-less, %i unknown type, %i non-changeable\n", 
		count, menuless, promptless, invisible, symbolless, unknown, nonchangeable);
	

	// alternative counts by iterating symbols
	int i, sym_candidates=0, promptless_unchangeable=0;
	count=0, invisible=0, unknown=0, nonchangeable=0, promptless=0;
	//DEBUG
	int dep_mod=0, blocked_1=0, blocked_2=0, blocked_3=0, blocked_4=0;
	//DEBUG 

	for_all_symbols(i, sym) {
		count++;

		if (!sym_has_prompt(sym))
			promptless++;
		if (sym->visible == no)
			invisible++;
		if (!sym_is_changeable(sym))
			nonchangeable++;
		if (sym_get_type(sym) == S_UNKNOWN)
			unknown++;
		if (sym_has_conflict(sym)) 
			sym_candidates++;
		if (!sym_is_changeable(sym) && !sym_has_prompt(sym))
			promptless_unchangeable++;
		//DEBUG
		if (expr_contains_symbol(sym->dir_dep.expr, &symbol_mod))
			dep_mod++;
		if (sym_has_blocked_values(sym) == 1)
			blocked_1++;
		if (sym_has_blocked_values(sym) == 2)
			blocked_2++;
		if (sym_has_blocked_values(sym) == 3)
			blocked_3++;

		/* // print symbol details
		if (!sym_is_choice(sym) &&
			// sym_is_changeable(sym) &&
			sym_has_conflict(sym) &&
			sym_has_blocked_values(sym)) {
		//DEBUG SYMBOL
			printf("\t%s%s %s (visible: %s, changeable: %s, flags: %#x):\n\tcurr = %s, def[S_DEF_USER] = %s\n", 
				sym_type_name(sym->type), sym_is_choice(sym) ? " choice" : "",
				sym_get_name(sym), 
				sym->visible == no ? "n" : (sym->visible == mod ? "m" : "y"), 
				sym_is_changeable(sym) ? "true" : "false", sym->flags, 
				sym_get_string_value(sym), 
				sym->def[S_DEF_USER].tri == no ? "n" : (sym->def[S_DEF_USER].tri == mod ? "m" : "y")); 
			printf("\tconflict candidate: %s\n", 
				sym_has_conflict(sym) ? "true" : "false");
			printf("\tdepends on 'mod': %s\n",
				expr_depends_symbol(sym->dir_dep.expr, &symbol_mod) ?
					"true" : "false");
			printf("\tdeps contain 'mod': %s\n",
				expr_contains_symbol(sym->dir_dep.expr, &symbol_mod) ? 
					"true" : "false");
			printf("\t%i blocked: no: %s, mod: %s, yes: %s\n",
				sym_has_blocked_values(sym),
				sym_tristate_within_range(sym, no) ? "false" : "true",
				sym_get_type(sym) == S_BOOLEAN ? 
					"-" : sym_tristate_within_range(sym, mod) ? "false" : "true",
				expr_contains_symbol(sym->dir_dep.expr, &symbol_mod) ?
					"-" : (sym_tristate_within_range(sym, yes) ? "false" : "true"));
			// getchar();
			//DEBUG SYMBOL
		}
		if (sym_has_blocked_values(sym) > 3)
			printf("%s: > 3 blocked values!\n", sym_get_name(sym));
		//DEBUG */
	}

	printf("%i symbols: %i prompt-less, %i invisible, %i unknown type, %i non-changeable, %i prompt-less & unchangeable\n", 
		count, promptless, invisible, unknown, nonchangeable, promptless_unchangeable);
	
	printf("Conflict candidates: %i config items (%i symbols)\n", 
		conf_item_candidates, sym_candidates);
	
	// set static variables
	sym_count = count;
	no_conflict_candidates = sym_candidates;

	//DEBUG
	printf("Depend on 'mod': %i\n", dep_mod);
	printf("Blocked values: 1 - %i, 2 - %i, 3 - %i, total - %i\n", 
		blocked_1, blocked_2, blocked_3, 
		(blocked_1 + blocked_2 + blocked_3));
	//DEBUG
}

static void print_sample_stats() {

	int i, count=0, invalid=0, other=0,
	// value counts
	bool_y=0, bool_n=0, tri_y=0, tri_m=0, tri_n=0;

	gstr sample_stats = str_new();
	str_append(&sample_stats, getenv("ARCH"));
	str_append(&sample_stats, ",");
	str_append(&sample_stats, (char*) conf_get_configname());
	str_append(&sample_stats, ",");

	struct symbol *sym;
	for_all_symbols(i, sym) {
		count++;

		const char* val = sym_get_string_value(sym);

		switch (sym_get_type(sym)) {
			case S_BOOLEAN:
				if (strcmp(val, "y") == 0)
					bool_y++;
				else if (strcmp(val, "n") == 0)
					bool_n++;
				else
					invalid++;
				break;
			case S_TRISTATE:
				if (strcmp(val, "y") == 0)
					tri_y++;
				else if (strcmp(val, "m") == 0)
					tri_m++;
				else if (strcmp(val, "n") == 0)
					tri_n++;
				else
					invalid++;
				break;
			default:
				other++;
		}
	}
	printf("\n%9s  %10s  %16s\n", "Sym count",    "Boolean",        "Tristates");
	printf(  "%9s  %13s  %16s\n", "---------", "------ ------", "----- ----- -----");
	
	printf("%9s  %6s %6s  %5s %5s %5s\n", "", "  Y", "  N", "  Y", "  M", "  N");
	
	printf("%9d  %6d %6d  %5d %5d %5d\n", 
		count, bool_y, bool_n, tri_y, tri_m, tri_n);

	// set static variables
	if (tri_y+tri_m+tri_n > 0)
		tristates = true;
	no_enabled_symbols = bool_y + tri_y + tri_m;
}

/*
 * FNV32 string hash, copied from symbol.c
 */
// static unsigned strhash(const char *s)
// {
// 	/* fnv32 hash */
// 	unsigned hash = 2166136261U;
// 	for (; *s; s++)
// 		hash = (hash ^ *s) * 0x01000193;
// 	return hash % HASHSIZE;
// }

/*
 * Save the current configuration (symbol values) into hash table, 
 * where keys are symbol names, and values are symbol values.
 */
static GHashTable* config_backup() 
{	
	GHashTable *backup = g_hash_table_new_full(
			g_str_hash,
			g_str_equal,
			NULL,
			free
  	);

	printf("\nBacking up configuration...\n");

	int i, count = 0, duplicates = 0, unknowns = 0;
	struct symbol *sym;
	char* val;
	for_all_symbols(i, sym) {

		count++;

		if (sym_get_type(sym) == S_UNKNOWN) {
			unknowns++;
			continue;
		}

		// cannot backup NULL-values non-booleans
		if (sym->type != S_BOOLEAN && sym->type != S_TRISTATE && sym->curr.val == NULL) {
			printf("\tWARN: nonboolean symbol %s %s has NULL value\n", sym_get_type_name(sym), sym_get_name(sym));
			continue;
		}

		val = (char*) g_hash_table_lookup(backup, sym_get_name(sym));
		if (val != NULL) {
			printf("\tWARN: duplicate key: %s %s/%s\n", 
				sym_get_type_name(sym), sym_get_name(sym), sym->name);
			//print_symbol(sym);
			if (strcmp(val, sym_get_string_value(sym)))
				printf("\t\tvalue has changed: %s -> %s", 
					val, sym_get_string_value(sym));
		}

		//FIXME free pointer to prevent memleak
		g_hash_table_insert(backup, strdup(sym_get_name(sym)), strdup(sym_get_string_value(sym)));
	}

	printf("Done: iterated %i symbols, %i symbols in backup table, %i UNKNOWNs ignored \n\n", 
		count, g_hash_table_size(backup), unknowns);
	return backup;
}

/*
 * Compare the current configuration with given backup.
 * Return 0 if the configuration and the backup match,
 * otherwise return number of mismatching symbols.
 */
static int config_compare(GHashTable *backup) 
{
	// printf("Comparing current configuration with backup...\n");

	// GHashTableIter iter;
	// gpointer key, val;

	// g_hash_table_iter_init(&iter, backup);
	// while (g_hash_table_iter_next(&iter, &key, &val))
	// 	{
	// 		if (key != nullptr)
	// 		printf("%s\n", (char*) key);
	// 	}

	struct symbol *sym;
	int i, count = 0, match = 0, mismatch = 0, unknowns = 0;
	char *backup_val, *current_val;
	for_all_symbols(i, sym) {

		count++;

		if (sym_get_type(sym) == S_UNKNOWN) {
			unknowns++;
			continue;
		}

		backup_val  = (char*) g_hash_table_lookup(backup, sym_get_name(sym));
		if (backup_val == NULL)
			backup_val = "symbol backup missing";
		
		current_val = strdup(sym_get_string_value(sym));
		if (strcmp(backup_val, current_val) != 0) {
			//DEBUG
			// printf("%s %s %s/%s has changed: %s -> %s\n", 
			// 	sym_is_choice(sym) ? "choice" : "", 
			// 	sym_type_name(sym_get_type(sym)),
			// 	sym_get_name(sym), sym->name, backup_val, current_val);
			//DEBUG
			mismatch++;
		} else
			match++;

		free(current_val);
	}
	//DEBUG
	// printf("Done: %i symbols compared (%i match, %i mismatch), %i UNKNOWNs ignored\n", 
	// 	count, match, mismatch, unknowns);
	
	// printf("Current configuration and backup %s\n", mismatch ? "MISMATCH" : "MATCH");		
	//DEBUG
	return mismatch;
}

/*
 * Reset configuration to the initial one, which was read upon program start.
 * Initial configuration filename is specified by the KCONFIG_CONFIG variable,
 * and defaults to '.config' if the variable is not set.
 */
static void config_reset(void)
{
	conf_read(conf_get_configname());
}

/*
 * Returns a path for saving a next conflict 
 * for the current configuration sample.
 * The result is dynamically allocated and must be freed.
 */
static char* get_conflict_dir()
{
  
    // open the configuration sample directory  
	struct dirent *de;
    DIR *dr = opendir(get_config_dir()); 
  
    if (dr == NULL) { 
        printf("Could not open directory\n"); 
        return NULL; 
    } 
  
	int next_conflict_num = 1;

    // iterate it
    while ((de = readdir(dr)) != NULL) 
		// look for subdirectories 
		if (de->d_type == DT_DIR
			// whose name start with 'conflict.'
			&& strncmp("conflict.", de->d_name, strlen("conflict.")) == 0) 
			{
				char *num_string = strtok(de->d_name, "conflict.");
				if (num_string) {
					int current_conflict_num = atoi(num_string);
					if (current_conflict_num >= next_conflict_num)
						next_conflict_num = current_conflict_num + 1;
				}
			}
  
    closedir(dr);
	
	// construct path e.g. /path/to/config/sample/conflict.05/
	// gstr pathname = str_new();
	// str_append(&pathname, get_config_dir());
	// str_printf(&pathname, "/conflict.%.2d/", next_conflict_num);

	char *conflict_dir = (char*) malloc(
		sizeof(char) * (strlen(get_config_dir()) + strlen("/conflict.XXX/") + 1));
	sprintf(conflict_dir, "%s/conflict.%.3d/", get_config_dir(), next_conflict_num);
	return conflict_dir; 
}

/*
 * Returns the path to the used configuration sample
 * stored in CONFIGFIX_TEST_CONFIG_DIR env. variable;
 * returns "." if this variable is not defined.
 */
static char* get_config_dir(void)
{
	// char *config_dir = (char*) conf_get_configname();
	// // if config filename is a path
	// if (strrchr(config_dir, '/')) {
	// 	// drop its contents past the last slash
	// 	strrchr(config_dir, '/')[1] = 0;
	// 	return config_dir;
	if (getenv("CONFIGFIX_TEST_CONFIG_DIR"))
		return getenv("CONFIGFIX_TEST_CONFIG_DIR");
	else
		return ".";
}

/*
 * Constructs the path to the RESULTS_FILE
 */
static char* get_results_file(void)
{
	gstr filename = str_new();

	if (getenv("CONFIGFIX_TEST_PATH")) {
		str_append(&filename, getenv("CONFIGFIX_TEST_PATH"));
		str_append(&filename, "/");
	}

	str_append(&filename, RESULTS_FILE);

	return (char*) str_get(&filename);
}

/*
 * Append given string followed by a comma to result_string.
 */
static void append_result(char *s) 
{
	if (str_get(&result_string)) {
		str_append(&result_string, s);
		str_append(&result_string, ",");	
	} else
		printf("Warning: result string is empty");
}

/*
 * In RANDOM_TESTING mode, writes contents of result string 
 * to RESULTS_FILE, otherwise prints it to stdout.
 * Frees the result string after that.
 */
static void output_result() 
{
	if (testing_mode == RANDOM_TESTING) {
		FILE* f = fopen(get_results_file(), "a");
    	if(!f) {
        	printf("Error: could not write to %s\n", get_results_file());
			return;
		}
		fprintf(f, "%s\n", str_get(&result_string));
		fclose(f);
	} else
		;// printf("\nResult string:\n----------------\n%s\n", str_get(&result_string));

	str_free(&result_string);

}

/**
 * Selects a tristate value currently blocked for given symbol.
 * For a tristate symbol, if two values are blocked, makes a random selection.
 */
static tristate random_blocked_value(struct symbol *sym)
{
	tristate *values = NULL;
	int no_values=0;

	// dynamically allocate at most 2 values (excluding current)
	if (sym_get_tristate_value(sym) != no &&
		!sym_tristate_within_range(sym, no)) 
		{
			no_values++;
			values = (tristate*) realloc(values, 
				no_values * sizeof(tristate));
			values[no_values-1] = no;
		}

	if (sym_get_type(sym) == S_TRISTATE &&
		sym_get_tristate_value(sym) != mod &&
		!sym_tristate_within_range(sym, mod)) 
		{
			no_values++;
			values = (tristate*) realloc(values, 
				no_values * sizeof(tristate));
			values[no_values-1] = mod;
		}

	if (sym_get_tristate_value(sym) != yes &&
		!expr_contains_symbol(sym->dir_dep.expr, &symbol_mod) &&
		!sym_tristate_within_range(sym, yes)) 
		{
			no_values++;
			values = (tristate*) realloc(values, 
				no_values * sizeof(tristate));
			values[no_values-1] = yes;
		}

	switch(no_values) {
	case 1:
		return values[0];
	case 2:
		return values[rand() % no_values];
	default:
		printf("Error: too many random values for %s\n",
			sym_get_name(sym));
	}
	
	return no;
}

/**
 * Returns the conflicting ConfigItem with the given 1-based index
 * (which can be randomly generated) from the given ConfigList,
 * assuming its consistent iteration order.
 * 
 * This function iterates the ConfigList, and increases a counter 
 * for every ConfigItem that holds a conflicting symbol (see
 * sym_has_conflict()). 
 * When counter equals index, that ConfigItem is returned.
 */
static ConfigItem* get_conflict_item(int index, ConfigList* configList)
{
	//DEBUG
	// printf("Index parameter = %i\n",index);
	//DEBUG
	ConfigItem* item;
	struct symbol *sym;
	int cnt = 0;

	// iterate ConfigItems
	QTreeWidgetItemIterator it(configList);
	while (*it) 
	{
		item = (ConfigItem*)(*it);
		// skip items without menus or symbols
		if (!item->menu) {
			++it;
			continue;
		}
		sym = item->menu->sym;
		if (!sym) {
			++it;
			continue;
		}

		// consider only conflicting items
		if (sym_has_conflict(sym)) {
			cnt++;
			//DEBUG
			// printf("\tcounter = %i\t%s : %s\n", 
			// 	cnt, sym_get_name(sym), sym_has_conflict(sym) ? "conflict" : "NO conflict");
			// printf("Symbol with index %i %s %s conflict\n",
			// index, sym_get_name(sym), 
			// sym_has_conflict(sym) ? "HAS" : "DOES NOT HAVE");
			//DEBUG
		}

		if (cnt == index)
			return item;
		else
			++it;
	}

	return NULL;
}

/*
 * Returns 'true' if the symbol conflicts with the current
 * configuration, 'false' otherwise.
 */
static bool sym_has_conflict(struct symbol *sym)
{
	// symbol is conflicting if it
	return (
		// has prompt (visible to user)
		sym_has_prompt(sym) && 
		// is bool or tristate
		sym_is_boolean(sym) &&
		// is not 'choice' (choice values should be used instead)
		!sym_is_choice(sym)) &&
		// cannot be changed
		//!sym_is_changeable(sym)
		// has at least 1 blocked value 
		sym_has_blocked_values(sym);
}

/**
 * For a visible boolean or tristate symbol, returns the number of 
 * its possible values that cannot be set (not within range).
 * Otherwise, returns 0 (including other symbol types).
 */
static int sym_has_blocked_values(struct symbol *sym)
{
	if (!sym_is_boolean(sym))
		return 0;

	// ignore symbols disabled in the base config
	if (!sym_enabled_in_base_config(sym))
		return 0;

	int result = 0;

	if (!sym_tristate_within_range(sym, no))
		result++;
	
	if (sym_get_type(sym) == S_TRISTATE && 
		!sym_tristate_within_range(sym, mod))
		result++;

	/* some tristates depend on 'mod', can never be set to 'yes */
	if (!expr_contains_symbol(sym->dir_dep.expr, &symbol_mod) &&
		!sym_tristate_within_range(sym, yes))
		result++;

	return result;
}

/**
 * Returns 'true' if a bool/tristate symbol has a value 'yes' or 'mod'
 * in the base configuration, 'false' otherwise.
 */
static bool sym_enabled_in_base_config(struct symbol *sym)
{
	// FIXME replace GLib
	char *base_val  = (char*) g_hash_table_lookup(base_config, sym_get_name(sym));
	if (base_val == NULL) {
		printf("ERROR: symbol %s missing in base config\n", sym_get_name(sym));
		return false;
	}

	if (strcmp(base_val, "y") == 0 || strcmp(base_val, "m") == 0)
		return true;
		
	return false;
}

/*
 * Return the string representation of the given symbol's type
 */
static const char* sym_get_type_name(struct symbol *sym) 
{ 
	/*
	 * This is different from sym_type_name(sym->type),
	 * because sym_get_type() covers some special cases
	 * related to choice values and MODULES.
	 */
	return sym_type_name(sym_get_type(sym));
}

/*
 * Return value of the given symbol fix as string.
 */ 
static const char* sym_fix_get_string_value(struct symbol_fix *fix)
{
	if (fix->type == SF_BOOLEAN)
		switch (fix->tri) {
			case no:
				return "n";
			case mod:
				return "m";
			case yes:
				return "y";
		}
	else if (fix->type == SF_NONBOOLEAN)
		return str_get(&fix->nb_val);
	
	printf("Cannot get value: disallowed symbol fix\n");
	return NULL;
}

/*
 * Add element to the head of a sfix_list
 */
void sfix_list_prepend(struct sfix_list *list, struct symbol_fix *fix)
{
	struct sfix_node *node = (sfix_node*) xcalloc(1, sizeof(*node));
	node->elem = fix;

	if (list->size == 0) {
		list->head = node;
		list->tail = node;
	} else {
		node->next = list->head;
		list->head = node;
		node->next->prev = node;
	}

	list->size++;
}

/**
 * Returns an as-is or a reversed shallow copy of the argument solution, 
 * depending on the 'reverse_diag_order' flag.
 * The flag value is flipped before the function returns.
 */
// WAS static GArray* rearrange_solution(GArray *diag)
static sfix_list* rearrange_solution(sfix_list *solution)
{
	// WAS GArray* result = g_array_new(false,false,sizeof(struct symbol_fix*));
	struct sfix_list *result = sfix_list_init();
	struct sfix_node *node;

	sfix_list_for_each(node, solution) {
		// reverse order
		if (reverse_diag_order)
			sfix_list_prepend(result, node->elem);
		// exact copy
		else 
			sfix_list_add(result, node->elem);
	}

	reverse_diag_order = !reverse_diag_order;

	return result;
}

/*
 *
 */
static bool free_permutation(sfix_list *permutation)
{
	if (permutation == NULL) {
		//DEBUG
		printf("permutation is NULL\n");
		//DEBUG
		return true;
	}

	struct sfix_node *node = permutation->head, *tmp;

	while (node != NULL) {
		tmp = node->next;
		node = tmp;
	}
	
	free(permutation);
	//DEBUG
	printf("permutation freed\n");
	permutation = NULL;
	printf("permutation set to NULL\n");
	//DEBUG
	return true;
}

/*
 * Check that the conflict in the given table is resolved,
 * i.e. all its symbols have their target values
 * in the current configuration.
 */
static bool verify_resolution(QTableWidget* conflictsTable)
{
	for (int i = 0; i < conflictsTable->rowCount(); i++) {
		auto _symbol = conflictsTable->item(i,0)->text().toUtf8().data();
		struct symbol *sym = sym_find(_symbol);
		tristate value  = string_value_to_tristate(conflictsTable->item(i,1)->text());
			
		// consider only booleans as conflict symbols
		if (value != sym_get_tristate_value(sym)) {
			printf("Conflict symbol %s: target %s != actual %s\n", 
				sym_get_name(sym),
				conflictsTable->item(i,1)->text().toUtf8().data(),
				sym_get_string_value(sym));
			return false;
		}
	}

	return true;
}

/* 
 * Check if all symbols in the solution have their target values.
 */
static bool verify_fix_target_values(sfix_list *solution)
{
	struct sfix_node *node;
	struct symbol_fix *fix;
	struct symbol *sym;

	sfix_list_for_each(node, solution) {
		fix = node->elem;
		sym = fix->sym;
		switch (sym_get_type(sym)) {
		case S_BOOLEAN:
		case S_TRISTATE:
			if (fix->tri != sym_get_tristate_value(fix->sym)) {
				printf("Fix symbol %s: target %s != actual %s\n", 
					sym_get_name(sym),
					sym_fix_get_string_value(fix),
					sym_get_string_value(sym));
				return false;
			}
			break;
		default:
			if (strcmp(str_get(&fix->nb_val), sym_get_string_value(fix->sym)) != 0)
			{
				printf("\t%s: target %s != actual %s\n", 
					sym_get_name(sym),
					sym_fix_get_string_value(fix),
					sym_get_string_value(sym));
				return false;
			}
		}
	}

	return true;
}

/*
 * Save diagnosis using filename that combines given prefix and 
 * diagnosis status e.g. diag02.VALID.txt or diag08.INVALID.txt.
 * The file is saved into the 'conflict_dir'.
 */
static void save_diagnosis(struct sfix_list *diag, char* file_prefix, bool valid_diag)
{
	// only save diagnosis in the RANDOM_TESTING mode
	if (testing_mode != RANDOM_TESTING)
		return;
	
	// construct name of a file
	char filename[
		strlen(conflict_dir) // to be saved into 'conflict_dir'
		+ strlen(file_prefix)
		+ strlen(valid_diag ? ".VALID" : ".INVALID")
		+ strlen(".txt") + 1];

	sprintf(filename, "%s%s%s.txt", 
		conflict_dir, file_prefix, 
		valid_diag ? ".VALID" : ".INVALID");
	
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        printf("I/O ERROR: could not save diagnosis\n");
		return;
	}

    QTextStream out(&file);

	struct sfix_node *node;
	struct symbol_fix *fix;
	
	sfix_list_for_each(node, diag) {
		fix = node->elem;
		if (fix->type == SF_BOOLEAN)
			out << fix->sym->name << " => " << tristate_get_char(fix->tri) << "\n";
		else if (fix->type == SF_NONBOOLEAN)
			out << fix->sym->name << " => " << str_get(&fix->nb_val) << "\n"; 
		else
			perror("NB not yet implemented.");
	}

	file.close();
	printf("\n#\n# diagnosis saved to %s\n#\n", filename);
}


static bool verify_solution(int i, const char *result_prefix, 
	struct sfix_list *solution, QTableWidget* conflictsTable)
{
	int size = solution->size;

	/* 
	 * Initialise result string with:
	 * - columns 1-12 - common prefix passed as argument
	 * - column 13  - Diag. index
	 * - column 14  - Diag. size
	 */
	result_string = str_new();
	str_append(&result_string, result_prefix);
	str_printf(&result_string, "%i,%i,", i, size);

	// print solution info
	printf("\n-------------------------------\nSolution %i:\n", i);
	print_diagnosis_symbol(solution);

	/*
	 - save & reload config - should match
	 - applied (all symbols have target values)
	 - compare changed symbols and dependencies
	 - restoring initial configuration
	 */
	// status flags
	bool 
		// check 1 - conflict resolved
		RESOLVED = false,  
		// check 2 - fix fully applied
		APPLIED = false,
		// config reset error
		ERR_RESET = false,    
		// save & reload config - should match
		CONFIGS_MATCH = false,
		// dependencies met
		DEPS_MET = false,
		// result
		VALID = false; 


	/* Apply the symbol fixes */

	int permutation_count = 0;

	/* 
	 * Collect indices of the symbol fixes in the diagnosis.
	 * std::next_permutation() generates lexicographically larger permutations,
	 * hence its argument array should be initially sorted in ascending order.
	 */
	// int fix_idxs[size];
	// for (int k=0; k<size; k++)
	// 	fix_idxs[k] = k;

	// WAS GArray *permutation;
	struct sfix_list *permutation;
	// do {
	while ( permutation_count < 2 ) {
		permutation = rearrange_solution(solution); //rearrange_diagnosis(diag, fix_idxs);
		permutation_count++;
		//DEBUG
		printf("----------\nPermutation %i:\n",permutation_count);
		print_diagnosis_symbol(permutation);
		printf("----------\n");
		//DEBUG

		/* Verifying target values directly after apply_fix() may fail.
		   Therefore config must be saved before verifying target values. */ 
		if (apply_fix(permutation)) { // at least 1 symbol must be applied => returns > 0
			conf_write(".config.applied");

			// check 1 - conflict resolved
			if (verify_resolution(conflictsTable)) {
				RESOLVED = true;
			}

			// check 2 - fix applied
			if (verify_fix_target_values(permutation)) { 
				APPLIED = true;
			}

			// exit loop if the conflict is resolved
			if (RESOLVED) {
				break;
			}

		} else {
			config_reset();
			if (config_compare(initial_config) != 0) {
				printf("\nERROR: could not reset configuration after testing permutation:\n");
				print_diagnosis_symbol(permutation);
				ERR_RESET = true;
				break;
			}

			// DEBUG
			printf("TEST FAILED\n\n");
			// DEBUG

		} 
	} //while ( std::next_permutation(fix_idxs, fix_idxs+size) );
	
	printf("Conflict resolution status: %s (%d permutations tested)\n", 
		RESOLVED ? "SUCCESS" : "FAILURE", permutation_count);
				
	// filename prefix e.g. diag09
	char diag_prefix[strlen("diagXX") + 1]; 
	sprintf(diag_prefix, "diag%.2d", i);		

	save_diagnosis(solution, diag_prefix, RESOLVED);

    /* Save & reload config - should match */

	// RANDOM_TESTING only
	if (testing_mode == RANDOM_TESTING) {
		gstr config_filename = str_new();

		//e.g. /path/to/config/sample/conflict/.config.diag09
		// char config_filename[
		// 	strlen(conflict_dir)
		// 	+ strlen(".config.") 
		// 	+ strlen(diag_prefix) + 1];

		// sprintf(config_filename, "%s.config.%s", conflict_dir, diag_prefix); 
		str_append(&config_filename, conflict_dir);
		str_append(&config_filename, ".config.");
		str_append(&config_filename, diag_prefix);
		
		// save configuration, make backup		
		conf_write(str_get(&config_filename));
		GHashTable *after_write = config_backup();

		// reload, compare
		conf_read(str_get(&config_filename));
		if (config_compare(after_write) == 0)
			CONFIGS_MATCH =  true;
		else
			printf("WARNING: reloaded configuration and backup mismatch\n");

		// free memory
		g_hash_table_destroy(after_write);
		str_free(&config_filename);
	}
	// skip the rest if failed
	// if (!CONFIGS_MATCH) {
	// 	// column 12 - Resolved
	// 	append_result("NO");
	// 	// column 13 - Applied
	// 	append_result((char*) (APPLIED ? "YES" : "NO"));
	// 	// column 14 - Reset errors
	// 	append_result((char*) (ERR_RESET ? "YES" : "NO"));
	// 	// column 15 - Configs match
	// 	append_result((char*) (CONFIGS_MATCH ? "YES" : "NO"));
	// 	output_result();

	// 	return false;
	// }


	/* Check 3 - check for unmet dependencies */
	// DEPS_MET = diag_dependencies_met(diag);


	/* Output result */
	// VALID = APPLIED && !ERR_RESET && CONFIGS_MATCH && DEPS_MET;
	// column 15 - Resolved
	append_result((char*) (RESOLVED ? "YES" : "NO"));
	// column 16 - Applied
	append_result((char*) (APPLIED ? "YES" : "NO"));
	// // column 14 - Reset errors
	// append_result((char*) (ERR_RESET ? "YES" : "NO"));
	// // column 15 - Configs match
	// append_result((char*) (CONFIGS_MATCH ? "YES" : "NO"));
	// // column 16 - Deps. met
	// append_result((char*) (DEPS_MET ? "YES" : "NO"));

	output_result();
	str_free(&result_string);

	return RESOLVED;



	// // reload saved configuration

	// conf_read(config_filename);
	// // make sure it hasn't changed
	// printf("Reloaded configuration and backup %s\n", 
	// config_compare(after_fix) ? "MISMATCH" : "MATCH");
	// // return;
			
	/* FIXME Check 4 - verify changed symbols */
	
			
	// check that only symbols in the fix were changed 
	// printf("Will restore initial configuration\n");getchar();
	// config_reset();
	// config_compare(initial_config);
}


/* ------ copied from cf_rangefix.c ------ */

// /* 
//  * print a single diagnosis of type symbol_fix
//  */
// void print_diagnosis_symbol(struct sfix_list *diag_sym)
// {
// 	struct symbol_fix *fix;
// 	struct sfix_node *node;

// 	printd("[");

// 	sfix_list_for_each(node, diag_sym) {
// 		fix = node->elem;

// 		if (fix->type == SF_BOOLEAN) {
// 			printd("%s => %s", fix->sym->name, tristate_get_char(fix->tri));
// 		} else if (fix->type == SF_NONBOOLEAN) {
// 			printd("%s => %s", fix->sym->name, str_get(&fix->nb_val));
// 		} else {
// 			perror("NB not yet implemented.");
// 		}

// 		if (node->next != NULL)
// 			printd(", ");
// 	}
// 	printd("]\n");
// }
// 
// /*
//  * print the diagnoses of type symbol_fix
//  */
// static void print_diagnoses_symbol(struct sfl_list *diag_sym)
// {
// 	struct sfl_node *arr;
// 	unsigned int i = 1;

// 	sfl_list_for_each(arr, diag_sym) {
// 		printd("%d: ", i++);
// 		print_diagnosis_symbol(arr->elem);
// 	}
// }


#endif
/* ----- end cf_testing.c ----- */

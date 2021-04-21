#ifndef MUDLET_DLGVARSMAINAREA_H
#define MUDLET_DLGVARSMAINAREA_H

/***************************************************************************
 *   Copyright (C) 2013 by Chris Mitchell                                  *
 *   Copyright (C) 2014 by Ahmed Charles - acharles@outlook.com            *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#include "pre_guard.h"
#include "ui_vars_main_area.h"
#include "post_guard.h"

#include "dlgTriggerEditor.h"
#include "LuaInterface.h"
#include "TTreeWidget.h"
#include "VarUnit.h"


class dlgVarsMainArea : public QWidget, public Ui::vars_main_area
{
    Q_OBJECT
    friend class dlgTriggerEditor;

public:
    Q_DISABLE_COPY(dlgVarsMainArea)
    dlgVarsMainArea(QWidget*);

    void repopulateVars();
    void addVar(bool);
    void saveVar();
    void setInterface(LuaInterface*);
    int canRecast(QTreeWidgetItem* pItem, int newNameType, int newValueType);

    void recurseVariablesUp(QTreeWidgetItem* const pItem, QList<QTreeWidgetItem*>& list);
    void recurseVariablesDown(QTreeWidgetItem* const pItem, QList<QTreeWidgetItem*>& list);

    void show_vars();
    void search(QString &varShort);
    void searchVariables(const QString& s);
    void recursiveSearchVariables(TVar* var, QList<TVar*>& list, bool isSorted);


public slots:
    void slot_item_selected_save(QTreeWidgetItem*);
    void slot_var_selected(QTreeWidgetItem*);
    void slot_var_changed(QTreeWidgetItem*);
    void slot_tree_selection_changed();
    void slot_toggleHiddenVar(bool);
    void slot_toggleHiddenVariables(bool);
    void slot_show_vars();

private:
    QTreeWidgetItem* mpCurrentVarItem = nullptr;
    QTreeWidgetItem* mpVarBaseItem = nullptr;
    TTreeWidget* treeWidget_variables = nullptr;

    LuaInterface* luaInterface;

    bool showHiddenVars;
    bool mChangingVar;

    edbee::TextEditorWidget *   mpSourceEditorEdbee;
    edbee::TextDocument *       mpSourceEditorEdbeeDocument;

};

#endif // MUDLET_DLGVARSMAINAREA_H

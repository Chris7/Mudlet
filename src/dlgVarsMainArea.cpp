/***************************************************************************
 *   Copyright (C) 2013 by Chris Mitchell                                  *
 *   Copyright (C) 2014 by Ahmed Charles - acharles@outlook.com            *
 *   Copyright (C) 2017 by Stephen Lyons - slysven@virginmedia.com         *
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


#include "dlgVarsMainArea.h"

#include "pre_guard.h"
#include <QListWidgetItem>
#include <QDebug>
#include <QTreeWidget>
#include "post_guard.h"

extern "C" {
#include <lua.h>
}

dlgVarsMainArea::dlgVarsMainArea(QWidget* pF) : QWidget(pF)
{
    // init generated dialog
    setupUi(this);

    treeWidget_variables = new TTreeWidget();
    treeWidget_variables->hide();
    treeWidget_variables->setIsVarTree();
    treeWidget_variables->setColumnCount(2);
    treeWidget_variables->hideColumn(1);
    treeWidget_variables->header()->hide();
    treeWidget_variables->setRootIsDecorated(false);
    treeWidget_variables->setContextMenuPolicy(Qt::ActionsContextMenu);
    connect(treeWidget_variables, &QTreeWidget::itemClicked, this, &dlgVarsMainArea::slot_item_selected_save);

    connect(treeWidget_variables, &QTreeWidget::itemClicked, this, &dlgVarsMainArea::slot_var_selected);
    connect(treeWidget_variables, &QTreeWidget::itemChanged, this, &dlgVarsMainArea::slot_var_changed);
    connect(treeWidget_variables, &QTreeWidget::itemSelectionChanged, this, &dlgVarsMainArea::slot_tree_selection_changed);


    // Modify the normal QComboBoxes with customised data models that impliment
    // https://stackoverflow.com/a/21376774/4805858 so that individual entries
    // can be "disabled":
    // Key type widget:

    // Magic - part 1 set up a replacement data model:
    auto *contents = new QListWidget(comboBox_variable_key_type);
    contents->hide();
    comboBox_variable_key_type->setModel(contents->model());

    // Now populate the widget - throw away stuff entered from form design
    // before substitute model was attached:
    comboBox_variable_key_type->clear();
    comboBox_variable_key_type->insertItem(0, tr("Auto-Type"), -1); // LUA_TNONE
    comboBox_variable_key_type->insertItem(1, tr("key (string)"), 4);  // LUA_TSTRING
    comboBox_variable_key_type->insertItem(2, tr("index (integer number)"), 3);  // LUA_TNUMBER
    comboBox_variable_key_type->insertItem(3, tr("table (use \"Add Group\" to create)"), 5);  // LUA_TTABLE
    comboBox_variable_key_type->insertItem(4, tr("function (cannot create from GUI)"), 6);  // LUA_TFUNCTION

    // Magic - part 2 use the features of the substitute data model to disable
    // the required entries - they can still be set programmatically for display
    // purposes:
    // Disable table type:
    QListWidgetItem *item = contents->item(3);
    item->setFlags(item->flags() & ~Qt::ItemIsEnabled);

    // Disable function type:
    item = contents->item(4);
    item->setFlags(item->flags() & ~Qt::ItemIsEnabled);


    // Value type widget:
    // Magic - part 1 set up a replacement data model:
    contents = new QListWidget(comboBox_variable_value_type);
    contents->hide();
    comboBox_variable_value_type->setModel(contents->model());

    // Now populate the widget - throw away stuff entered from form design
    // before substitute model was attached:
    comboBox_variable_value_type->clear();
    comboBox_variable_value_type->insertItem(0, tr("Auto-Type"), LUA_TNONE);
    comboBox_variable_value_type->insertItem(1, tr("string"), LUA_TSTRING);
    comboBox_variable_value_type->insertItem(2, tr("number"), LUA_TNUMBER);
    comboBox_variable_value_type->insertItem(3, tr("boolean"), LUA_TBOOLEAN);
    comboBox_variable_value_type->insertItem(4, tr("table"), LUA_TTABLE);
    comboBox_variable_value_type->insertItem(5, tr("function"), LUA_TFUNCTION);

    // Magic - part 2 use the features of the substitute data model:
    // Disable function type:
    item = contents->item(5);
    item->setFlags(item->flags() & ~Qt::ItemIsEnabled);

    connect(checkBox_variable_hidden, &QAbstractButton::clicked, this, &dlgVarsMainArea::slot_toggleHiddenVar);
}

void dlgVarsMainArea::slot_item_selected_save(QTreeWidgetItem* pItem)
{
    if (!pItem) {
        return;
    }

    saveVar();
}

void dlgVarsMainArea::slot_tree_selection_changed()
{
    auto * sender = qobject_cast<TTreeWidget*>(QObject::sender());
    if (sender) {
        QList<QTreeWidgetItem*> items = sender->selectedItems();
        if (items.length()) {
            QTreeWidgetItem* item = items.first();
                slot_var_selected(item);
        }
    }
}

void dlgVarsMainArea::setInterface(LuaInterface* interface) {
    luaInterface = interface;
}

void dlgVarsMainArea::addVar(bool isFolder)
{
    saveVar();
    comboBox_variable_key_type->setCurrentIndex(0);
    if (isFolder) {
        // in lieu of readonly
        mpSourceEditorEdbee->setEnabled(false);
        comboBox_variable_value_type->setDisabled(true);
        comboBox_variable_value_type->setCurrentIndex(4);
        lineEdit_var_name->setText(QString());
        lineEdit_var_name->setPlaceholderText(tr("Table name..."));

//        clearDocument(mpSourceEditorEdbee, QLatin1String("NewTable"));
    } else {
        // in lieu of readonly
        mpSourceEditorEdbee->setEnabled(true);
        lineEdit_var_name->setText(QString());
        lineEdit_var_name->setPlaceholderText(tr("Variable name..."));
        comboBox_variable_value_type->setDisabled(false);
        comboBox_variable_value_type->setCurrentIndex(0);
    }

    VarUnit* vu = luaInterface->getVarUnit();

    QStringList nameL;
    nameL << QString(isFolder ? "New table name" : "New variable name");

    QTreeWidgetItem* pParent;
    QTreeWidgetItem* pNewItem;
    QTreeWidgetItem* cItem = treeWidget_variables->currentItem();
    TVar* cVar = vu->getWVar(cItem);
    if (cItem) {
        if (cVar && cVar->getValueType() == LUA_TTABLE) {
            pParent = cItem;
        } else {
            pParent = cItem->parent();
        }
    }

    auto newVar = new TVar();
    if (pParent) {
        //we're nested under something, or going to be.  This HAS to be a table
        TVar* parent = vu->getWVar(pParent);
        if (parent && parent->getValueType() == LUA_TTABLE) {
            //create it under the parent
            pNewItem = new QTreeWidgetItem(pParent, nameL);
            newVar->setParent(parent);
        } else {
            pNewItem = new QTreeWidgetItem(mpVarBaseItem, nameL);
            newVar->setParent(vu->getBase());
        }
    } else {
        pNewItem = new QTreeWidgetItem(mpVarBaseItem, nameL);
        newVar->setParent(vu->getBase());
    }

    if (isFolder) {
        newVar->setValue(QString(), LUA_TTABLE);
    } else {
        newVar->setValueType(LUA_TNONE);
    }
    vu->addTempVar(pNewItem, newVar);
    pNewItem->setFlags(pNewItem->flags() & ~(Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled));

    mpCurrentVarItem = pNewItem;
    treeWidget_variables->setCurrentItem(pNewItem);
    slot_var_selected(treeWidget_variables->currentItem());
}

int dlgVarsMainArea::canRecast(QTreeWidgetItem* pItem, int newNameType, int newValueType)
{
    //basic checks, return 1 if we can recast, 2 if no need to recast, 0 if we can't recast
    VarUnit* vu = luaInterface->getVarUnit();
    TVar* var = vu->getWVar(pItem);
    if (!var) {
        return 2;
    }
    int currentNameType = var->getKeyType();
    int currentValueType = var->getValueType();
    //most anything is ok to do.  We just want to enforce these rules:
    //you cannot change the type of a table that has children
    //rule removed to see if anything bad happens:
    //you cannot change anything to a table that isn't a table already
    if (currentValueType == LUA_TFUNCTION || currentNameType == LUA_TTABLE) {
        return 0; //no recasting functions or table keys
    }

    if (newValueType == LUA_TTABLE && currentValueType != LUA_TTABLE) {
        //trying to change a table to something else
        if (!var->getChildren(false).empty()) {
            return 0;
        }
        //no children, we can do this without bad things happening
        return 1;
    }

    if (currentNameType == newNameType && currentValueType == newValueType) {
        return 2;
    }
    return 1;
}

void dlgVarsMainArea::saveVar()
{
    // We can enter this function if:
    // we click on a variable without having one selected ( no parent )
    // we click on a variable from another variable
    // we click on a variable from having the top-most element selected ( parent but parent is not a variable/table )
    // we click on a variable from the same variable (such as a double click)
    // we add a new variable
    // we switch away from a variable (so we are saving the old variable)

    if (!mpCurrentVarItem) {
        return;
    }
    QTreeWidgetItem* pItem = mpCurrentVarItem;
    if (!pItem->parent()) {
        return;
    }

    auto* varUnit = luaInterface->getVarUnit();
    TVar* variable = varUnit->getWVar(pItem);
    bool newVar = false;
    if (!variable) {
        newVar = true;
        variable = varUnit->getTVar(pItem);
    }
    if (!variable) {
        return;
    }
    QString newName = lineEdit_var_name->text();
    QString newValue = mpSourceEditorEdbeeDocument->text();
    if (newName.isEmpty()) {
        slot_var_selected(pItem);
        return;
    }
    mChangingVar = true;
    int uiNameType = comboBox_variable_key_type->itemData(comboBox_variable_key_type->currentIndex(), Qt::UserRole).toInt();
    int uiValueType = comboBox_variable_value_type->itemData(comboBox_variable_value_type->currentIndex(), Qt::UserRole).toInt();
    if ((uiNameType == LUA_TNUMBER || uiNameType == LUA_TSTRING) && newVar) {
        uiNameType = LUA_TNONE;
    }
    //check variable recasting
    int varRecast = canRecast(pItem, uiNameType, uiValueType);
    if ((uiNameType == -1) || (variable && uiNameType != variable->getKeyType())) {
        if (QString(newName).toInt()) {
            uiNameType = LUA_TNUMBER;
        } else {
            uiNameType = LUA_TSTRING;
        }
    }
    if ((uiValueType != LUA_TTABLE) && (uiValueType == -1)) {
        if (newValue.toInt()) {
            uiValueType = LUA_TNUMBER;
        } else if (newValue.toLower() == "true" || newValue.toLower() == "false") {
            uiValueType = LUA_TBOOLEAN;
        } else {
            uiValueType = LUA_TSTRING;
        }
    }
    if (varRecast == 2) {
        //we sometimes get in here from new variables
        if (newVar) {
            //we're making this var
            variable = varUnit->getTVar(pItem);
            if (!variable) {
                variable = new TVar();
            }
            variable->setName(newName, uiNameType);
            variable->setValue(newValue, uiValueType);
            luaInterface->createVar(variable);
            varUnit->addVariable(variable);
            varUnit->addTreeItem(pItem, variable);
            varUnit->removeTempVar(pItem);
            varUnit->getBase()->addChild(variable);
            pItem->setText(0, newName);
            mpCurrentVarItem = nullptr;
        } else if (variable) {
            if (newName == variable->getName() && (variable->getValueType() == LUA_TTABLE && newValue == variable->getValue())) {
                //no change made
            } else {
                //we're trying to rename it/recast it
                int change = 0;
                if (newName != variable->getName() || uiNameType != variable->getKeyType()) {
                    //let's make sure the nametype works
                    if (variable->getKeyType() == LUA_TNUMBER && newName.toInt()) {
                        uiNameType = LUA_TNUMBER;
                    } else {
                        uiNameType = LUA_TSTRING;
                    }
                    change = change | 0x1;
                }
                variable->setNewName(newName, uiNameType);
                if (variable->getValueType() != LUA_TTABLE && (newValue != variable->getValue() || uiValueType != variable->getValueType())) {
                    //let's check again
                    if (variable->getValueType() == LUA_TTABLE) {
                        //HEIKO: obvious logic error used to be valueType == LUA_TABLE
                        uiValueType = LUA_TTABLE;
                    } else if (uiValueType == LUA_TNUMBER && newValue.toInt()) {
                        uiValueType = LUA_TNUMBER;
                    } else if (uiValueType == LUA_TBOOLEAN && (newValue.toLower() == "true" || newValue.toLower() == "false")) {
                        uiValueType = LUA_TBOOLEAN;
                    } else {
                        uiValueType = LUA_TSTRING; //nope, you don't agree, you lose your value
                    }
                    variable->setValue(newValue, uiValueType);
                    change = change | 0x2;
                }
                if (change) {
                    if (change & 0x1 || newVar) {
                        luaInterface->renameVar(variable);
                    }
                    if ((variable->getValueType() != LUA_TTABLE && change & 0x2) || newVar) {
                        luaInterface->setValue(variable);
                    }
                    pItem->setText(0, newName);
                    mpCurrentVarItem = nullptr;
                } else {
                    variable->clearNewName();
                }
            }
        }
    } else if (varRecast == 1) { //recast it
        TVar* var = varUnit->getWVar(pItem);
        if (newVar) {
            //we're making this var
            var = varUnit->getTVar(pItem);
            var->setName(newName, uiNameType);
            var->setValue(newValue, uiValueType);
            luaInterface->createVar(var);
            varUnit->addVariable(var);
            varUnit->addTreeItem(pItem, var);
            pItem->setText(0, newName);
            mpCurrentVarItem = nullptr;
        } else if (var) {
            //we're trying to rename it/recast it
            int change = 0;
            if (newName != var->getName() || uiNameType != var->getKeyType()) {
                //let's make sure the nametype works
                if (uiNameType == LUA_TSTRING) {
                    //do nothing, we can always make key to string
                } else if (var->getKeyType() == LUA_TNUMBER && newName.toInt()) {
                    uiNameType = LUA_TNUMBER;
                } else {
                    uiNameType = LUA_TSTRING;
                }
                var->setNewName(newName, uiNameType);
                change = change | 0x1;
            }
            if (newValue != var->getValue() || uiValueType != var->getValueType()) {
                //let's check again
                if (uiValueType == LUA_TTABLE) {
                    newValue = "{}";
                } else if (uiValueType == LUA_TNUMBER && newValue.toInt()) {
                    uiValueType = LUA_TNUMBER;
                } else if (uiValueType == LUA_TBOOLEAN && (newValue.toLower() == QLatin1String("true") || newValue.toLower() == QLatin1String("false"))) {
                    uiValueType = LUA_TBOOLEAN;
                } else {
                    uiValueType = LUA_TSTRING; //nope, you don't agree, you lose your value
                }
                var->setValue(newValue, uiValueType);
                change = change | 0x2;
            }
            if (change) {
                if (change & 0x1 || newVar) {
                    luaInterface->renameVar(var);
                }
                if (change & 0x2 || newVar) {
                    luaInterface->setValue(var);
                }
                pItem->setText(0, newName);
                mpCurrentVarItem = nullptr;
            }
        }
    }
    //redo this here in case we changed type
    pItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsAutoTristate | Qt::ItemIsUserCheckable);
    pItem->setToolTip(0, tr("Checked variables will be saved and loaded with your profile."));
    if (!varUnit->shouldSave(variable)) {
        pItem->setFlags(pItem->flags() & ~(Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsUserCheckable));
        pItem->setForeground(0, QBrush(QColor("grey")));
        pItem->setToolTip(0, "");
        pItem->setCheckState(0, Qt::Unchecked);
    } else if (varUnit->isSaved(variable)) {
        pItem->setCheckState(0, Qt::Checked);
    }
    pItem->setData(0, Qt::UserRole, variable->getValueType());
    QIcon icon;
    switch (variable->getValueType()) {
    case 5:
        icon.addPixmap(QPixmap(QStringLiteral(":/icons/table.png")), QIcon::Normal, QIcon::Off);
        break;
    case 6:
        icon.addPixmap(QPixmap(QStringLiteral(":/icons/function.png")), QIcon::Normal, QIcon::Off);
        break;
    default:
        icon.addPixmap(QPixmap(QStringLiteral(":/icons/variable.png")), QIcon::Normal, QIcon::Off);
        break;
    }
    pItem->setIcon(0, icon);
    mChangingVar = false;
    slot_var_selected(pItem);
}

void dlgVarsMainArea::slot_var_changed(QTreeWidgetItem* pItem)
{
    // This handles a small case where the radio buttom is clicked while the item is currently selected
    // which causes the variable to not save. In places where we populate the TreeWidgetItem, we have
    // to guard it with mChangingVar or else this will be called with every change such as the variable
    // name, etc.
    if (!pItem || mChangingVar) {
        return;
    }
    int column = 0;
    int state = pItem->checkState(column);
    VarUnit* vu = luaInterface->getVarUnit();
    TVar* var = vu->getWVar(pItem);
    if (!var) {
        return;
    }
    if (state == Qt::Checked || state == Qt::PartiallyChecked) {
        if (vu->isSaved(var)) {
            return;
        }
        vu->addSavedVar(var);
        QList<QTreeWidgetItem*> list;
        recurseVariablesUp(pItem, list);
        for (auto& treeWidgetItem : list) {
            TVar* v = vu->getWVar(treeWidgetItem);
            if (v && (treeWidgetItem->checkState(column) == Qt::Checked || treeWidgetItem->checkState(column) == Qt::PartiallyChecked)) {
                vu->addSavedVar(v);
            }
        }
        list.clear();
        recurseVariablesDown(pItem, list);
        for (auto& treeWidgetItem : list) {
            TVar* v = vu->getWVar(treeWidgetItem);
            if (v && (treeWidgetItem->checkState(column) == Qt::Checked || treeWidgetItem->checkState(column) == Qt::PartiallyChecked)) {
                vu->addSavedVar(v);
            }
        }
    } else {
        // we're not checked, dont save us
        if (!vu->isSaved(var)) {
            return;
        }
        vu->removeSavedVar(var);
        QList<QTreeWidgetItem*> list;
        recurseVariablesUp(pItem, list);
        for (auto& treeWidgetItem : list) {
            TVar* v = vu->getWVar(treeWidgetItem);
            if (v && (treeWidgetItem->checkState(column) == Qt::Checked || treeWidgetItem->checkState(column) == Qt::PartiallyChecked)) {
                vu->removeSavedVar(v);
            }
        }
        list.clear();
        recurseVariablesDown(pItem, list);
        for (auto& treeWidgetItem : list) {
            TVar* v = vu->getWVar(treeWidgetItem);
            if (v && (treeWidgetItem->checkState(column) == Qt::Checked || treeWidgetItem->checkState(column) == Qt::PartiallyChecked)) {
                vu->removeSavedVar(v);
            }
        }
    }
}

// This should not modify the contents of what pItem points at:
void dlgVarsMainArea::recurseVariablesUp(QTreeWidgetItem* const pItem, QList<QTreeWidgetItem*>& list)
{
    QTreeWidgetItem* pParent = pItem->parent();
    if (pParent && pParent != mpVarBaseItem) {
        list.append(pParent);
        recurseVariablesUp(pParent, list);
    }
}

// This should not modify the contents of what pItem points at:
void dlgVarsMainArea::recurseVariablesDown(QTreeWidgetItem* const pItem, QList<QTreeWidgetItem*>& list)
{
    list.append(pItem);
    for (int i = 0; i < pItem->childCount(); ++i) {
        recurseVariablesDown(pItem->child(i), list);
    }
}

void dlgVarsMainArea::slot_var_selected(QTreeWidgetItem* pItem)
{
    if (!pItem ||treeWidget_variables->indexOfTopLevelItem(pItem) == 0) {
        // Null item or it is for the first row of the tree
        hide();
//        // mpSourceEditorArea->hide();
//        showInfo(msgInfoAddVar);
        return;
    }

//    clearEditorNotification();

    // save the current variable before switching to the new one
    if (pItem != mpCurrentVarItem) {
        saveVar();
    }

    mChangingVar = true;
    int column = treeWidget_variables->currentColumn();
    int state = pItem->checkState(column);

    VarUnit* vu = luaInterface->getVarUnit();
    TVar* var = vu->getWVar(pItem); // This does NOT modify pItem or what it points at
    QList<QTreeWidgetItem*> list;
    if (state == Qt::Checked || state == Qt::PartiallyChecked) {
        if (var) {
            vu->addSavedVar(var);
        }
        recurseVariablesUp(pItem, list); // This does NOT modify pItem or what it points at
        for (auto& treeWidgetItem : list) {
            TVar* v = vu->getWVar(treeWidgetItem);
            if (v && (treeWidgetItem->checkState(column) == Qt::Checked || treeWidgetItem->checkState(column) == Qt::PartiallyChecked)) {
                vu->addSavedVar(v);
            }
        }
        list.clear();
        recurseVariablesDown(pItem, list); // This does NOT modify pItem or what it points at
        for (auto& treeWidgetItem : list) {
            TVar* v = vu->getWVar(treeWidgetItem);
            if (v && (treeWidgetItem->checkState(column) == Qt::Checked || treeWidgetItem->checkState(column) == Qt::PartiallyChecked)) {
                vu->addSavedVar(v);
            }
        }
    } else {
        if (var) {
            vu->removeSavedVar(var);
        }
        recurseVariablesUp(pItem, list); // This does NOT modify pItem or what it points at
        for (auto& treeWidgetItem : list) {
            TVar* v = vu->getWVar(treeWidgetItem);
            if (v && (treeWidgetItem->checkState(column) == Qt::Unchecked)) {
                vu->removeSavedVar(v);
            }
        }
        list.clear();
        recurseVariablesDown(pItem, list); // This does NOT modify pItem or what it points at
        for (auto& treeWidgetItem : list) {
            TVar* v = vu->getWVar(treeWidgetItem);
            if (v && (treeWidgetItem->checkState(column) == Qt::Unchecked)) {
                vu->removeSavedVar(v);
            }
        }
    }
    show();
//    // mpSourceEditorArea->show();

    mpCurrentVarItem = pItem; //remember what has been clicked to save it
    // There was repeated test for pItem being null here but we have NOT altered
    // it since the start of the function where it was already tested for not
    // being zero so we don't need to retest it! - Slysven
    if (column) {
        mChangingVar = false;
        return;
    }

    if (!var) {
        checkBox_variable_hidden->setChecked(false);
        lineEdit_var_name->clear();
//        clearDocument(mpSourceEditorEdbee); // Var Select
        //check for temp item
        var = vu->getTVar(pItem);
        if (var && var->getValueType() == LUA_TTABLE) {
            comboBox_variable_value_type->setDisabled(true);
            // index 4 = "table"
            comboBox_variable_value_type->setCurrentIndex(4);
        } else {
            comboBox_variable_value_type->setDisabled(false);
            // index 0 = "Auto-type"
            comboBox_variable_value_type->setCurrentIndex(0);
        }
        comboBox_variable_key_type->setCurrentIndex(0);
        mChangingVar = false;
        return;
    }

    int varType = var->getValueType();
    int keyType = var->getKeyType();
    QIcon icon;

    switch (keyType) {
//    case LUA_TNONE: // -1
//    case LUA_TNIL: // 0
//    case LUA_TBOOLEAN: // 1
//    case LUA_TLIGHTUSERDATA: // 2
    case LUA_TNUMBER: // 3
        // index 2 = "index (integer number)"
        comboBox_variable_key_type->setCurrentIndex(2);
        comboBox_variable_key_type->setEnabled(true);
        break;
    case LUA_TSTRING: // 4
        // index 1 = "key (string)"
        comboBox_variable_key_type->setCurrentIndex(1);
        comboBox_variable_key_type->setEnabled(true);
        break;
    case LUA_TTABLE: // 5
        // index 3 = "table (use \"Add Group\" to create"
        comboBox_variable_key_type->setCurrentIndex(3);
        comboBox_variable_key_type->setEnabled(false);
        break;
    case LUA_TFUNCTION: // 6
        // index 4 = "function (cannot create from GUI)"
        comboBox_variable_key_type->setCurrentIndex(4);
        comboBox_variable_key_type->setEnabled(false);
        break;
//    case LUA_TUSERDATA: // 7
//    case LUA_TTHREAD: // 8
    }

    switch (varType) {
    case LUA_TNONE:
        [[fallthrough]];
    case LUA_TNIL:
        // mpSourceEditorArea->hide();
        break;
    case LUA_TBOOLEAN:
        // mpSourceEditorArea->show();
        mpSourceEditorEdbee->setEnabled(true);
        icon.addPixmap(QPixmap(QStringLiteral(":/icons/variable.png")), QIcon::Normal, QIcon::Off);
        // index 3 = "boolean"
        comboBox_variable_value_type->setCurrentIndex(3);
        comboBox_variable_value_type->setEnabled(true);
        break;
    case LUA_TNUMBER:
        // mpSourceEditorArea->show();
        mpSourceEditorEdbee->setEnabled(true);
        icon.addPixmap(QPixmap(QStringLiteral(":/icons/variable.png")), QIcon::Normal, QIcon::Off);
        // index 2 = "number"
        comboBox_variable_value_type->setCurrentIndex(2);
        comboBox_variable_value_type->setEnabled(true);
        break;
    case LUA_TSTRING:
        // mpSourceEditorArea->show();
        mpSourceEditorEdbee->setEnabled(true);
        icon.addPixmap(QPixmap(QStringLiteral(":/icons/variable.png")), QIcon::Normal, QIcon::Off);
        // index 1 = "string"
        comboBox_variable_value_type->setCurrentIndex(1);
        comboBox_variable_value_type->setEnabled(true);
        break;
    case LUA_TTABLE:
        // mpSourceEditorArea->hide();
        mpSourceEditorEdbee->setEnabled(false);
        // Only allow the type to be changed away from a table if it is empty:
        comboBox_variable_value_type->setEnabled(!(pItem->childCount() > 0));
        // index 4 = "table"
        comboBox_variable_value_type->setCurrentIndex(4);
        icon.addPixmap(QPixmap(QStringLiteral(":/icons/table.png")), QIcon::Normal, QIcon::Off);
        break;
    case LUA_TFUNCTION:
        // mpSourceEditorArea->hide();
        mpSourceEditorEdbee->setEnabled(false);
        comboBox_variable_value_type->setCurrentIndex(5);
        comboBox_variable_value_type->setEnabled(false);
        icon.addPixmap(QPixmap(QStringLiteral(":/icons/function.png")), QIcon::Normal, QIcon::Off);
        break;
    case LUA_TLIGHTUSERDATA:
        [[fallthrough]];
    case LUA_TUSERDATA:
        [[fallthrough]];
    case LUA_TTHREAD:
        ; // No-op
    }

    checkBox_variable_hidden->setChecked(vu->isHidden(var));
    lineEdit_var_name->setText(var->getName());
//    clearDocument(mpSourceEditorEdbee, luaInterface->getValue(var));
    pItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsAutoTristate | Qt::ItemIsUserCheckable);
    pItem->setToolTip(0, "Checked variables will be saved and loaded with your profile.");
    pItem->setCheckState(0, Qt::Unchecked);
    if (!vu->shouldSave(var)) {
        pItem->setFlags(pItem->flags() & ~(Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsUserCheckable));
        pItem->setForeground(0, QBrush(QColor("grey")));
        pItem->setToolTip(0, "");
    } else if (vu->isSaved(var)) {
        pItem->setCheckState(0, Qt::Checked);
    }
    pItem->setData(0, Qt::UserRole, var->getValueType());
    pItem->setIcon(0, icon);
    mChangingVar = false;
}

void dlgVarsMainArea::show_vars()
{
    //no repopulation of variables
//    changeView(EditorViewType::cmVarsView);
    mpCurrentVarItem = nullptr;
//    // mpSourceEditorArea->show();
//    checkBox_displayAllVariables->show();
//    checkBox_displayAllVariables->setChecked(showHiddenVars);
    QTreeWidgetItem* pI = treeWidget_variables->topLevelItem(0);
//    if (pI) {
//        if (pI->childCount() > 0) {
//            show();
//            slot_var_selected(treeWidget_variables->currentItem());
//        } else {
//            hide();
//            showInfo(msgInfoAddVar);
//        }
//    }
    treeWidget_variables->show();
}

void dlgVarsMainArea::search(QString &varShort) {
    VarUnit* vu = luaInterface->getVarUnit();

    QList<QTreeWidgetItem*> list;
    recurseVariablesDown(mpVarBaseItem, list);
    QListIterator<QTreeWidgetItem*> it(list);
    while (it.hasNext()) {
        QTreeWidgetItem* treeWidgetItem = it.next();
        TVar* var = vu->getWVar(treeWidgetItem);
//        if (vu->shortVarName(var) == varShort) {
//            show_vars();
//            treeWidget_variables->setCurrentItem(treeWidgetItem, 0);
//            treeWidget_variables->scrollToItem(treeWidgetItem);

//            // highlight all instances of the item that we're searching for.
//            // edbee already remembers this from a setSearchTerm() call elsewhere
//            auto controller = mpSourceEditorEdbee->controller();
//            auto searcher = controller->textSearcher();
//            searcher->markAll(controller->borderedTextRanges());
//            controller->update();

//            switch (pItem->data(0, TypeRole).toInt()) {
//            case SearchResultIsName:
//                lineEdit_var_name->setFocus(Qt::OtherFocusReason);
//                lineEdit_var_name->setCursorPosition(pItem->data(0, PositionRole).toInt());
//                break;
//            case SearchResultIsValue:
//                mpSourceEditorEdbee->setFocus();
//                controller->moveCaretTo(pItem->data(0, PatternOrLineRole).toInt(), pItem->data(0, PositionRole).toInt(), false);
//                break;
//            default:
//                qDebug() << "dlgTriggerEditor::slot_item_selected_list(...) Called for a VAR type item but handler for element of type:"
//                         << treeWidgetItem->data(0, TypeRole).toInt() << "not yet done/applicable...!";
//            }
//            return;
//        }
    }
}

// This WAS called recurseVariablesDown(TVar*, QList<TVar*>&, bool) but it is
// used for searching like the other resursiveSearchXxxxx(...) are
void dlgVarsMainArea::recursiveSearchVariables(TVar* var, QList<TVar*>& list, bool isSorted)
{
    list.append(var);
    QListIterator<TVar*> it(var->getChildren(isSorted));
    while (it.hasNext()) {
        recursiveSearchVariables(it.next(), list, isSorted);
    }
}

void dlgVarsMainArea::searchVariables(const QString& s)
{
//    VarUnit* vu = luaInterface->getVarUnit();
//    TVar* base = vu->getBase();
//    QListIterator<TVar*> itBaseVarChildren(base->getChildren(false));
//    while (itBaseVarChildren.hasNext()) {
//        TVar* var = itBaseVarChildren.next();
//        // We do not search for hidden variables - probably because we would
//        // have to unhide all of them to show the hidden ones found by
//        // searching
//        if (!showHiddenVars && vu->isHidden(var)) {
//            continue;
//        }

//        //recurse down this variable
//        QList<TVar*> list;
//        recursiveSearchVariables(var, list, false);
//        QListIterator<TVar*> itVarDecendent(list);
//        while (itVarDecendent.hasNext()) {
//            TVar* varDecendent = itVarDecendent.next();
//            if (!showHiddenVars && vu->isHidden(varDecendent)) {
//                continue;
//            }

//            QTreeWidgetItem* pItem;
//            QTreeWidgetItem* parent = nullptr;
//            QString name = varDecendent->getName();
//            QString value = varDecendent->getValue();
//            QStringList idStringList = vu->shortVarName(varDecendent);
//            QString idString;
//            // Take the first element - to comply with lua requirement it
//            // must begin with not a digit and not contain any spaces so is
//            // a string - and it is used "unquoted" as is to be the base
//            // of a lua table
//            if (idStringList.size() > 1) {
//                QStringList midStrings = idStringList;
//                idString = midStrings.takeFirst();
//                QStringListIterator itSubString(midStrings);
//                while (itSubString.hasNext()) {
//                    QString intermediate = itSubString.next();
//                    bool isOk = false;
//                    int numberValue = intermediate.toInt(&isOk);
//                    if (isOk && QString::number(numberValue) == intermediate) {
//                        // This seems to be an integer
//                        idString.append(QStringLiteral("[%1]").arg(intermediate));
//                    } else {
//                        idString.append(QStringLiteral("[\"%1\"]").arg(intermediate));
//                    }
//                }
//            } else if (!idStringList.empty()) {
//                idString = idStringList.at(0);
//            }

//            int startPos = 0;
//            if ((startPos = name.indexOf(s, 0, ((mSearchOptions & SearchOptionCaseSensitive) ? Qt::CaseSensitive : Qt::CaseInsensitive))) != -1) {
//                QStringList sl;
//                sl << tr("Variable") << idString << tr("Name");
//                parent = new QTreeWidgetItem(sl);
//                // We do not (yet) worry about multiple search results in the "name"
//                setAllSearchData(parent, name, vu->shortVarName(varDecendent), SearchResultIsName, startPos);
//                treeWidget_searchResults->addTopLevelItem(parent);
//            }

//            // The additional first test is needed to exclude the case when
//            // the search term matches on the word "function" which will
//            // appear in EVERY "value" for a lua function in the variable
//            // tree widget...
//            if (value != QLatin1String("function") && (startPos = value.indexOf(s, 0, ((mSearchOptions & SearchOptionCaseSensitive) ? Qt::CaseSensitive : Qt::CaseInsensitive))) != -1) {
//                QStringList sl;
//                if (!parent) {
//                    sl << tr("Variable") << idString << tr("Value") << value;
//                    parent = new QTreeWidgetItem(sl);
//                    // We do not (yet) worry about multiple search results in the "value"
//                    setAllSearchData(parent, name, vu->shortVarName(varDecendent), SearchResultIsValue, startPos);
//                    treeWidget_searchResults->addTopLevelItem(parent);
//                } else {
//                    sl << QString() << QString() << tr("Value") << value;
//                    pItem = new QTreeWidgetItem(sl);
//                    // We do not (yet) worry about multiple search results in the "value"
//                    setAllSearchData(pItem, name, vu->shortVarName(varDecendent), SearchResultIsValue, startPos);
//                    parent->addChild(pItem);
//                    parent->setExpanded(true);
//                }
//            }
//        }
//    }
}

void dlgVarsMainArea::slot_show_vars()
{
//    changeView(EditorViewType::cmVarsView);
    repopulateVars();
    mpCurrentVarItem = nullptr;
//    checkBox_displayAllVariables->show();
//    checkBox_displayAllVariables->setChecked(showHiddenVars);
    QTreeWidgetItem* pI = treeWidget_variables->topLevelItem(0);
    if (!pI || pI == treeWidget_variables->currentItem() || !pI->childCount()) {
        // There is no root item, we are on the root item or there are no other
        // items - so show the help message:
        hide();
        // mpSourceEditorArea->hide();
//        showInfo(msgInfoAddVar);
    } else {
        show();
        // mpSourceEditorArea->show();
        slot_var_selected(treeWidget_variables->currentItem());
    }
//    if (!mVarEditorSplitterState.isEmpty()) {
//        splitter_right->restoreState(mVarEditorSplitterState);
//    } else {
//        const QList<int> sizes = {30, 900, 30};
//        splitter_right->setSizes(sizes);
//        mVarEditorSplitterState = splitter_right->saveState();
//    }
}

void dlgVarsMainArea::repopulateVars()
{
    treeWidget_variables->setUpdatesEnabled(false);
    mpVarBaseItem = new QTreeWidgetItem(QStringList(tr("Variables")));
    mpVarBaseItem->setTextAlignment(0, Qt::AlignLeft | Qt::AlignVCenter);
    mpVarBaseItem->setIcon(0, QPixmap(QStringLiteral(":/icons/variables.png")));
    treeWidget_variables->clear();
    mpCurrentVarItem = nullptr;
    treeWidget_variables->insertTopLevelItem(0, mpVarBaseItem);
    mpVarBaseItem->setExpanded(true);

    luaInterface->getVars(false);
    VarUnit* vu = luaInterface->getVarUnit();
    vu->buildVarTree(mpVarBaseItem, vu->getBase(), showHiddenVars);
    mpVarBaseItem->setExpanded(true);
    treeWidget_variables->setUpdatesEnabled(true);
}

void dlgVarsMainArea::slot_toggleHiddenVar(bool status)
{

    VarUnit* vu = luaInterface->getVarUnit();
    TVar* var = vu->getWVar(mpCurrentVarItem);
    if (var) {
        if (status) {
            vu->addHidden(var, 1);
        } else {
            vu->removeHidden(var);
        }
    }
}

void dlgVarsMainArea::slot_toggleHiddenVariables(bool state)
{
    if (showHiddenVars != state) {
        showHiddenVars = state;
        repopulateVars();
    }
}

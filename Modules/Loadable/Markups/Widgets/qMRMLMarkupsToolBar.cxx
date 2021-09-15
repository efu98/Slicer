/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Julien Finet, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

// Qt includes
#include <QDebug>
#include <QToolButton>
#include <QMenu>
#include <QCheckBox>
#include <QSignalMapper>
#include <QSplitter>
#include <QShortcut>
#include <QKeySequence>
#include <QTest>

// Slicer includes
#include "qSlicerCoreApplication.h"
#include "qSlicerApplication.h"
#include "qSlicerLayoutManager.h"

// MRML includes
#include "qMRMLMarkupsToolBar_p.h"
#include "qMRMLNodeComboBox.h"
#include "qMRMLThreeDView.h"
#include "qMRMLThreeDWidget.h"
#include "qMRMLSliceView.h"
#include "qMRMLSliceWidget.h"
#include "qSlicerAbstractCoreModule.h"
#include "qSlicerModuleManager.h"

#include <vtkMRMLScene.h>
#include <vtkMRMLMarkupsNode.h>
#include <vtkMRMLInteractionNode.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLLayoutLogic.h>
#include <vtkMRMLSelectionNode.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLViewNode.h>
#include <vtkMRMLWindowLevelWidget.h>
#include <qSlicerMarkupsPlaceWidget.h>
#include <vtkMRMLDisplayNode.h>
#include <vtkMRMLAnnotationNode.h>

// SlicerLogic includes
#include <vtkSlicerApplicationLogic.h>
// MRMLLogic includes
#include <vtkMRMLApplicationLogic.h>
#include <vtkSlicerMarkupsLogic.h>

// VTK includes
#include <vtkWeakPointer.h>
#include <vtkSmartPointer.h>

//---------------------------------------------------------------------------
qMRMLMarkupsToolBarPrivate::qMRMLMarkupsToolBarPrivate(qMRMLMarkupsToolBar& object)
  : q_ptr(&object)
{
  this->MarkupsNodeSelector = 0;
  this->MarkupsPlaceWidget = 0;
  this->DefaultPlaceClassName = "vtkMRMLMarkupsFiducialNode";

}

//---------------------------------------------------------------------------
void qMRMLMarkupsToolBarPrivate::init()
{
  Q_Q(qMRMLMarkupsToolBar);

  // Markups node selector
  this->MarkupsNodeSelector = new qMRMLNodeComboBox();
  this->MarkupsNodeSelector->setNodeTypes(QStringList(QString("vtkMRMLMarkupsNode")));
  this->MarkupsNodeSelector->setNoneEnabled(false);
  this->MarkupsNodeSelector->setAddEnabled(false);
  this->MarkupsNodeSelector->setRenameEnabled(true);
  this->MarkupsNodeSelector->setEditEnabled(true);
  this->MarkupsNodeSelector->setMaximumWidth(150);
  this->MarkupsNodeSelector->setEnabled(true);

  connect(this->MarkupsNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)),
    q, SIGNAL(activeMarkupsNodeChanged(vtkMRMLNode*)) );
  connect(this->MarkupsNodeSelector, SIGNAL(currentNodeChanged(vtkMRMLNode*)), q, SLOT(onMarkupsNodeChanged()));

  // set up keyboard shortcuts
  q->addCreateNodeShortcut(tr("Ctrl+Shift+A"));
  q->addTogglePersistenceShortcut(tr("Ctrl+Shift+T"));
  q->addPlacePointShortcut(tr("Ctrl+Shift+Space"));
  // Get scene and application logic
  q->setApplicationLogic(qSlicerApplication::application()->applicationLogic());
  q->setMRMLScene(qSlicerApplication::application()->mrmlScene());
}

// --------------------------------------------------------------------------
void qMRMLMarkupsToolBarPrivate::addPlaceWidget()
{
  Q_Q(qMRMLMarkupsToolBar);
  this->MarkupsPlaceWidget = new qSlicerMarkupsPlaceWidget;
  this->MarkupsPlaceWidget->setDeleteAllMarkupsOptionVisible(true);
  this->MarkupsPlaceWidget->setPlaceMultipleMarkups(qSlicerMarkupsPlaceWidget::ShowPlaceMultipleMarkupsOption);
  q->addWidget(this->MarkupsPlaceWidget);
  connect(this->MarkupsPlaceWidget, SIGNAL(activeMarkupsPlaceModeChanged(bool)), q, SIGNAL(activeMarkupsPlaceModeChanged(bool)));
  this->MarkupsPlaceWidget->setMRMLScene(this->MRMLScene);
}

// --------------------------------------------------------------------------
void qMRMLMarkupsToolBarPrivate::addCreateNodeActions(vtkSlicerMarkupsLogic* markupsLogic)
{
  Q_Q(qMRMLMarkupsToolBar);
  // Add node creation buttons
  q->addSeparator();

  for(const auto markupName: markupsLogic->GetRegisteredMarkupsTypes())
    {
    vtkMRMLMarkupsNode* markupsNode =
      markupsLogic->GetNodeByMarkupsType(markupName.c_str());
    if (markupsNode && markupsLogic->GetCreateMarkupsPushButton(markupName.c_str()))
      {
      QSignalMapper* mapper = new QSignalMapper(q);
      QPushButton* markupCreateButton = new QPushButton();
      markupCreateButton->setObjectName(QString("Create") + QString(markupsNode->GetMarkupType()) + QString("PushButton"));
      markupCreateButton->setToolTip("Create new " + QString(markupsNode->GetMarkupType()) + " node");
      markupCreateButton->setIcon(QIcon(markupsNode->GetPlaceAddIcon()));
      q->addWidget(markupCreateButton);
      QObject::connect(markupCreateButton, SIGNAL(clicked()), mapper, SLOT(map()));
      mapper->setMapping(markupCreateButton, markupsNode->GetClassName());
      QObject::connect(mapper, SIGNAL(mapped(const QString&)),q, SLOT(onCreateMarkupByClass(const QString&)));
      }
    }
}

// --------------------------------------------------------------------------
void qMRMLMarkupsToolBarPrivate::addSetModuleButton(vtkSlicerMarkupsLogic* markupsLogic, const QString& moduleName)
{
  Q_Q(qMRMLMarkupsToolBar);

  QPushButton* moduleButton = new QPushButton();
  moduleButton->setObjectName(QString(moduleName + " module shortcut"));
  moduleButton->setToolTip("Open the " + moduleName + " module");
  QString iconName = ":/Icons/" + moduleName + ".png";
  moduleButton->setIcon(QIcon(iconName));
  QSignalMapper* mapper = new QSignalMapper(q);
  QObject::connect(moduleButton, SIGNAL(clicked()), mapper, SLOT(map()));
  mapper->setMapping(moduleButton, moduleName);
  QObject::connect(mapper, SIGNAL(mapped(const QString&)),
    this, SLOT(onSetModule(const QString&)));
  q->addWidget(moduleButton);
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsToolBarPrivate::onSetModule(const QString& moduleName)
{
  qSlicerModuleManager* moduleManager = qSlicerCoreApplication::application()->moduleManager();
  if (!moduleManager)
    {
    return;
    }
  qSlicerAbstractCoreModule* module = moduleManager->module(moduleName);
  if (!module)
    {
    return;
    }
  qSlicerLayoutManager* layoutManager = qSlicerApplication::application()->layoutManager();
  if (!layoutManager)
    {
    return;
    }
  layoutManager->setCurrentModule(moduleName);
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsToolBarPrivate::onCreateMarkupByClass(const QString& className)
{
  if (this->MRMLScene)
    {
    vtkMRMLNode* node = this->MRMLScene->AddNewNodeByClass(className.toStdString().c_str());
    vtkMRMLMarkupsNode* markupsNode = vtkMRMLMarkupsNode::SafeDownCast(node);

    if (!markupsNode)
      {
      qCritical() << Q_FUNC_INFO << ": node added is not a vtkMRMLMarkupsNode.";
      return;
      }

    std::string nodeName =
      this->MRMLScene->GenerateUniqueName(markupsNode->GetDefaultNodeNamePrefix());
    markupsNode->SetName(nodeName.c_str());
    }
}

// --------------------------------------------------------------------------
void qMRMLMarkupsToolBarPrivate::setMRMLScene(vtkMRMLScene* newScene)
{
  Q_Q(qMRMLMarkupsToolBar);

  if (newScene == this->MRMLScene)
    {
    return;
    }

  this->qvtkReconnect(this->MRMLScene, newScene, vtkMRMLScene::StartBatchProcessEvent,
    this, SLOT(onMRMLSceneStartBatchProcess()));

  this->qvtkReconnect(this->MRMLScene, newScene, vtkMRMLScene::EndBatchProcessEvent,
    this, SLOT(onMRMLSceneEndBatchProcess()));

  this->MRMLScene = newScene;
  this->MarkupsNodeSelector->setMRMLScene(newScene);
  if (this->MarkupsPlaceWidget)
    {
    this->MarkupsPlaceWidget->setMRMLScene(newScene);
    }

  // watch for changes to the interaction, selection nodes so can update the widget
  q->setInteractionNode((this->MRMLAppLogic && this->MRMLScene) ? this->MRMLAppLogic->GetInteractionNode() : nullptr);
  q->setSelectionNode((this->MRMLAppLogic && this->MRMLScene) ? this->MRMLAppLogic->GetSelectionNode() : nullptr);

  vtkMRMLSelectionNode* selectionNode =
    (this->MRMLAppLogic && this->MRMLScene) ?
    this->MRMLAppLogic->GetSelectionNode() : nullptr;
  this->qvtkReconnect(selectionNode, vtkMRMLSelectionNode::ActivePlaceNodeClassNameChangedEvent,
    this, SLOT(updateWidgetFromMRML()));
  this->qvtkReconnect(selectionNode, vtkMRMLSelectionNode::PlaceNodeClassNameListModifiedEvent,
    this, SLOT(updateWidgetFromMRML()));

  // Update UI
  q->setEnabled(this->MRMLScene != nullptr);
  this->updateWidgetFromMRML();

}

//---------------------------------------------------------------------------
void qMRMLMarkupsToolBarPrivate::onMRMLSceneStartBatchProcess()
{
  Q_Q(qMRMLMarkupsToolBar);
  q->setEnabled(false);
}

//---------------------------------------------------------------------------
void qMRMLMarkupsToolBarPrivate::onMRMLSceneEndBatchProcess()
{
  Q_Q(qMRMLMarkupsToolBar);

  // re-enable in case it didn't get re-enabled for scene load
  q->setEnabled(true);

  q->setInteractionNode((this->MRMLAppLogic && this->MRMLScene) ? this->MRMLAppLogic->GetInteractionNode() : nullptr);
  q->setSelectionNode((this->MRMLAppLogic && this->MRMLScene) ? this->MRMLAppLogic->GetSelectionNode() : nullptr);

  // update the state from mrml
  this->updateWidgetFromMRML();
}


//---------------------------------------------------------------------------
QCursor qMRMLMarkupsToolBarPrivate::cursorFromIcon(QIcon& icon)
{
  QList<QSize> availableSizes = icon.availableSizes();
  if (availableSizes.size() > 0)
    {
    return QCursor(icon.pixmap(availableSizes[0]));
    }
  else
    {
    // use a default
    return QCursor(icon.pixmap(20));
    }
}

//---------------------------------------------------------------------------
void qMRMLMarkupsToolBarPrivate::updateWidgetFromMRML()
{
  Q_Q(qMRMLMarkupsToolBar);
  vtkMRMLInteractionNode* interactionNode = q->interactionNode();
  if (!interactionNode)
    {
    qDebug() << "Markups ToolBar: no interaction node";
    q->setEnabled(false);
    return;
    }
  vtkMRMLSelectionNode* selectionNode = q->selectionNode();
  if (!selectionNode)
    {
    q->setEnabled(false);
    return;
    }

  q->setEnabled(true);
  // Find action corresponding to current interaction mode
  int currentInteractionMode = interactionNode->GetCurrentInteractionMode();

  if (this->MarkupsPlaceWidget)
    {
    this->MarkupsPlaceWidget->setEnabled(true);
    this->MarkupsPlaceWidget->setInteractionNode(interactionNode);
    this->MarkupsPlaceWidget->setSelectionNode(selectionNode);
    }

}
//---------------------------------------------------------------------------
void qMRMLMarkupsToolBarPrivate::onActivePlaceNodeClassNameChangedEvent()
{
  this->updateWidgetFromMRML();
}

//---------------------------------------------------------------------------
void qMRMLMarkupsToolBarPrivate::onPlaceNodeClassNameListModifiedEvent()
{
  this->updateWidgetFromMRML();
}

// --------------------------------------------------------------------------
// qMRMLMarkupsToolBar methods

// --------------------------------------------------------------------------
qMRMLMarkupsToolBar::qMRMLMarkupsToolBar(const QString& title, QWidget* parentWidget)
  :Superclass(title, parentWidget)
   , d_ptr(new qMRMLMarkupsToolBarPrivate(*this))
{
  Q_D(qMRMLMarkupsToolBar);
  d->init();
}

// --------------------------------------------------------------------------
qMRMLMarkupsToolBar::qMRMLMarkupsToolBar(QWidget* _parent)
  : Superclass(_parent)
  , d_ptr(new qMRMLMarkupsToolBarPrivate(*this))
{
  Q_D(qMRMLMarkupsToolBar);
  d->init();
}

//---------------------------------------------------------------------------
qMRMLMarkupsToolBar::~qMRMLMarkupsToolBar() = default;

// --------------------------------------------------------------------------
void qMRMLMarkupsToolBar::setMRMLScene(vtkMRMLScene* scene)
{
  Q_D(qMRMLMarkupsToolBar);
  d->setMRMLScene(scene);
}

// --------------------------------------------------------------------------
vtkMRMLMarkupsNode* qMRMLMarkupsToolBar::activeMarkupsNode()
{
  Q_D(qMRMLMarkupsToolBar);
  return vtkMRMLMarkupsNode::SafeDownCast(d->MarkupsNodeSelector->currentNode());
}

// --------------------------------------------------------------------------
void qMRMLMarkupsToolBar::setActiveMarkupsNode(vtkMRMLMarkupsNode* newActiveNode)
{
  Q_D(qMRMLMarkupsToolBar);

  vtkMRMLMarkupsNode* currentMarkupsNode = vtkMRMLMarkupsNode::SafeDownCast(newActiveNode);
  if (currentMarkupsNode == d->CurrentMarkupsNode)
    {
    // not changed
    return;
    }
  d->MarkupsNodeSelector->setCurrentNode(currentMarkupsNode);
  d->MarkupsPlaceWidget->setCurrentNode(currentMarkupsNode);
  d->CurrentMarkupsNode = currentMarkupsNode;
}

//-----------------------------------------------------------------------------
vtkMRMLInteractionNode* qMRMLMarkupsToolBar::interactionNode()const
{
  Q_D(const qMRMLMarkupsToolBar);

  return d->InteractionNode;
}

//-----------------------------------------------------------------------------
vtkMRMLSelectionNode* qMRMLMarkupsToolBar::selectionNode()const
{
  Q_D(const qMRMLMarkupsToolBar);

  return d->SelectionNode;
}

//---------------------------------------------------------------------------
void qMRMLMarkupsToolBar::setPersistence(bool persistent)
{
  Q_D(qMRMLMarkupsToolBar);

  d->MarkupsPlaceWidget->setPlaceModePersistency(persistent ? true : false);
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsToolBar::setInteractionNode(vtkMRMLInteractionNode* interactionNode)
{
  Q_D(qMRMLMarkupsToolBar);

  if (d->InteractionNode == interactionNode)
    {
    return;
    }
  d->qvtkReconnect(d->InteractionNode, interactionNode, vtkCommand::ModifiedEvent,
    d, SLOT(updateWidgetFromMRML()));
  d->InteractionNode = interactionNode;
  d->updateWidgetFromMRML();
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsToolBar::setSelectionNode(vtkMRMLSelectionNode* selectionNode)
{
  Q_D(qMRMLMarkupsToolBar);

  if (d->SelectionNode == selectionNode)
    {
    return;
    }
  d->qvtkReconnect(d->SelectionNode, selectionNode, vtkCommand::ModifiedEvent,
    d, SLOT(updateWidgetFromMRML()));
  d->SelectionNode = selectionNode;
  d->updateWidgetFromMRML();
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsToolBar::interactionModeActionTriggered(bool toggled)
{
  Q_D(qMRMLMarkupsToolBar);

  if (!toggled)
    {
    return;
    }
  QAction* sourceAction = qobject_cast<QAction*>(sender());
  if (!sourceAction)
    {
    return;
    }
  int selectedInteractionMode = sourceAction->data().toInt();
  if (!d->InteractionNode)
    {
    return;
    }
  d->InteractionNode->SetCurrentInteractionMode(selectedInteractionMode);

  // If no active place node class name is selected then use the default class
  if (d->InteractionNode->GetCurrentInteractionMode() == vtkMRMLInteractionNode::Place)
    {
    vtkMRMLSelectionNode* selectionNode = (d->MRMLAppLogic && d->MRMLScene) ?
      d->MRMLAppLogic->GetSelectionNode() : nullptr;
    if (selectionNode)
      {
      const char* currentPlaceNodeClassName = selectionNode->GetActivePlaceNodeClassName();
      if (!currentPlaceNodeClassName || strlen(currentPlaceNodeClassName) == 0)
        {
        selectionNode->SetReferenceActivePlaceNodeClassName(d->DefaultPlaceClassName.toUtf8());
        }
      }
  }
}

//---------------------------------------------------------------------------
void qMRMLMarkupsToolBar::setApplicationLogic(vtkSlicerApplicationLogic* appLogic)
{
  Q_D(qMRMLMarkupsToolBar);
  d->MRMLAppLogic = appLogic;
}

//---------------------------------------------------------------------------
void qMRMLMarkupsToolBar::addNodeActions(vtkSlicerMarkupsLogic* markupsLogic)
{
  // Node creation buttons
  Q_D(qMRMLMarkupsToolBar);

  d->addCreateNodeActions(markupsLogic);
  this->addWidget(d->MarkupsNodeSelector);
  d->addPlaceWidget();
  this->addSeparator();
  d->addSetModuleButton(markupsLogic, "Markups");
  d->addSetModuleButton(markupsLogic, "Annotations");
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsToolBar::onCreateMarkupByClass(const QString& className)
{
  Q_D(qMRMLMarkupsToolBar);

  if (d->MRMLScene)
    {
    vtkMRMLNode* node = d->MRMLScene->AddNewNodeByClass(className.toStdString().c_str());
    vtkMRMLMarkupsNode* markupsNode = vtkMRMLMarkupsNode::SafeDownCast(node);
    if (markupsNode)
      {
      std::string nodeName = d->MRMLScene->GenerateUniqueName(markupsNode->GetDefaultNodeNamePrefix());
      markupsNode->SetName(nodeName.c_str());
      d->updateWidgetFromMRML();
      d->MarkupsPlaceWidget->setPlaceModeEnabled(true);
      }
    else
      {
      qCritical() << Q_FUNC_INFO << ": node added is not a vtkMRMLMarkupsNode.";
      return;
      }
    }
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsToolBar::onCreateAnnotationByClass(const QString& className)
{
  Q_D(qMRMLMarkupsToolBar);

  if (d->MRMLScene)
    {
    d->updateWidgetFromMRML();
    this->selectionNode()->SetReferenceActivePlaceNodeClassName(className.toUtf8());
    this->interactionNode()->SetCurrentInteractionMode(vtkMRMLInteractionNode::Place);
    }
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsToolBar::onMarkupsNodeChanged()
{
  Q_D(qMRMLMarkupsToolBar);
  vtkMRMLMarkupsNode* currentMarkupsNode = vtkMRMLMarkupsNode::SafeDownCast(d->MarkupsNodeSelector->currentNode());
  if (d->MarkupsPlaceWidget)
    {
    this->setActiveMarkupsNode(currentMarkupsNode);
    }

}

//-----------------------------------------------------------------------------
void qMRMLMarkupsToolBar::addCreateNodeShortcut(QString keySequence)
{
  QObject::connect(new QShortcut(QKeySequence(keySequence), this), SIGNAL(activated()), SLOT(onCreateNodeShortcut()));

}


//-----------------------------------------------------------------------------
void qMRMLMarkupsToolBar::addTogglePersistenceShortcut(QString keySequence)
{
  QObject::connect(new QShortcut(QKeySequence(keySequence), this), SIGNAL(activated()), SLOT(onTogglePersistenceShortcut()));

}


//-----------------------------------------------------------------------------
void qMRMLMarkupsToolBar::addPlacePointShortcut(QString keySequence)
{
  QObject::connect(new QShortcut(QKeySequence(keySequence), this), SIGNAL(activated()), SLOT(onPlacePointShortcut()));

}

//-----------------------------------------------------------------------------
void qMRMLMarkupsToolBar::onCreateNodeShortcut()
{
  Q_D(qMRMLMarkupsToolBar);

  vtkMRMLMarkupsNode* currentMarkupsNode = vtkMRMLMarkupsNode::SafeDownCast(d->MarkupsNodeSelector->currentNode());
  QString className = d->DefaultPlaceClassName;
  if (currentMarkupsNode != nullptr)
    {
    className = d->MarkupsPlaceWidget->currentNode()->GetClassName();
    }
  this->onCreateMarkupByClass(className);
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsToolBar::onTogglePersistenceShortcut()
{
  Q_D(qMRMLMarkupsToolBar);
  bool persistent = d->MarkupsPlaceWidget->placeModePersistency();
  this->setPersistence(persistent ? false : true);
}

//-----------------------------------------------------------------------------
void qMRMLMarkupsToolBar::onPlacePointShortcut()
{
  Q_D(qMRMLMarkupsToolBar);
  bool placeModeActive = d->MarkupsPlaceWidget->placeModeEnabled();
  d->MarkupsPlaceWidget->setPlaceModeEnabled(placeModeActive ? false : true);
}
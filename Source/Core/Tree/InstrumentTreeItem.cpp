/*
    This file is part of Helio Workstation.

    Helio is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Helio is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Helio. If not, see <http://www.gnu.org/licenses/>.
*/

#include "Common.h"
#include "InstrumentTreeItem.h"
#include "TreeItemChildrenSerializer.h"
#include "MainLayout.h"
#include "Icons.h"
#include "SerializationKeys.h"

#include "Instrument.h"
#include "InstrumentEditor.h"
#include "InstrumentMenu.h"
#include "OrchestraPit.h"
#include "OrchestraPitTreeItem.h"
#include "AudioPluginTreeItem.h"

#include "App.h"
#include "Workspace.h"
#include "AudioCore.h"


InstrumentTreeItem::InstrumentTreeItem(Instrument *targetInstrument) :
    TreeItem({}, Serialization::Core::instrumentRoot),
    instrument(targetInstrument),
    instrumentEditor(nullptr)
{
    this->audioCore = &App::Workspace().getAudioCore();
    
    if (this->instrument != nullptr)
    {
        this->name = this->instrument->getName();
        this->initInstrumentEditor();
    }
}

InstrumentTreeItem::~InstrumentTreeItem()
{
    // cleanup UI before unplugging an instrument
    this->deleteAllSubItems();
    this->removeInstrumentEditor();

    if (!this->instrument.wasObjectDeleted())
    {
        this->audioCore->removeInstrument(this->instrument);
    }
}

Colour InstrumentTreeItem::getColour() const noexcept
{
    return Colour(0xffff80f3).interpolatedWith(Colour(0xffa489ff), 0.5f);
    //return Colour(0xffd151ff);
}

Image InstrumentTreeItem::getIcon() const noexcept
{
    return Icons::findByName(Icons::instrument, HEADLINE_ICON_SIZE);
}

void InstrumentTreeItem::showPage()
{
    if (this->instrument.wasObjectDeleted())
    { 
        delete this;
        return;
    }

    App::Layout().showPage(this->instrumentEditor, this);
}

void InstrumentTreeItem::safeRename(const String &newName)
{
    if (this->instrument.wasObjectDeleted())
    { 
        delete this;
        return;
    }

    TreeItem::safeRename(newName);
    this->instrument->setName(newName);
    this->dispatchChangeTreeItemView();
    this->notifyOrchestraChanged();
}


//===----------------------------------------------------------------------===//
// Instrument
//===----------------------------------------------------------------------===//

Instrument *InstrumentTreeItem::getInstrument() const
{
    return this->instrument;
}

String InstrumentTreeItem::getInstrumentIdAndHash() const
{
    return this->instrument->getIdAndHash();
}

Array<uint32> InstrumentTreeItem::getInstrumentNodeIds() const
{
    Array<uint32> result;

    for (int i = 0; i < this->instrument->getNumNodes(); ++i)
    {
        result.add(this->instrument->getNode(i)->nodeID);
    }

    return result;
}

bool InstrumentTreeItem::hasInstrumentWithNodeId(uint32 targetNodeId) const
{
    for (const uint32 existingNodeId : this->getInstrumentNodeIds())
    {
        if (existingNodeId == targetNodeId)
        {
            return true;
        }
    }

    return false;
}

TreeItem *InstrumentTreeItem::findAudioPluginEditorForNodeId(uint32 nodeId) const
{
    for (const auto audioPluginTreeItem : this->findChildrenOfType<AudioPluginTreeItem>())
    {
        if (audioPluginTreeItem->getNodeId() == nodeId)
        {
            return audioPluginTreeItem;
        }
    }

    return nullptr;
}


//===----------------------------------------------------------------------===//
// Dragging
//===----------------------------------------------------------------------===//

var InstrumentTreeItem::getDragSourceDescription()
{
    return Serialization::Core::instrumentRoot.toString();
}

bool InstrumentTreeItem::isInterestedInDragSource(const DragAndDropTarget::SourceDetails &dragSourceDetails)
{
    const bool isInterested =
        (nullptr != dynamic_cast<PluginDescriptionDragnDropWrapper *>(dragSourceDetails.description.getObject()));

    if (isInterested)
    { this->setOpen(true); }

    return isInterested;
}

void InstrumentTreeItem::itemDropped(const DragAndDropTarget::SourceDetails &dragSourceDetails, int insertIndex)
{
    if (ListBox *list = dynamic_cast<ListBox *>(dragSourceDetails.sourceComponent.get()))
    {
        if (auto pd = dynamic_cast<PluginDescriptionDragnDropWrapper *>(dragSourceDetails.description.getObject()))
        {
            const PluginDescription pluginDescription(pd->pluginDescription);
            this->instrument->addNodeToFreeSpace(pluginDescription, [this](Instrument *instrument)
            {
                this->updateChildrenEditors();
                Logger::writeToLog("Added " + instrument->getName());
            });
        }
    }

    TreeItem::itemDropped(dragSourceDetails, insertIndex);
}

//===----------------------------------------------------------------------===//
// Menu
//===----------------------------------------------------------------------===//

bool InstrumentTreeItem::hasMenu() const noexcept
{
    return true;
}

ScopedPointer<Component> InstrumentTreeItem::createMenu()
{
    return new InstrumentMenu(*this, App::Workspace().getPluginManager());
}

void InstrumentTreeItem::updateChildrenEditors()
{
    this->deleteAllSubItems();

    for (int i = 0; i < this->instrument->getNumNodes(); ++i)
    {
        const AudioProcessorGraph::Node::Ptr node = this->instrument->getNode(i);

        if (AudioProcessorGraph::AudioGraphIOProcessor *io =
                    dynamic_cast<AudioProcessorGraph::AudioGraphIOProcessor *>(node->getProcessor()))
        {
            continue; // для девайсов ввода-вывода показывать нечего, пропускаем
        }

        // неюзабельно, используем только на мобилках
        AudioPluginTreeItem *ap = new AudioPluginTreeItem(node->nodeID,
            node->getProcessor()->getName());
        this->addChildTreeItem(ap);
    }
}

//===----------------------------------------------------------------------===//
// Callbacks
//===----------------------------------------------------------------------===//

Function<void(const String &text)> InstrumentTreeItem::getRenameCallback()
{
    return [this](const String &text)
    {
        if (text != this->getName())
        {
            this->safeRename(text);
        }
    };
}

//===----------------------------------------------------------------------===//
// Serializable
//===----------------------------------------------------------------------===//

ValueTree InstrumentTreeItem::serialize() const
{
    ValueTree tree(Serialization::Core::treeItem);
    tree.setProperty(Serialization::Core::treeItemType, this->type, nullptr);
    tree.setProperty(Serialization::Core::treeItemName, this->name, nullptr);
    tree.setProperty(Serialization::Audio::instrumentId, this->instrument->getIdAndHash(), nullptr);
    return tree;
}

void InstrumentTreeItem::deserialize(const ValueTree &tree)
{
    this->reset();

    const String id = tree.getProperty(Serialization::Audio::instrumentId);
    this->instrument = this->audioCore->findInstrumentById(id);

    // если в аудиоядре инструмент исчез:
    if (this->instrument == nullptr)
    {
        delete this;
        return;
    }

    this->initInstrumentEditor();

    // Proceed with basic properties and children
    TreeItem::deserialize(tree);

    this->updateChildrenEditors();
}

void InstrumentTreeItem::initInstrumentEditor()
{
    if (this->instrumentEditor == nullptr)
    {
        this->instrumentEditor = new InstrumentEditor(this->instrument, this->audioCore);
        this->instrumentEditor->updateComponents();
    }
}

void InstrumentTreeItem::removeInstrumentEditor()
{
    if (this->instrumentEditor != nullptr)
    {
        this->instrumentEditor = nullptr;
    }
}

void InstrumentTreeItem::notifyOrchestraChanged()
{
    if (auto orchestra = dynamic_cast<OrchestraPitTreeItem *>(this->getParentItem()))
    {
        orchestra->sendChangeMessage();
    }
}

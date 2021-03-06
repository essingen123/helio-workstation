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
#include "NoteComponent.h"
#include "PianoSequence.h"
#include "PianoRoll.h"
#include "ProjectTreeItem.h"
#include "MidiSequence.h"
#include "MidiTrack.h"
#include "Note.h"
#include "SelectionComponent.h"
#include "SequencerOperations.h"
#include "Transport.h"
#include "App.h"
#include "MainWindow.h"

#define RESIZE_CORNER 10
#define MAX_DRAG_POLYPHONY 8

static PianoSequence *getPianoSequence(SelectionProxyArray::Ptr selection)
{
    const auto &firstEvent = selection->getFirstAs<NoteComponent>()->getNote();
    PianoSequence *pianoLayer = static_cast<PianoSequence *>(firstEvent.getSequence());
    return pianoLayer;
}

NoteComponent::NoteComponent(PianoRoll &editor, const Note &event, bool ghostMode)
    : MidiEventComponent(editor, ghostMode),
      midiEvent(event),
      state(None),
      anchor(event),
      groupScalingAnchor(event),
      firstChangeDone(false)
{
    this->updateColours();
    this->toFront(false);
    this->setPaintingIsUnclipped(true);
    this->setFloatBounds(this->getRoll().getEventBounds(this));
}

const Note &NoteComponent::getNote() const noexcept
{
    return static_cast<const Note &>(this->midiEvent);
}

PianoRoll &NoteComponent::getRoll() const noexcept
{
    return static_cast<PianoRoll &>(this->roll);
}

//===----------------------------------------------------------------------===//
// Accessors
//===----------------------------------------------------------------------===//

int NoteComponent::getKey() const noexcept
{
    return static_cast<const Note &>(this->midiEvent).getKey();
}

float NoteComponent::getLength() const noexcept
{
    return static_cast<const Note &>(this->midiEvent).getLength();
}

float NoteComponent::getVelocity() const noexcept
{
    return static_cast<const Note &>(this->midiEvent).getVelocity();
}

void NoteComponent::updateColours()
{
    this->colour = Colours::white
        .interpolatedWith(this->getNote().getColour(), 0.5f)
        .withAlpha(this->ghostMode ? 0.2f : 0.95f)
        .darker(this->selectedState ? 0.5f : 0.f);

    this->colourLighter = this->colour.brighter(0.125f);
    this->colourDarker = this->colour.darker(0.175f);
    this->colourVolume = Colours::black.withAlpha(0.4f);
}

bool NoteComponent::canResize() const noexcept
{
     return (this->getWidth() >= (RESIZE_CORNER * 3));
}

bool NoteComponent::shouldGoQuickSelectLayerMode(const ModifierKeys &modifiers) const
{
    return (modifiers.isAltDown() || modifiers.isRightButtonDown());
}

void NoteComponent::setQuickSelectLayerMode(bool value)
{
    if (value)
    {
        //Logger::writeToLog("setQuickSelectLayerMode true");
        this->setMouseCursor(MouseCursor::CrosshairCursor);
    }
    else
    {
        //Logger::writeToLog("setQuickSelectLayerMode false");
        this->setMouseCursor(MouseCursor::NormalCursor);
    }
}

//===----------------------------------------------------------------------===//
// MidiEventComponent
//===----------------------------------------------------------------------===//

void NoteComponent::setSelected(bool selected)
{
    MidiEventComponent::setSelected(selected);
}

float NoteComponent::getBeat() const
{
    return this->midiEvent.getBeat();
}

String NoteComponent::getSelectionGroupId() const
{
    return this->midiEvent.getSequence()->getTrackId();
}

String NoteComponent::getId() const
{
    return this->midiEvent.getId();
}

//===----------------------------------------------------------------------===//
// Component
//===----------------------------------------------------------------------===//

bool NoteComponent::keyStateChanged(bool isKeyDown)
{
    return this->roll.keyStateChanged(isKeyDown);
}

void NoteComponent::modifierKeysChanged(const ModifierKeys &modifiers)
{
    this->roll.modifierKeysChanged(modifiers);

    // Space drag mode
    const bool mode = this->shouldGoQuickSelectLayerMode(modifiers);
    this->setQuickSelectLayerMode(mode);
}

void NoteComponent::mouseMove(const MouseEvent &e)
{
    const bool selectLayerMode = this->shouldGoQuickSelectLayerMode(e.mods);
    this->setQuickSelectLayerMode(selectLayerMode);
    if (selectLayerMode)
    {
        return;
    }

    if (! this->isActive())
    {
        this->roll.mouseMove(e.getEventRelativeTo(&this->roll));
        return;
    }

    if (this->canResize() &&
        (e.x >= (this->getWidth() - RESIZE_CORNER) ||
         e.x <= RESIZE_CORNER))
    {
        this->setMouseCursor(MouseCursor::LeftRightResizeCursor);
    }
    else
    {
        this->setMouseCursor(MouseCursor::NormalCursor);
    }
}

void NoteComponent::mouseDown(const MouseEvent &e)
{
    if (this->shouldGoQuickSelectLayerMode(e.mods))
    {
        const bool selectAll = false; // e.mods.isRightButtonDown();
        const bool soloMode = !e.mods.isShiftDown();
        this->activateCorrespondingTrack(selectAll, soloMode);
        return;
    }
    
    if (! this->isActive())
    {
        this->roll.mouseDown(e.getEventRelativeTo(&this->roll));
        return;
    }
    
    //const double transportPosition = this->roll.getTransportPositionByBeat(this->getBeat());
    //Logger::writeToLog("Beat: " + String(this->getBeat()) + ", transport position: " + String(transportPosition));
    
    if (e.mods.isRightButtonDown() &&
        this->roll.getEditMode().isMode(HybridRollEditMode::defaultMode))
    {
        this->setMouseCursor(MouseCursor::DraggingHandCursor);
        this->roll.mouseDown(e.getEventRelativeTo(&this->roll));
        return;
    }

    MidiEventComponent::mouseDown(e);

    const Lasso &selection = this->roll.getLassoSelection();

    if (e.mods.isLeftButtonDown())
    {
#if HELIO_MOBILE
        const bool shouldSendMidi = false;
#elif HELIO_DESKTOP
        const bool shouldSendMidi = (selection.getNumSelected() < MAX_DRAG_POLYPHONY);
#endif
        
        if (shouldSendMidi)
        {
            this->stopSound();
        }

        if (this->canResize() &&
            e.x >= (this->getWidth() - RESIZE_CORNER))
        {
            if (e.mods.isShiftDown())
            {
                const float groupStartBeat = SequencerOperations::findStartBeat(selection);
                
                for (int i = 0; i < selection.getNumSelected(); i++)
                {
                    if (NoteComponent *note = dynamic_cast<NoteComponent *>(selection.getSelectedItem(i)))
                    {
                        if (selection.shouldDisplayGhostNotes()) { note->getRoll().showGhostNoteFor(note); }
                        note->startGroupScalingRight(groupStartBeat);
                    }
                }
            }
            else
            {
                for (int i = 0; i < selection.getNumSelected(); i++)
                {
                    if (NoteComponent *note = dynamic_cast<NoteComponent *>(selection.getSelectedItem(i)))
                    {
                        if (selection.shouldDisplayGhostNotes()) { note->getRoll().showGhostNoteFor(note); }
                        note->startResizingRight(shouldSendMidi);
                    }
                }
            }
        }
        else if (this->canResize() && e.x <= RESIZE_CORNER)
        {
            if (e.mods.isShiftDown())
            {
                const float groupEndBeat = SequencerOperations::findEndBeat(selection);
                
                for (int i = 0; i < selection.getNumSelected(); i++)
                {
                    if (NoteComponent *note = dynamic_cast<NoteComponent *>(selection.getSelectedItem(i)))
                    {
                        if (selection.shouldDisplayGhostNotes()) { note->getRoll().showGhostNoteFor(note); }
                        note->startGroupScalingLeft(groupEndBeat);
                    }
                }
            }
            else
            {
                for (int i = 0; i < selection.getNumSelected(); i++)
                {
                    if (NoteComponent *note = dynamic_cast<NoteComponent *>(selection.getSelectedItem(i)))
                    {
                        if (selection.shouldDisplayGhostNotes()) { note->getRoll().showGhostNoteFor(note); }
                        note->startResizingLeft(shouldSendMidi);
                    }
                }
            }
        }
        else
        {
            this->dragger.startDraggingComponent(this, e);
            
            for (int i = 0; i < selection.getNumSelected(); i++)
            {
                if (NoteComponent *note = dynamic_cast<NoteComponent *>(selection.getSelectedItem(i)))
                {
                    if (selection.shouldDisplayGhostNotes()) { note->getRoll().showGhostNoteFor(note); }
                    note->startDragging(shouldSendMidi);
                }
            }
        }

        //DBG("Note: " + String(note) + " " + String(beat) + " " + String(length));
    }
    else if (e.mods.isMiddleButtonDown())
    {
        this->midiEvent.getSequence()->checkpoint();

        this->setMouseCursor(MouseCursor::UpDownResizeCursor);

        for (int i = 0; i < selection.getNumSelected(); i++)
        {
            if (NoteComponent *note = dynamic_cast<NoteComponent *>(selection.getSelectedItem(i)))
            {
                note->startTuning();
            }
        }
    }
}

static int lastDeltaKey = 0;

void NoteComponent::mouseDrag(const MouseEvent &e)
{
    if (this->shouldGoQuickSelectLayerMode(e.mods))
    {
        return;
    }
    
    if (!this->isActive())
    {
        this->roll.mouseDrag(e.getEventRelativeTo(&this->roll));
        return;
    }

    if (e.mods.isRightButtonDown() && this->roll.getEditMode().isMode(HybridRollEditMode::defaultMode))
    {
        this->roll.mouseDrag(e.getEventRelativeTo(&this->roll));
        return;
    }
    
    const auto &selection = this->roll.getLassoSelection();

    if (this->state == ResizingRight)
    {
        float deltaLength = 0.f;
        const bool lengthChanged = this->getResizingRightDelta(e, deltaLength);

        if (lengthChanged)
        {
            this->checkpointIfNeeded();
            for (const auto &s : selection.getGroupedSelections())
            {
                const auto sequenceSelection(s.second);
                Array<Note> groupDragBefore, groupDragAfter;
                
                for (int i = 0; i < sequenceSelection->size(); ++i)
                {
                    NoteComponent *nc = static_cast<NoteComponent *>(sequenceSelection->getUnchecked(i));
                    groupDragBefore.add(nc->getNote());
                    groupDragAfter.add(nc->continueResizingRight(deltaLength));
                }
                
                getPianoSequence(sequenceSelection)->changeGroup(groupDragBefore, groupDragAfter, true);
            }
        }
        else
        {
            this->setFloatBounds(this->getRoll().getEventBounds(this)); // возвращаем на место
        }
    }
    else if (this->state == ResizingLeft)
    {
        float deltaLength = 0.f;
        const bool lengthChanged = this->getResizingLeftDelta(e, deltaLength);
        
        if (lengthChanged)
        {
            this->checkpointIfNeeded();
            for (const auto &s : selection.getGroupedSelections())
            {
                const auto sequenceSelection(s.second);
                Array<Note> groupDragBefore, groupDragAfter;
                
                for (int i = 0; i < sequenceSelection->size(); ++i)
                {
                    NoteComponent *nc = static_cast<NoteComponent *>(sequenceSelection->getUnchecked(i));
                    groupDragBefore.add(nc->getNote());
                    groupDragAfter.add(nc->continueResizingLeft(deltaLength));
                }
                
                getPianoSequence(sequenceSelection)->changeGroup(groupDragBefore, groupDragAfter, true);
            }
        }
        else
        {
            this->setFloatBounds(this->getRoll().getEventBounds(this)); // возвращаем на место
        }
    }
    else if (this->state == GroupScalingRight)
    {
        float groupScaleFactor = 1.f;
        const bool scaleFactorChanged = this->getGroupScaleRightFactor(e, groupScaleFactor);
        
        if (scaleFactorChanged)
        {
            this->checkpointIfNeeded();
            for (const auto &s : selection.getGroupedSelections())
            {
                const auto sequenceSelection(s.second);
                Array<Note> groupDragBefore, groupDragAfter;
                
                for (int i = 0; i < sequenceSelection->size(); ++i)
                {
                    NoteComponent *nc = static_cast<NoteComponent *>(sequenceSelection->getUnchecked(i));
                    groupDragBefore.add(nc->getNote());
                    groupDragAfter.add(nc->continueGroupScalingRight(groupScaleFactor));
                }
                
                getPianoSequence(sequenceSelection)->changeGroup(groupDragBefore, groupDragAfter, true);
            }
        }
        else
        {
            this->setFloatBounds(this->getRoll().getEventBounds(this)); // возвращаем на место
        }
    }
    else if (this->state == GroupScalingLeft)
    {
        float groupScaleFactor = 1.f;
        const bool scaleFactorChanged = this->getGroupScaleLeftFactor(e, groupScaleFactor);
        
        if (scaleFactorChanged)
        {
            this->checkpointIfNeeded();
            for (const auto &s : selection.getGroupedSelections())
            {
                const auto sequenceSelection(s.second);
                Array<Note> groupDragBefore, groupDragAfter;
                
                for (int i = 0; i < sequenceSelection->size(); ++i)
                {
                    NoteComponent *nc = static_cast<NoteComponent *>(sequenceSelection->getUnchecked(i));
                    groupDragBefore.add(nc->getNote());
                    groupDragAfter.add(nc->continueGroupScalingLeft(groupScaleFactor));
                }
                
                getPianoSequence(sequenceSelection)->changeGroup(groupDragBefore, groupDragAfter, true);
            }
        }
        else
        {
            this->setFloatBounds(this->getRoll().getEventBounds(this)); // возвращаем на место
        }
    }
    else if (this->state == Dragging)
    {
        this->getRoll().showHelpers();

        // двигаем саму ноту и смотрим новую позицию
        int deltaKey = 0;
        float deltaBeat = 0.f;
        const bool eventChanged = this->getDraggingDelta(e, deltaBeat, deltaKey);
        
#if HELIO_MOBILE
        const bool shouldSendMidi = false;
#elif HELIO_DESKTOP
        const bool shouldSendMidi = (lastDeltaKey != deltaKey) && (selection.getNumSelected() < MAX_DRAG_POLYPHONY);
#endif
        lastDeltaKey = deltaKey;
        
        // возвращаем на место активную ноту
        // это здесь, чтоб не было лага между сдвинутой нотой и асинхронным обновлением ролла
        this->setFloatBounds(this->getRoll().getEventBounds(this));
        
        if (eventChanged)
        {
            const bool firstChangeIsToCome = !this->firstChangeDone;

            this->checkpointIfNeeded();
            
            // Drag-and-copy logic:
            if (firstChangeIsToCome && e.mods.isAnyModifierKeyDown())
            {
                // We duplicate the notes only at the very moment when they are about to be moved into the new position,
                // to make sure that simple shift-clicks on a selection won't confuse a user with lots of silently created notes.

                // Another trick is that user just continues to drag the originally selected notes,
                // while the duplicated ones stay in their places, not vice versa, which simplifies the logic pretty much
                // (though that behaviour may appear counterintuitive later, if we ever create a merge-tool with a diff viewer).

                SequencerOperations::duplicateSelection(this->getRoll().getLassoSelection(), false);

                // Ghost note markers become useless from now on, as there are duplicates in their places already:
                this->getRoll().hideAllGhostNotes();
            }

            this->getRoll().moveHelpers(deltaBeat, deltaKey);
            
            if (shouldSendMidi)
            {
                this->stopSound();
            }
            
            for (const auto &s : selection.getGroupedSelections())
            {
                const auto sequenceSelection(s.second);
                Array<Note> groupDragBefore, groupDragAfter;
                
                for (int i = 0; i < sequenceSelection->size(); ++i)
                {
                    NoteComponent *nc = static_cast<NoteComponent *>(sequenceSelection->getUnchecked(i));
                    groupDragBefore.add(nc->getNote());
                    groupDragAfter.add(nc->continueDragging(deltaBeat, deltaKey, shouldSendMidi));
                }
                
                getPianoSequence(sequenceSelection)->changeGroup(groupDragBefore, groupDragAfter, true);
            }
        }
    }
    else if (this->state == Tuning)
    {
        this->checkpointIfNeeded();
        
        for (const auto &s : selection.getGroupedSelections())
        {
            const auto sequenceSelection(s.second);
            Array<Note> groupTuneBefore, groupTuneAfter;
            
            for (int i = 0; i < sequenceSelection->size(); ++i)
            {
                NoteComponent *nc = static_cast<NoteComponent *>(sequenceSelection->getUnchecked(i));
                groupTuneBefore.add(nc->getNote());
                
                Note tuned(nc->continueTuning(e));
                groupTuneAfter.add(tuned);
                
                //this->getRoll().setDefaultNoteVelocity(tuned.getVelocity());
                //this->roll.triggerBatchRepaintFor(nc);
            }
            
            getPianoSequence(sequenceSelection)->changeGroup(groupTuneBefore, groupTuneAfter, true);
        }
    }
}

void NoteComponent::mouseUp(const MouseEvent &e)
{
    if (this->shouldGoQuickSelectLayerMode(e.mods))
    {
        return;
    }
    
    if (!this->isActive())
    {
        this->roll.mouseUp(e.getEventRelativeTo(&this->roll));
        return;
    }

    if (e.mods.isRightButtonDown() && this->roll.getEditMode().isMode(HybridRollEditMode::defaultMode))
    {
        this->setMouseCursor(MouseCursor::NormalCursor);
        this->roll.mouseUp(e.getEventRelativeTo(&this->roll));
        return;
    }
    
    this->getRoll().hideAllGhostNotes();

#if HELIO_MOBILE
    const bool shouldSendMidi = false;
#elif HELIO_DESKTOP
    const bool shouldSendMidi = true;
#endif

    
    Lasso &selection = this->roll.getLassoSelection();

    if (this->state == ResizingRight)
    {
        //this->updateBounds(this->getRoll().getEventBounds(this)); // анти-лаг (?)

        for (int i = 0; i < selection.getNumSelected(); i++)
        {
            NoteComponent *nc = static_cast<NoteComponent *>(selection.getSelectedItem(i));
            nc->endResizingRight();
        }
    }
    else if (this->state == ResizingLeft)
    {
        for (int i = 0; i < selection.getNumSelected(); i++)
        {
            NoteComponent *nc = static_cast<NoteComponent *>(selection.getSelectedItem(i));
            nc->endResizingLeft();
        }
    }
    else if (this->state == GroupScalingRight)
    {
        for (int i = 0; i < selection.getNumSelected(); i++)
        {
            NoteComponent *nc = static_cast<NoteComponent *>(selection.getSelectedItem(i));
            nc->endGroupScalingRight();
        }
    }
    else if (this->state == GroupScalingLeft)
    {
        for (int i = 0; i < selection.getNumSelected(); i++)
        {
            NoteComponent *nc = static_cast<NoteComponent *>(selection.getSelectedItem(i));
            nc->endGroupScalingLeft();
        }
    }
    else if (this->state == Dragging)
    {
        this->getRoll().hideHelpers();
        this->setFloatBounds(this->getRoll().getEventBounds(this));
        
        for (int i = 0; i < selection.getNumSelected(); i++)
        {
            NoteComponent *nc = static_cast<NoteComponent *>(selection.getSelectedItem(i));
            nc->endDragging(shouldSendMidi);
        }
    }
    else if (this->state == Tuning)
    {
        for (int i = 0; i < selection.getNumSelected(); i++)
        {
            NoteComponent *note = static_cast<NoteComponent *>(selection.getSelectedItem(i));
            note->endTuning();
        }

        this->setMouseCursor(MouseCursor::NormalCursor);
    }

    // TODO: show selection menu on right click?
//    if (e.getDistanceFromDragStart() < 5 &&
//        e.mods.isRightButtonDown())
//    {
//        if (selection.getNumSelected() > 0)
//        {
//            this->getRoll().showSelectionPopup(e.getEventRelativeTo(&this->getRoll()));
//        }
//    }
}

void NoteComponent::mouseDoubleClick(const MouseEvent &e)
{
    // Double right click - activate this layer and others
    const bool selectAll = e.mods.isRightButtonDown();
    this->activateCorrespondingTrack(selectAll, true);
}

//===----------------------------------------------------------------------===//
// Notes painting, the very bottleneck of all rendering process
//===----------------------------------------------------------------------===//

// Drawing note as a number of lines, not just rounded rect,
// gives an overwhelming performance boost on OpenGL and DirectX
// CoreGraphics, though, performs very badly this way

void NoteComponent::paint(Graphics &g)
{
#if JUCE_MAC
    if (MainWindow::isOpenGLRendererEnabled())
    {
        this->paintNewLook(g);
    }
    else
    {
        this->paintLegacyLook(g);
    }
#else
    this->paintNewLook(g);
#endif
}

// Really fast approximation, we don't need accuracy here:
// (using Bhaskara I's sine approximation, ~5% error)
static constexpr float piSqr5 = static_cast<float>(49.3480220f);
static constexpr float fastSine(float x)
{
    return (16.f * x * (MathConstants<float>::pi - x)) /
        (piSqr5 - 4.f * x * (MathConstants<float>::pi - x));
}

void NoteComponent::paintNewLook(Graphics &g)
{
    const float w = this->floatLocalBounds.getWidth() - .75f; // a small gap between notes
    const float h = this->floatLocalBounds.getHeight();
    const float x1 = this->floatLocalBounds.getX();
    const float x2 = x1 + w;
    const float y1 = this->floatLocalBounds.getY();
    const float y2 = y1 + h - 1;
    const float yh = (y2 - y1);
    
    // Для маленьких нот надо убрать коэффициент скругления в 0
    // Для нот больше 6 пикселей - коэффициент = 1
    const float bevelCoeff = 1.f - jmax(0.f, (6.f - w) / 6.f);
    
    if (! this->activeState)
    {
        g.setColour(this->colourLighter);
        g.drawHorizontalLine(int(y1), x1 + 1.f, x2 - 1.f);
        g.setColour(this->colourDarker);
        g.drawHorizontalLine(int(y2), x1 + 1.f, x2 - 1.f);
        
        g.setColour(this->colour);
        for (float y = y1 + 1.f; y <= y2 - 1.f; y += 1.f)
        {
            const float yMap = (y - y1) / yh * MathConstants<float>::pi;
            const float bevel = bevelCoeff * (1.f - (fastSine(yMap) - fastSine(yMap) / 2.5f));
            g.drawHorizontalLine(int(y), x1 + bevel, x1 + bevel + 1.f);
            g.drawHorizontalLine(int(y), x2 - bevel - 1.f, x2 - bevel);
        }

        return;
    }
    
    g.setColour(this->colourLighter);
    g.drawHorizontalLine(int(y1), x1 + 1.f, x2 - 1.f);
    g.setColour(this->colourDarker);
    g.drawHorizontalLine(int(y2), x1 + 1.f, x2 - 1.f);
    
    g.setColour(this->colour);
    for (float y = y1 + 1.f; y <= y2 - 1.f; y += 1.f)
    {
        const float yMap = (y - y1) / yh * MathConstants<float>::pi;
        const float bevel = bevelCoeff * (1.f - (sin(yMap) - sin(yMap) / 2.5f));
        g.drawHorizontalLine(int(y), x1 + bevel, x2 - bevel);
    }
    
//#ifdef DEBUG
//    g.setColour(Colours::black);
//    g.drawText(String(this->getBeat()), this->getLocalBounds().translated(5, 0), Justification::centredLeft, false);
//#else
    const float sx = x1 + 2.f;
    const float sw = (w - 2.f) * this->getVelocity();
    g.setColour(this->colourVolume);
    g.drawHorizontalLine(this->getHeight() - 2, sx, sw);
    g.drawHorizontalLine(this->getHeight() - 3, sx, sw);
    g.drawHorizontalLine(this->getHeight() - 4, sx, sw);
//#endif
}

void NoteComponent::paintLegacyLook(Graphics &g)
{
    g.setColour(this->colour);

    if (! this->activeState)
    {
        g.drawRoundedRectangle(this->floatLocalBounds.reduced(0.5f, 0.5f), 2.f, 1.0f);
        return;
    }
    
    g.fillRoundedRectangle(this->floatLocalBounds, 2.f);
    
    const float sx = this->floatLocalBounds.getX() + 2.f;
    const float sw = (this->floatLocalBounds.getWidth() - 2.f - .75f) * this->getVelocity();

    g.setColour(this->colourVolume);
    g.drawHorizontalLine(this->getHeight() - 2, sx, sw);
    g.drawHorizontalLine(this->getHeight() - 3, sx, sw);
    g.drawHorizontalLine(this->getHeight() - 4, sx, sw);
}

//===----------------------------------------------------------------------===//
// Helpers
//===----------------------------------------------------------------------===//

bool NoteComponent::belongsToAnyTrack(const Array<WeakReference<MidiTrack>> &tracks) const
{
    for (int i = 0; i < tracks.size(); ++i)
    {
        if (this->getNote().getSequence() == tracks.getUnchecked(i)->getSequence())
        {
            return true;
        }
    }

    return false;
}

void NoteComponent::activateCorrespondingTrack(bool selectAll, bool soloSelection)
{
    MidiSequence *layer = this->getNote().getSequence();
    this->roll.getProject().activateLayer(layer, selectAll, soloSelection);
}

//===----------------------------------------------------------------------===//
// Dragging
//===----------------------------------------------------------------------===//

void NoteComponent::startDragging(const bool sendMidiMessage)
{
    this->firstChangeDone = false;
    this->state = Dragging;
    this->anchor = this->getNote();
    
    if (sendMidiMessage)
    {
        this->sendMidiMessage(MidiMessage::noteOn(1, this->getKey(), this->getVelocity()));
    }
}

bool NoteComponent::getDraggingDelta(const MouseEvent &e, float &deltaBeat, int &deltaKey)
{
    this->dragger.dragComponent(this, e, nullptr);

    int newKey = -1;
    float newBeat = -1;

    // по идее, там надо прибавлять this->clickOffset.getX() % (длина минимального деления)
    this->getRoll().getRowsColsByComponentPosition(this->getX() + this->floatLocalBounds.getX() + 1 /*+ this->clickOffset.getX()*/,
            this->getY() + (this->getHeight() / 2) + this->floatLocalBounds.getY() + this->clickOffset.getY(),
            newKey,
            newBeat);

    deltaKey = (newKey - this->anchor.getKey());
    deltaBeat = (newBeat - this->anchor.getBeat());

    const bool keyChanged = (this->getKey() != newKey);
    const bool beatChanged = (this->getBeat() != newBeat);

    return (keyChanged || beatChanged);
}

Note NoteComponent::continueDragging(float deltaBeat, int deltaKey, bool sendMidiMessage)
{
    const int &newKey = this->anchor.getKey() + deltaKey;
    const float &newBeat = this->anchor.getBeat() + deltaBeat;

    if (sendMidiMessage)
    {
        this->sendMidiMessage(MidiMessage::noteOn(1, newKey, this->getVelocity()));
    }

    return this->getNote().withKeyBeat(newKey, newBeat);
}

void NoteComponent::endDragging(bool sendMidiMessage)
{
    if (sendMidiMessage)
    {
        this->stopSound();
    }
    
    this->state = None;
}

//===----------------------------------------------------------------------===//
// Resizing Right
//===----------------------------------------------------------------------===//

void NoteComponent::setNoCheckpointNeededForNextAction()
{
    this->firstChangeDone = true;
}

bool NoteComponent::isResizing() const
{
    return (this->state == ResizingRight) || (this->state == ResizingLeft);
}

void NoteComponent::startResizingRight(bool sendMidiMessage)
{
    this->firstChangeDone = false;
    this->state = ResizingRight;
    this->anchor = this->getNote();
    
    if (sendMidiMessage)
    {
        this->sendMidiMessage(MidiMessage::noteOn(1, this->getKey(), this->getVelocity()));
    }
}

bool NoteComponent::getResizingRightDelta(const MouseEvent &e, float &deltaLength) const
{
    int newNote = -1;
    float newBeat = -1;

    this->getRoll().getRowsColsByComponentPosition(getX() + this->floatLocalBounds.getX() + e.x,
                                                   getY() + this->floatLocalBounds.getY() + e.y,
                                                   newNote,
                                                   newBeat);

    const float newLength = (newBeat - this->getBeat());
    deltaLength = newLength - this->anchor.getLength();

    const bool lengthChanged = (this->getLength() != newLength);
    return lengthChanged;
}

Note NoteComponent::continueResizingRight(float deltaLength)
{
    const float newLength = this->anchor.getLength() + deltaLength;
    return this->getNote().withLength(newLength);
}

void NoteComponent::endResizingRight()
{
    this->stopSound();
    this->state = None;
}

//===----------------------------------------------------------------------===//
// Resizing Left
//===----------------------------------------------------------------------===//

void NoteComponent::startResizingLeft(bool sendMidiMessage)
{
    this->firstChangeDone = false;
    this->state = ResizingLeft;
    this->anchor = this->getNote();
    
    if (sendMidiMessage)
    {
        this->sendMidiMessage(MidiMessage::noteOn(1, this->getKey(), this->getVelocity()));
    }
}

bool NoteComponent::getResizingLeftDelta(const MouseEvent &e, float &deltaLength) const
{
    int newNote = -1;
    float newBeat = -1;
    
    this->getRoll().getRowsColsByComponentPosition(getX() + this->floatLocalBounds.getX() + e.x,
                                                   getY() + this->floatLocalBounds.getY() + e.y,
                                                   newNote,
                                                   newBeat);
    
    deltaLength = this->anchor.getBeat() - newBeat;
    const bool lengthChanged = (this->getBeat() != newBeat);
    return lengthChanged;
}

Note NoteComponent::continueResizingLeft(float deltaLength)
{
    const float newLength = this->anchor.getLength() + deltaLength;
    const float newBeat = this->anchor.getBeat() - deltaLength;
    return this->getNote().withBeat(newBeat).withLength(newLength);
}

void NoteComponent::endResizingLeft()
{
    this->stopSound();
    this->state = None;
}

//===----------------------------------------------------------------------===//
// Group Scaling Right
//===----------------------------------------------------------------------===//

void NoteComponent::startGroupScalingRight(float groupStartBeat)
{
    this->firstChangeDone = false;
    this->state = GroupScalingRight;
    const float newLength = this->getLength() + (this->getBeat() - groupStartBeat);
    this->anchor = this->getNote();
    this->groupScalingAnchor = this->getNote().withBeat(groupStartBeat).withLength(newLength);
}

bool NoteComponent::getGroupScaleRightFactor(const MouseEvent &e, float &absScaleFactor) const
{
    int newNote = -1;
    float newBeat = -1;
    
    this->getRoll().getRowsColsByComponentPosition(getX() + this->floatLocalBounds.getX() + e.x,
                                                   getY() + this->floatLocalBounds.getY() + e.y,
                                                   newNote,
                                                   newBeat);
    
    const float minGroupLength = 1.f;
    const float myEndBeat = (this->getBeat() + this->getLength());
    const float newGroupLength = jmax(minGroupLength, (newBeat - this->groupScalingAnchor.getBeat()));
    absScaleFactor = newGroupLength / this->groupScalingAnchor.getLength();
    
    const bool endBeatChanged = (newBeat != myEndBeat);
    return endBeatChanged;
}

Note NoteComponent::continueGroupScalingRight(float absScaleFactor)
{
    const float anchorBeatDelta = this->anchor.getBeat() - this->groupScalingAnchor.getBeat();
    const float newLength = this->anchor.getLength() * absScaleFactor;
    const float newBeat = this->groupScalingAnchor.getBeat() + (anchorBeatDelta * absScaleFactor);
    return this->getNote().withBeat(newBeat).withLength(newLength);
}

void NoteComponent::endGroupScalingRight()
{
    this->state = None;
}

//===----------------------------------------------------------------------===//
// Group Scaling Left
//===----------------------------------------------------------------------===//

void NoteComponent::startGroupScalingLeft(float groupEndBeat)
{
    this->firstChangeDone = false;
    this->state = GroupScalingLeft;
    this->anchor = this->getNote();
    const float newLength = (groupEndBeat - this->getBeat());
    this->groupScalingAnchor = this->getNote().withLength(newLength);
}

bool NoteComponent::getGroupScaleLeftFactor(const MouseEvent &e, float &absScaleFactor) const
{
    int newNote = -1;
    float newBeat = -1;
    
    this->getRoll().getRowsColsByComponentPosition(this->getX() + this->floatLocalBounds.getX() + e.x,
                                                   this->getY() + this->floatLocalBounds.getY() + e.y,
                                                   newNote,
                                                   newBeat);
    
    const float minGroupLength = 1.f;
    const float groupAnchorEndBeat = this->groupScalingAnchor.getBeat() + this->groupScalingAnchor.getLength();
    const float newGroupLength = jmax(minGroupLength, (groupAnchorEndBeat - newBeat));
    absScaleFactor = newGroupLength / this->groupScalingAnchor.getLength();
    
    const bool endBeatChanged = (newBeat != this->getBeat());
    return endBeatChanged;
}

Note NoteComponent::continueGroupScalingLeft(float absScaleFactor)
{
    const float groupAnchorEndBeat = this->groupScalingAnchor.getBeat() + this->groupScalingAnchor.getLength();
    const float anchorBeatDelta = groupAnchorEndBeat - this->anchor.getBeat();
    const float newLength = this->anchor.getLength() * absScaleFactor;
    const float newBeat = groupAnchorEndBeat - (anchorBeatDelta * absScaleFactor);
    return this->getNote().withBeat(newBeat).withLength(newLength);
}

void NoteComponent::endGroupScalingLeft()
{
    this->state = None;
}

//===----------------------------------------------------------------------===//
// Velocity
//===----------------------------------------------------------------------===//

void NoteComponent::startTuning()
{
    this->firstChangeDone = false;
    this->state = Tuning;
    this->anchor = this->getNote();
}

Note NoteComponent::continueTuningLinear(float delta)
{
    const float newVelocity = (this->anchor.getVelocity() - delta);
    return this->getNote().withVelocity(newVelocity);
}

Note NoteComponent::continueTuningMultiplied(float factor)
{
    // -1 .. 0   ->   0 .. anchor
    // 0 .. 1    ->   anchor .. 1
    const float av = this->anchor.getVelocity();
    //const float newVelocity = (factor < 0) ? (av * (factor + 1.f)) : (av + ((1.f - av) * factor));
    const float newVelocity = (factor < 0) ? (av * (factor + 1.f)) : (av + (av * factor * 2));
    return this->getNote().withVelocity(newVelocity);
}

Note NoteComponent::continueTuningSine(float factor, float midline, float phase)
{
    // -1 .. 0   ->   0 .. anchor
    // 0 .. 1    ->   anchor .. 1
    const float amplitude = jmin(midline, (1.f - midline));
    const float sine = cosf(phase) * amplitude;
    const float av = this->anchor.getVelocity();
    const float f = (factor < 0) ? (factor + 1.f) : factor;
    const float downscale = ((av * f) + (midline * (1.f - f)));
    const float upscale = ((midline + sine) * f) + (av * (1.f - f));
    const float newVelocity = (factor < 0) ? downscale : upscale;
    return this->getNote().withVelocity(newVelocity);
}

Note NoteComponent::continueTuning(const MouseEvent &e)
{
    return this->continueTuningLinear(e.getDistanceFromDragStartY() / 250.0f);
}

void NoteComponent::endTuning()
{
    this->state = None;
    this->roll.triggerBatchRepaintFor(this);
}


void NoteComponent::checkpointIfNeeded()
{
    if (! this->firstChangeDone)
    {
        this->midiEvent.getSequence()->checkpoint();
        this->firstChangeDone = true;
    }
}

//===----------------------------------------------------------------------===//
// Shorthands
//===----------------------------------------------------------------------===//

void NoteComponent::stopSound()
{
    this->getRoll().getTransport().allNotesControllersAndSoundOff();
}

void NoteComponent::sendMidiMessage(const MidiMessage &message)
{
    const String layerId = this->getNote().getSequence()->getTrackId();
    this->getRoll().getTransport().sendMidiMessage(layerId, message);
}

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
#include "CommandIDs.h"

// Implements compile-time FNV1a hash:
constexpr uint32 fnv1a_32_val = 0x811c9dc5;
constexpr uint64 fnv1a_32_prime = 0x1000193;
inline constexpr uint32 CompileTimeHash(const char *const str, const uint32 value = fnv1a_32_val) noexcept
{
    return (str[0] == '\0') ? value : CompileTimeHash(&str[1], uint32(value ^ uint32(str[0])) * fnv1a_32_prime);
}

#define CASE_FOR(x) \
    case CompileTimeHash(#x): \
    return x; \

int CommandIDs::getIdForName(const String &command)
{
    switch (CompileTimeHash(command.toRawUTF8()))
    {
        CASE_FOR(Back)
        CASE_FOR(Cancel)
        CASE_FOR(Browse)
        CASE_FOR(IconButtonPressed)
        CASE_FOR(MenuButtonPressed)
        CASE_FOR(RootTreeItemPressed)
        CASE_FOR(HideDialog)
        CASE_FOR(HideCallout)
        CASE_FOR(ChangeTimeSignature)
        CASE_FOR(ChangeTimeSignatureConfirmed)
        CASE_FOR(DeleteTimeSignature)
        CASE_FOR(RenameAnnotation)
        CASE_FOR(RenameAnnotationConfirmed)
        CASE_FOR(SetAnnotationColour)
        CASE_FOR(DeleteAnnotation)
        CASE_FOR(AddAnnotation)
        CASE_FOR(AddAnnotationConfirmed)
        CASE_FOR(AddTimeSignature)
        CASE_FOR(AddTimeSignatureConfirmed)
        CASE_FOR(AddKeySignature)
        CASE_FOR(AddKeySignatureConfirmed)
        CASE_FOR(JumpToAnnotation)
        CASE_FOR(CreateArpeggiatorFromSelection)
        CASE_FOR(ApplyArpeggiator)
        CASE_FOR(InitWorkspace)
        CASE_FOR(RenameInstrument)
        CASE_FOR(UpdateInstrument)
        CASE_FOR(DeleteInstrument)
        CASE_FOR(ScanAllPlugins)
        CASE_FOR(ScanPluginsFolder)
        CASE_FOR(CreateInstrument)
        CASE_FOR(DeleteTrack)
        CASE_FOR(MuteTrack)
        CASE_FOR(UnmuteTrack)
        CASE_FOR(RenameTrack)
        CASE_FOR(SelectTrackColour)
        CASE_FOR(SelectTrackInstrument)
        CASE_FOR(SelectAllEvents)
        CASE_FOR(DuplicateTrackTo)
        CASE_FOR(SetTrackColour)
        CASE_FOR(SetTrackInstrument)
        CASE_FOR(MoveTrackToProject)
        CASE_FOR(DeleteEvents)
        CASE_FOR(CopyEvents)
        CASE_FOR(CutEvents)
        CASE_FOR(PasteEvents)
        CASE_FOR(CursorTool)
        CASE_FOR(DrawTool)
        CASE_FOR(SelectionTool)
        CASE_FOR(ZoomTool)
        CASE_FOR(DragTool)
        CASE_FOR(InsertSpaceTool)
        CASE_FOR(WipeSpaceTool)
        CASE_FOR(ScissorsTool)
        CASE_FOR(ZoomIn)
        CASE_FOR(ZoomOut)
        CASE_FOR(Undo)
        CASE_FOR(Redo)
        CASE_FOR(RefactorNotes)
        CASE_FOR(SplitNotes)
        CASE_FOR(ArpeggiateNotes)
        CASE_FOR(TweakNotesVolume)
        CASE_FOR(ShowAnnotations)
        CASE_FOR(ResetVolumeChanges)
        CASE_FOR(ApplyOpenGLRenderer)
        CASE_FOR(TransportStartPlayback)
        CASE_FOR(TransportPausePlayback)
        CASE_FOR(PopupMenuDismiss)
        CASE_FOR(PopupMenuDismissedAsDone)
        CASE_FOR(PopupMenuDismissedAsCancel)
        CASE_FOR(SelectRootItemPanel)
        CASE_FOR(DeselectRootItemPanel)
        CASE_FOR(UpdateRootItemPanel)
        CASE_FOR(LoginLogout)
        CASE_FOR(OpenProject)
        CASE_FOR(CreateNewProject)
        CASE_FOR(RenderToFLAC)
        CASE_FOR(RenderToOGG)
        CASE_FOR(RenderToWAV)
        CASE_FOR(AddItemsMenu)
        CASE_FOR(AddItemsMenuBack)
        CASE_FOR(AddMidiTrack)
        CASE_FOR(AddMidiTrackConfirmed)
        CASE_FOR(AddAutomationTrack)
        CASE_FOR(ImportMidi)
        CASE_FOR(ExportMidi)
        CASE_FOR(UnloadProject)
        CASE_FOR(DeleteProject)
        CASE_FOR(DeleteProjectConfirmed1)
        CASE_FOR(DeleteProjectConfirmed2)
        CASE_FOR(RefactorTransposeUp)
        CASE_FOR(RefactorTransposeDown)
        CASE_FOR(RefactorRemoveOverlaps)
        CASE_FOR(ProjectPatternEditor)
        CASE_FOR(ProjectLinearEditor)
        CASE_FOR(ProjectVersionsEditor)
        CASE_FOR(ProjectMainMenu)
        CASE_FOR(ProjectRenderMenu)
        CASE_FOR(ProjectBatchMenu)
        CASE_FOR(ProjectBatchMenuBack)
        CASE_FOR(ProjectInstrumentsMenu)
        CASE_FOR(AddTempoController)
        CASE_FOR(AddCustomController)
        CASE_FOR(BatchChangeInstrument)
        CASE_FOR(BatchSetInstrument)
        CASE_FOR(DismissModalDialogAsync)
        CASE_FOR(SelectFunction)
        CASE_FOR(SelectScale)
        CASE_FOR(SelectTimeSignature)
        CASE_FOR(SwitchBetweenRolls)
        CASE_FOR(ShowPreviousPage)
        CASE_FOR(ShowNextPage)
        CASE_FOR(ToggleShowHideConsole)
        CASE_FOR(ToggleShowHideCombo)
        CASE_FOR(StartDragViewport)
        CASE_FOR(EndDragViewport)
        CASE_FOR(SelectAudioDeviceType)
        CASE_FOR(SelectAudioDevice)
        CASE_FOR(SelectSampleRate)
        CASE_FOR(SelectBufferSize)
        CASE_FOR(EditModeDefault)
        CASE_FOR(EditModeDraw)
        CASE_FOR(EditModePan)
        CASE_FOR(EditModeWipeSpace)
        CASE_FOR(EditModeInsertSpace)
        CASE_FOR(EditModeSelect)
        CASE_FOR(BeatShiftLeft)
        CASE_FOR(BeatShiftRight)
        CASE_FOR(BarShiftLeft)
        CASE_FOR(BarShiftRight)
        CASE_FOR(KeyShiftUp)
        CASE_FOR(KeyShiftDown)
        CASE_FOR(OctaveShiftUp)
        CASE_FOR(OctaveShiftDown)
        CASE_FOR(CleanupOverlaps)
        CASE_FOR(InvertChordUp)
        CASE_FOR(InvertChordDown)
        CASE_FOR(ShowArpeggiatorsPanel)
        CASE_FOR(ShowVolumePanel)
        CASE_FOR(TweakVolumeRandom)
        CASE_FOR(TweakVolumeFadeOut)
        CASE_FOR(VersionControlToggleQuickStash)
        CASE_FOR(VersionControlPush)
        CASE_FOR(VersionControlPull)
        CASE_FOR(VersionControlForcePull)
        CASE_FOR(VersionControlResetAll)
        CASE_FOR(VersionControlCommitAll)
        CASE_FOR(VersionControlSelectAll)
        CASE_FOR(VersionControlSelectNone)
        CASE_FOR(VersionControlResetSelected)
        CASE_FOR(VersionControlCommitSelected)
        CASE_FOR(VersionControlCheckout)
        default:
            return 0;
    };
}

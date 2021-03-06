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
#include "InternalClipboard.h"
#include "SerializationKeys.h"
#include "ClipboardOwner.h"
#include "XmlSerializer.h"
#include "App.h"

// todo multiple clipboards?

void Clipboard::copy(const ClipboardOwner &owner, bool mirrorToSystemClipboard /*= false*/)
{
    App::Helio().getClipboard()->copyFrom(owner, mirrorToSystemClipboard);
}

void Clipboard::paste(ClipboardOwner &owner)
{
    App::Helio().getClipboard()->pasteTo(owner);
}

ValueTree Clipboard::getCurrentContent()
{
    return App::Helio().getClipboard()->clipboard;
}

String Clipboard::getCurrentContentAsString()
{
    if (Clipboard::getCurrentContent().isValid())
    {
        String text;
        static XmlSerializer serializer;
        serializer.saveToString(text, Clipboard::getCurrentContent());
        return text;
    }
    
    return {};
}

void Clipboard::copyFrom(const ClipboardOwner &owner, bool mirrorToSystemClipboard /*= false*/)
{
    this->clipboard = owner.clipboardCopy();

    if (mirrorToSystemClipboard)
    {
        SystemClipboard::copyTextToClipboard(this->getCurrentContentAsString());
    }
}

void Clipboard::pasteTo(ClipboardOwner &owner)
{
    if (this->clipboard.isValid())
    {
        owner.clipboardPaste(this->clipboard);
    }
}

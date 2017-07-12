/*
===========================================================================

Daemon GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.

This file is part of the Daemon GPL Source Code (Daemon Source Code).

Daemon Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Daemon Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Daemon Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Daemon Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following the
terms and conditions of the GNU General Public License which accompanied the Daemon
Source Code.  If not, please request a copy in writing from id Software at the address
below.

If you have questions concerning this license or the applicable additional terms, you
may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville,
Maryland 20850 USA.

===========================================================================
*/

#ifndef ENGINE_CLIENT_KEYS_H_
#define ENGINE_CLIENT_KEYS_H_

#include "common/Common.h"

#include "keycodes.h"
#include "key_identification.h"

#include "framework/ConsoleField.h"
#include "qcommon/q_unicode.h"

#define MAX_TEAMS 4
#define DEFAULT_BINDING 0

struct qkey_t
{
    bool down;
    int      repeats; // if > 1, it is autorepeating
    Util::optional<std::string> binding[ MAX_TEAMS ];
};


extern bool key_overstrikeMode;
extern std::unordered_map<Keyboard::Key, qkey_t, Keyboard::Key::hash> keys;

void            Field_KeyDownEvent(Util::LineEditData& edit, Keyboard::Key key );
void            Field_CharEvent(Util::LineEditData& edit, int c );
void            Field_Draw(const Util::LineEditData& edit, int x, int y,
                           bool showCursor, bool noColorEscape, float alpha );

extern Console::Field  g_consoleField;

void            Key_ClearStates();

namespace Keyboard {

void WriteBindings(fileHandle_t f);
void SetBinding(Key key, int team, std::string binding);
Util::optional<std::string> GetBinding(Key key, int team);

bool IsDown(Key key);
bool AnyKeyDown();

void SetTeam(int newTeam);
int GetTeam();

}

#endif // ENGINE_CLIENT_KEYS_H_

/*
 * refind/menu.h
 * menu functions header file
 *
 * Copyright (c) 2006-2009 Christoph Pfisterer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *  * Neither the name of Christoph Pfisterer nor the names of the
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Modifications copyright (c) 2012 Roderick W. Smith
 * 
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), a copy of which must be distributed
 * with this source code or binaries made from it.
 * 
 */

#ifndef __REFIND_MENU_H_
#define __REFIND_MENU_H_

#ifdef __MAKEWITH_GNUEFI
#include "efi.h"
#include "efilib.h"
#else
#include "../include/tiano_includes.h"
#endif
#include "global.h"

#include "libeg.h"

#include "pointer.h"

//
// menu module
//

#define MENU_EXIT_ENTER   (1)
#define MENU_EXIT_ESCAPE  (2)
#define MENU_EXIT_DETAILS (3)
#define MENU_EXIT_TIMEOUT (4)
#define MENU_EXIT_EJECT   (5)
#define MENU_EXIT_HIDE    (6)

#define TAG_RETURN       (99)

// scrolling definitions

typedef struct {
   INTN CurrentSelection, PreviousSelection, MaxIndex;
   INTN FirstVisible, LastVisible, MaxVisible;
   INTN FinalRow0, InitialRow1;
   INTN ScrollMode;
   BOOLEAN PaintAll, PaintSelection;
} SCROLL_STATE;

#define SCROLL_LINE_UP    (0)
#define SCROLL_LINE_DOWN  (1)
#define SCROLL_PAGE_UP    (2)
#define SCROLL_PAGE_DOWN  (3)
#define SCROLL_FIRST      (4)
#define SCROLL_LAST       (5)
#define SCROLL_NONE       (6)
#define SCROLL_LINE_RIGHT (7)
#define SCROLL_LINE_LEFT  (8)

#define SCROLL_MODE_TEXT  (0) /* Used in text mode & for GUI submenus */
#define SCROLL_MODE_ICONS (1) /* Used for main GUI menu */

#define POINTER_NO_ITEM     (-1)
#define POINTER_LEFT_ARROW  (-2)
#define POINTER_RIGHT_ARROW (-3)

#define INPUT_KEY         (0)
#define INPUT_POINTER     (1)
#define INPUT_TIMEOUT     (2)
#define INPUT_TIMER_ERROR (3)

// Maximum length of a text string in certain menus
#define MAX_LINE_LENGTH 65

struct _refit_menu_screen;

typedef VOID (*MENU_STYLE_FUNC)(IN REFIT_MENU_SCREEN *Screen, IN SCROLL_STATE *State, IN UINTN Function, IN CHAR16 *ParamText);

VOID AddMenuInfoLine(IN REFIT_MENU_SCREEN *Screen, IN CHAR16 *InfoLine);
VOID AddMenuEntry(IN REFIT_MENU_SCREEN *Screen, IN REFIT_MENU_ENTRY *Entry);
UINTN ComputeRow0PosY(VOID);
VOID MainMenuStyle(IN REFIT_MENU_SCREEN *Screen, IN SCROLL_STATE *State, IN UINTN Function, IN CHAR16 *ParamText);
UINTN RunMenu(IN REFIT_MENU_SCREEN *Screen, OUT REFIT_MENU_ENTRY **ChosenEntry);
VOID DisplaySimpleMessage(CHAR16 *Title, CHAR16 *Message);
VOID TextMenuStyle(IN REFIT_MENU_SCREEN *Screen,
                   IN SCROLL_STATE *State,
                   IN UINTN Function,
                   IN CHAR16 *ParamText);
VOID GraphicsMenuStyle(IN REFIT_MENU_SCREEN *Screen,
                       IN SCROLL_STATE *State,
                       IN UINTN Function,
                       IN CHAR16 *ParamText);
UINTN RunGenericMenu(IN REFIT_MENU_SCREEN *Screen,
                     IN MENU_STYLE_FUNC StyleFunc,
                     IN OUT INTN *DefaultEntryIndex,
                     OUT REFIT_MENU_ENTRY **ChosenEntry);
VOID ManageHiddenTags(VOID);
CHAR16* ReadHiddenTags(CHAR16 *VarName);
UINTN RunMainMenu(IN REFIT_MENU_SCREEN *Screen, IN CHAR16** DefaultSelection, OUT REFIT_MENU_ENTRY **ChosenEntry);
UINTN FindMainMenuItem(IN REFIT_MENU_SCREEN *Screen, IN SCROLL_STATE *State, IN UINTN PosX, IN UINTN PosY);
VOID GenerateWaitList();
UINTN WaitForInput(IN UINTN Timeout);

#endif

/* EOF */

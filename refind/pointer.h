/*
 * refind/pointer.h
 * pointer device functions header file
 *
 * Copyright (c) 2018 CJ Vaughter
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __REFIND_POINTERDEVICE_H_
#define __REFIND_POINTERDEVICE_H_

#ifdef __MAKEWITH_GNUEFI
#include "efi.h"
#include "efilib.h"
#else
#include "../include/tiano_includes.h"
#endif

#ifndef _EFI_POINT_H
#include "../EfiLib/AbsolutePointer.h"
#endif

typedef struct PointerStateStruct {
    UINTN X, Y;
    BOOLEAN Press;
    BOOLEAN Holding;
} POINTER_STATE;

VOID pdInitialize();
VOID pdCleanup();
BOOLEAN pdAvailable();
UINTN pdCount();
EFI_EVENT pdWaitEvent(IN UINTN Index);
EFI_STATUS pdUpdateState();
POINTER_STATE pdGetState();

VOID pdDraw();
VOID pdClear();

#endif

/* EOF */

/*
 * BootMaster/pointer.h
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
/*
 * Modified for RefindPlus
 * Copyright (c) 2020 - 2025 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#ifndef __REFINDPLUS_POINTERDEVICE_H_
#define __REFINDPLUS_POINTERDEVICE_H_

#ifdef __MAKEWITH_GNUEFI
#include "efi.h"
#include "efilib.h"
#else
#include "../include/tiano_includes.h"
#endif

#ifndef _EFI_POINT_H
#include "../EfiLib/AbsolutePointer.h"
#endif
extern BOOLEAN gSuppressPointerDraw;
typedef struct PointerStateStruct {
    UINTN         X;
    UINTN         Y;
    BOOLEAN   Press;
    BOOLEAN Holding;
} POINTER_STATE;

#ifdef  INT32_MIN
#undef  INT32_MIN
#endif
#define INT32_MIN    ((INT32) 0x80000000)         // -2,147,483,648

#ifdef  INT32_MAX
#undef  INT32_MAX
#endif
#define INT32_MAX    ((INT32) 0x7FFFFFFF)         //  2,147,483,647

#ifdef  UINTN_MIN
#undef  UINTN_MIN
#endif
#define UINTN_MIN    ((UINTN) 0)                  //  Always 0

#ifdef  UINTN_MAX
#undef  UINTN_MAX
#endif
#if defined(EFI32)
#define UINTN_MAX    ((UINTN) 0xFFFFFFFF)         //  4,294,967,295
#else
#define UINTN_MAX    ((UINTN) 0xFFFFFFFFFFFFFFFF) //  18,446,744,073,709,551,615
#endif


VOID pdDraw (VOID);
VOID pdClear (VOID);
VOID pdCleanup (VOID);
VOID pdInitialize (VOID);

UINTN pdCount (VOID);

BOOLEAN pdAvailable (VOID);

EFI_EVENT pdWaitEvent(IN UINTN Index);

EFI_STATUS pdUpdateState (VOID);

POINTER_STATE pdGetState (VOID);

#endif

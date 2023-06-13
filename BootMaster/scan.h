/*
 * BootMaster/scan.h
 * Headers related to scanning for boot loaders
 *
 * Copyright (c) 2006-2010 Christoph Pfisterer
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
 * Modifications copyright (c) 2012-2020 Roderick W. Smith
 *
 * Modifications distributed under the terms of the GNU General Public
 * License (GPL) version 3 (GPLv3), or (at your option) any later version.
 *
 */
/*
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

#ifndef __SCAN_H_
#define __SCAN_H_

LOADER_ENTRY * InitializeLoaderEntry (IN LOADER_ENTRY *Entry);

REFIT_MENU_ENTRY * CopyMenuEntry (REFIT_MENU_ENTRY *Entry);

REFIT_MENU_SCREEN * CopyMenuScreen (REFIT_MENU_SCREEN *Entry);
REFIT_MENU_SCREEN * InitializeSubScreen (IN LOADER_ENTRY *Entry);

VOID ScanForTools (VOID);
VOID ScanForBootloaders (VOID);
VOID SetLoaderDefaults (
    IN LOADER_ENTRY *Entry,
    IN CHAR16       *LoaderPath,
    IN REFIT_VOLUME *Volume
);
VOID GenerateSubScreen (
    IN OUT LOADER_ENTRY *Entry,
    IN     REFIT_VOLUME *Volume,
    IN     BOOLEAN       GenerateReturn
);

CHAR16 * SetVolJoin (IN CHAR16 *InstanceName);
CHAR16 * SetVolKind (
    IN CHAR16 *InstanceName,
    IN CHAR16 *VolumeName,
    IN UINT32  VolumeFSType
);
CHAR16 * SetVolFlag (
    IN CHAR16 *InstanceName,
    IN CHAR16 *VolumeName
);
CHAR16 * SetVolType (
    IN CHAR16 *InstanceName OPTIONAL,
    IN CHAR16 *VolumeName,
    IN UINT32  VolumeFSType
);
CHAR16 * GetVolumeGroupName (
    IN CHAR16 *LoaderPath,
    IN REFIT_VOLUME *Volume
);

BOOLEAN ShouldScan (REFIT_VOLUME *Volume, CHAR16 *Path);

#endif

/* EOF */

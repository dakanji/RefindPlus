/*
 * Additional functions to support LodePNG for use in RefindPlus
 *
 * copyright (c) 2013 by by Roderick W. Smith, and distributed
 * under the terms of the GNU GPL v3, or (at your option) any
 * later version.
 *
 * See http://lodev.org/lodepng/ for the original LodePNG.
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
/*
 * Modified for RefindPlus
 * Copyright (c) 2021-2024 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */

#include "lodepng.h"
#include "../BootMaster/rp_funcs.h"
#include "../BootMaster/screenmgt.h"
#include "../include/refit_call_wrapper.h"

typedef struct _lode_color {
   UINT8 red;
   UINT8 green;
   UINT8 blue;
   UINT8 alpha;
} lode_color;


static
size_t report_size (
    VOID *ptr
) {
    if (!ptr) {
        return 0;
    }

    return * (((size_t *) ptr) - 1);
} // static size_t report_size()

// EFI's equivalent of realloc requires the original buffer's size as an
// input parameter, which the standard libc realloc does not require. Thus,
// I've modified lodepng_refit_malloc() to allocate more memory to store this data,
// and lodepng_refit_realloc() can then read it when required. Because the size is
// stored at the start of the allocated area, these functions are NOT
// interchangeable with the standard EFI functions; memory allocated via
// lodepng_refit_malloc() should be freed via lodepng_refit_free(), and myfree() should
// NOT be used with memory allocated via AllocatePool() or AllocateZeroPool()!
VOID * lodepng_refit_malloc (
    size_t size
) {
    VOID *ptr;

    ptr = AllocateZeroPool(size + sizeof (size_t));
    if (!ptr) {
        return NULL;
    }

    *(size_t *) ptr = size;

    return ((size_t *) ptr) + 1;
} // VOID * lodepng_refit_malloc()

VOID lodepng_refit_free (
    VOID *ptr
) {
    if (ptr) {
        ptr = (VOID *) (((size_t *) ptr) - 1);
        MY_FREE_POOL(ptr);
    }
} // VOID lodepng_refit_free()

VOID * lodepng_refit_realloc (
    VOID   *ptr,
    size_t  new_size
) {
    size_t *new_pool;
    size_t  old_size;

    new_pool = lodepng_refit_malloc (new_size);
    if (new_pool && ptr) {
        old_size = report_size (ptr);
        REFIT_CALL_3_WRAPPER(
            gBS->CopyMem, new_pool,
            ptr, (old_size < new_size) ? old_size : new_size
        );
    }

    return new_pool;
} // VOID * lodepng_refit_realloc()

// DA-TAG: Investigate This
//         Why does this live here?
//         Only used in nanojpeg.c
VOID * MyMemSet (
    VOID   *s,
    int     c,
    size_t  n
) {
    REFIT_CALL_3_WRAPPER(
        gBS->SetMem, s,
        c, n
    );

    return s;
} // VOID * MyMemSet()

// DA-TAG: Investigate This
//         Why does this live here?
//         Only used in nanojpeg.c
VOID * MyMemCpy (
    VOID   *__restrict __dest,
    VOID   *__restrict __src,
    size_t  __n
) {
    REFIT_CALL_3_WRAPPER(
        gBS->CopyMem, __dest,
        __src, __n
    );

    return __dest;
} // VOID * MyMemCpy

EG_IMAGE * egDecodePNG (
    IN UINT8   *FileData,
    IN UINTN    FileDataLength,
    IN UINTN    IconSize,
    IN BOOLEAN  WantAlpha
) {
    unsigned    Error;
    unsigned    Width;
    unsigned    Height;
    lode_color *LodeData;
    EG_IMAGE   *NewImage;
    EG_PIXEL   *PixelData;
    UINTN       i;

    Error = lodepng_decode_memory (
        (unsigned char **) &PixelData,
        &Width,
        &Height,
        (unsigned char *) FileData,
        (size_t) FileDataLength,
        LCT_RGBA, 8
    );
    if (Error) {
        return NULL;
    }

    // Allocate image structure and buffer
    NewImage = egCreateImage (Width, Height, WantAlpha);
    if (NewImage == NULL) {
        return NULL;
    }

    if (NewImage->Width != Width || NewImage->Height != Height) {
        // Should never happen ... just being paranoid.
        MY_FREE_IMAGE(NewImage);

        return NULL;
    }

    LodeData = (lode_color *) PixelData;

    // UEFI and LodePNG use different ordering of RGB values in
    // their pixel data representations, so we must adjust them.
    for (i = 0; i < (NewImage->Height * NewImage->Width); i++) {
        NewImage->PixelData[i].r = LodeData[i].red;
        NewImage->PixelData[i].g = LodeData[i].green;
        NewImage->PixelData[i].b = LodeData[i].blue;

        if (WantAlpha) {
            NewImage->PixelData[i].a = LodeData[i].alpha;
        }
    }
    lodepng_refit_free (PixelData);

    return NewImage;
} // EG_IMAGE * egDecodePNG()

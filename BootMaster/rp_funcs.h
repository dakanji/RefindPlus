/*
 * BootMaster/rp_funcs.h
 *
 * Copyright (c) 2021-2022 Dayo Akanji (sf.net/u/dakanji/profile)
 * Licensed under the MIT License
 */

#ifndef _RP_FUNCS_H
#define _RP_FUNCS_H

// Temp Dereference to NULL for Debugging
#define MY_FAKE_FREE(Pointer)                 \
    do {                                      \
        if (Pointer) {                        \
            Pointer = NULL;                   \
        }                                     \
    } while (0)

// Dereference to NULL When Actually Needed
#define MY_SOFT_FREE(Pointer)                 \
    do {                                      \
        if (Pointer) {                        \
            MY_FAKE_FREE(Pointer);            \
        }                                     \
    } while (0)

#define MY_FREE_POOL(Pointer)                 \
    do {                                      \
        if (Pointer) {                        \
            FreePool (Pointer);               \
            Pointer = NULL;                   \
        }                                     \
    } while (0)

#define MY_FREE_IMAGE(Image)                  \
    do {                                      \
        if (Image) {                          \
            MY_FREE_POOL(Image->PixelData);   \
            MY_FREE_POOL(Image);              \
        }                                     \
    } while (0)

#define MY_FREE_FILE(File)                    \
    do {                                      \
        if (File) {                           \
            MY_FREE_POOL(File->Buffer);       \
            MY_FREE_POOL(File);               \
        }                                     \
    } while (0)

#endif

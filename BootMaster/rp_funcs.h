/*
 * BootMaster/rp_funcs.h
 *
 * Copyright (c) 2021 Dayo Akanji (sf.net/u/dakanji/profile)
 * Licensed under the MIT License
 */

#ifndef _RP_FUNCS_H
#define _RP_FUNCS_H

#define MY_FREE_POOL(Pointer)                 \
    do {                                      \
        if (Pointer) {                        \
            FreePool (Pointer);               \
            Pointer = NULL;                   \
        }                                     \
    } while (FALSE)

#define MY_FREE_IMAGE(Image)                  \
    do {                                      \
        if (Image) {                          \
            MY_FREE_POOL(Image->PixelData);   \
            MY_FREE_POOL(Image);              \
        }                                     \
    } while (FALSE)

#define MY_FREE_FILE(File)                    \
    do {                                      \
        if (File) {                           \
            MY_FREE_POOL(File->Buffer);       \
            MY_FREE_POOL(File);               \
        }                                     \
    } while (FALSE)

#endif

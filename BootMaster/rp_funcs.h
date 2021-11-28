/*
 * BootMaster/rp_funcs.h
 *
 * Copyright (c) 2021 Dayo Akanji (sf.net/u/dakanji/profile)
 * Licensed under the MIT License
 */

#ifndef _MY_FREE_POOL_H
#define _MY_FREE_POOL_H
#define MY_FREE_POOL(Pointer)     \
    do {                          \
        if (Pointer) {            \
            FreePool (Pointer);   \
            Pointer = NULL;       \
        }                         \
    } while (FALSE)
#endif

#ifndef _MY_FREE_IMAGE_H
#define _MY_FREE_IMAGE_H
#define MY_FREE_IMAGE(Image)                  \
    do {                                      \
        if (Image) {                          \
            MY_FREE_POOL(Image->PixelData);   \
            MY_FREE_POOL(Image);              \
        }                                     \
    } while (FALSE)
#endif

/*
 * BootMaster/my_free_pool.h
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

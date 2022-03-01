/*
 * BootMaster/rp_funcs.h
 *
 * Copyright (c) 2021-2022 Dayo Akanji (sf.net/u/dakanji/profile)
 * Licensed under the MIT License
 */

#ifndef _RP_FUNCS_H
#define _RP_FUNCS_H

// Control NativeLogger ... Especially Nested Instances
#define MY_NATIVELOGGER_SET if (!NativeLogger) ForceNative = NativeLogger = TRUE
#define MY_NATIVELOGGER_OFF if (ForceNative) ForceNative = NativeLogger = FALSE

// Control MuteLogger ... Especially Nested Instances
#define MY_MUTELOGGER_SET if (!MuteLogger) CheckMute = MuteLogger = TRUE
#define MY_MUTELOGGER_OFF if (CheckMute) CheckMute = MuteLogger = FALSE

// Control HybridLogger ... Especially Nested Instances
#define MY_HYBRIDLOGGER_SET                   \
    do {                                      \
        if (NativeLogger) {                   \
            HybridLogger =  TRUE;             \
            NativeLogger = FALSE;             \
        }                                     \
    } while (0)
#define MY_HYBRIDLOGGER_OFF                   \
    do {                                      \
        if (HybridLogger) {                   \
            NativeLogger =  TRUE;             \
            HybridLogger = FALSE;             \
        }                                     \
    } while (0)

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

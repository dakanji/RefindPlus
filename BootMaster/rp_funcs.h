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
        if (Pointer != NULL) {                \
            Pointer = NULL;                   \
        }                                     \
    } while (0)

// Dereference to NULL When Actually Needed
#define MY_SOFT_FREE(Pointer)                 \
    do {                                      \
        if (Pointer != NULL) {                \
            Pointer = NULL;                   \
        }                                     \
    } while (0)

#define MY_FREE_POOL(Pointer)                 \
    do {                                      \
        if (Pointer != NULL) {                \
            FreePool (Pointer);               \
            Pointer = NULL;                   \
        }                                     \
    } while (0)

#define MY_FREE_IMAGE(Image)                  \
    do {                                      \
        if (Image != NULL) {                  \
            if (Image->PixelData != NULL) {   \
                FreePool (Image->PixelData);  \
                Image->PixelData = NULL;      \
            }                                 \
            FreePool (Image);                 \
            Image = NULL;                     \
        }                                     \
    } while (0)

#define MY_FREE_FILE(File)                    \
    do {                                      \
        if (File != NULL) {                   \
            if (File->Buffer != NULL) {       \
                FreePool (File->Buffer);      \
                File->Buffer = NULL;          \
            }                                 \
            FreePool (File);                  \
            File = NULL;                      \
        }                                     \
    } while (0)

#endif

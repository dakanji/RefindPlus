/*
 * BootMaster/rp_funcs.h
 *
 * Copyright (c) 2021-2022 Dayo Akanji (sf.net/u/dakanji/profile)
 * Licensed under the MIT License
 */

#ifndef _RP_FUNCS_H
#define _RP_FUNCS_H

// Control NativeLogger ... Especially Nested Instances
#define MY_NATIVELOGGER_SET                   \
    do {                                      \
        ForceNative = FALSE;                  \
        if (!MuteLogger) {                    \
            ForceNative  = TRUE;              \
            NativeLogger = TRUE;              \
        }                                     \
    } while (0)
#define MY_NATIVELOGGER_OFF                   \
    do {                                      \
        if (ForceNative) {                    \
            NativeLogger = FALSE;             \
        }                                     \
    } while (0)

// Control MuteLogger ... Especially Nested Instances
#define MY_MUTELOGGER_SET                     \
    do {                                      \
        CheckMute = FALSE;                    \
        if (!MuteLogger) {                    \
            CheckMute  = TRUE;                \
            MuteLogger = TRUE;                \
        }                                     \
    } while (0)
#define MY_MUTELOGGER_OFF                     \
    do {                                      \
        if (CheckMute) {                      \
            MuteLogger = FALSE;               \
        }                                     \
    } while (0)

// Control HybridLogger ... Especially Nested Instances
#define MY_HYBRIDLOGGER_SET                   \
    do {                                      \
        HybridLogger = FALSE;                 \
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

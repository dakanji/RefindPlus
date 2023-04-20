/*
 * BootMaster/rp_funcs.h
 *
 * Copyright (c) 2021-2022 Dayo Akanji (sf.net/u/dakanji/profile)
 * Licensed under the MIT License
 */

#ifndef _RP_FUNCS_H
#define _RP_FUNCS_H

extern BOOLEAN gKernelStarted;

extern VOID RefitStall (
    UINTN StallLoops
);

// Control NativeLogger ... Especially Nested Instances
#define MY_NATIVELOGGER_SET                             \
    do {                                                \
        if (!gKernelStarted) {                          \
            ForceNative = FALSE;                        \
            if (!MuteLogger) {                          \
                ForceNative  = TRUE;                    \
                NativeLogger = TRUE;                    \
            }                                           \
        }                                               \
    } while (0)
#define MY_NATIVELOGGER_OFF                             \
    do {                                                \
        if (!gKernelStarted) {                          \
            if (ForceNative) {                          \
                NativeLogger = FALSE;                   \
            }                                           \
        }                                               \
    } while (0)

// Control MuteLogger ... Especially Nested Instances
#define MY_MUTELOGGER_SET                               \
    do {                                                \
        if (!gKernelStarted) {                          \
            CheckMute = FALSE;                          \
            if (!MuteLogger) {                          \
                CheckMute  = TRUE;                      \
                MuteLogger = TRUE;                      \
            }                                           \
        }                                               \
    } while (0)
#define MY_MUTELOGGER_OFF                               \
    do {                                                \
        if (!gKernelStarted) {                          \
            if (CheckMute) {                            \
                MuteLogger = FALSE;                     \
            }                                           \
        }                                               \
    } while (0)

// Control HybridLogger ... Especially Nested Instances
#define MY_HYBRIDLOGGER_SET                             \
    do {                                                \
        if (!gKernelStarted) {                          \
            HybridLogger = FALSE;                       \
            if (NativeLogger) {                         \
                HybridLogger =  TRUE;                   \
                NativeLogger = FALSE;                   \
            }                                           \
        }                                               \
    } while (0)
#define MY_HYBRIDLOGGER_OFF                             \
    do {                                                \
        if (!gKernelStarted) {                          \
            if (HybridLogger) {                         \
                NativeLogger =  TRUE;                   \
                HybridLogger = FALSE;                   \
            }                                           \
        }                                               \
    } while (0)

#define MY_FAKE_FREE(Pointer)                           \
    do {                                                \
        if (!gKernelStarted) {                          \
            if (Pointer != NULL) {                      \
                Pointer = NULL;                         \
            }                                           \
        }                                               \
    } while (0)

// Dereference to NULL When Actually Needed
#define MY_SOFT_FREE(Pointer)                           \
    do {                                                \
        if (!gKernelStarted) {                          \
            if (Pointer != NULL) {                      \
                Pointer = NULL;                         \
            }                                           \
        }                                               \
    } while (0)

#define MY_FREE_POOL(Pointer)                           \
    do {                                                \
        if (!gKernelStarted) {                          \
            if (Pointer != NULL) {                      \
                FreePool (Pointer);                     \
                Pointer = NULL;                         \
            }                                           \
        }                                               \
    } while (0)

#define MY_FREE_IMAGE(Image)                            \
    do {                                                \
        if (!gKernelStarted) {                          \
            if (Image != NULL) {                        \
                if (Image->PixelData != NULL) {         \
                    FreePool (Image->PixelData);        \
                    Image->PixelData = NULL;            \
                }                                       \
                FreePool (Image);                       \
                Image = NULL;                           \
            }                                           \
        }                                               \
    } while (0)

#define MY_FREE_FILE(File)                              \
    do {                                                \
        if (!gKernelStarted) {                          \
            if (File != NULL) {                         \
                if (File->Buffer != NULL) {             \
                    FreePool (File->Buffer);            \
                    File->Buffer = NULL;                \
                }                                       \
                FreePool (File);                        \
                File = NULL;                            \
            }                                           \
        }                                               \
    } while (0)

#endif

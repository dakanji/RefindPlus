/** @file
 * AmendSysTable.c
 * Amends the SystemTable to provide CreateEventEx and a UEFI 2.3 Revision Number
 *
 * Copyright (c) 2020-2022 Dayo Akanji (sf.net/u/dakanji/profile)
 * Portions Copyright (c) 2020 Joe van Tunen (joevt@shaw.ca)
 * Portions Copyright (c) 2004-2008 The Intel Corporation
 *
 * THIS PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
**/

EFI_STATUS AmendSysTable (VOID);

// Check Compile Type - START
#ifndef __MAKEWITH_TIANO // Compile Type = OTHER/GNU_EFI

/**
  @retval EFI_INCOMPATIBLE_VERSION  Not running on compatible TianoCore compiled version
**/
EFI_STATUS AmendSysTable (VOID) {
    // NOOP if not compiled using EDK II
    return EFI_INCOMPATIBLE_VERSION;
}

#else // Compile Type = TIANOCORE

#include "../BootMaster/global.h"
#include "../BootMaster/rp_funcs.h"
#include "../include/refit_call_wrapper.h"
#include "../../MdeModulePkg/Core/Dxe/DxeMain.h"
#include "../../MdeModulePkg/Core/Dxe/Event/Event.h"

extern BOOLEAN egInitUGADraw (BOOLEAN LogOutput);

BOOLEAN SetSysTab = FALSE;

#define EFI_FIELD_OFFSET(TYPE, Field) ((UINTN) (&(((TYPE *) 0)->Field)))
#define EFI_REVISION_MIN  EFI_2_00_SYSTEM_TABLE_REVISION
#define EFI_REVISION_MOD  EFI_2_30_SYSTEM_TABLE_REVISION

EFI_CPU_ARCH_PROTOCOL   *zCpu       = NULL;
EFI_SMM_BASE2_PROTOCOL  *zSmmBase2  = NULL;

EFI_RUNTIME_ARCH_PROTOCOL zRuntimeTemplate = {
    INITIALIZE_LIST_HEAD_VARIABLE (zRuntimeTemplate.ImageHead),
    INITIALIZE_LIST_HEAD_VARIABLE (zRuntimeTemplate.EventHead),
    sizeof (EFI_MEMORY_DESCRIPTOR) +
        sizeof (UINT64) -
        (sizeof (EFI_MEMORY_DESCRIPTOR) % sizeof (UINT64)),
    EFI_MEMORY_DESCRIPTOR_VERSION, 0,
    NULL, NULL, FALSE, FALSE
};

UINTN                       zEventPending      = 0;
EFI_TPL                     zEfiCurrentTpl     = TPL_APPLICATION;
EFI_LOCK                    zEventQueueLock    = EFI_INITIALIZE_LOCK_VARIABLE (TPL_HIGH_LEVEL);
EFI_RUNTIME_ARCH_PROTOCOL  *zRuntime           = &zRuntimeTemplate;
LIST_ENTRY                  zEventSignalQueue  = INITIALIZE_LIST_HEAD_VARIABLE (zEventSignalQueue);
LIST_ENTRY                  zEventQueue[TPL_HIGH_LEVEL + 1];

UINT32 rEventTable[] = {
    EVT_TIMER|EVT_NOTIFY_SIGNAL,
    EVT_TIMER, EVT_NOTIFY_WAIT, EVT_NOTIFY_SIGNAL,
    EVT_SIGNAL_EXIT_BOOT_SERVICES,
    EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE, 0x00000000,
    EVT_TIMER|EVT_NOTIFY_WAIT
};

EFI_TPL EFIAPI OurRaiseTpl (IN EFI_TPL  NewTpl);
VOID    EFIAPI OurRestoreTpl (IN EFI_TPL  NewTpl);
VOID           OurSetInterruptState (IN BOOLEAN  Enable);
VOID           OurDispatchEventNotifies (IN EFI_TPL  Priority);
VOID           OurAcquireLock (IN EFI_LOCK  *Lock);
VOID           OurReleaseLock (IN EFI_LOCK  *Lock);
EFI_STATUS     OurCreateEventEx (
    UINT32             Type,
    EFI_TPL            NotifyTpl,
    EFI_EVENT_NOTIFY   NotifyFunction,
    const void        *NotifyContext,
    const EFI_GUID    *EventGroup,
    EFI_EVENT         *Event
);

/**
  Set Interrupt State.
  @param Enable: The state of enable or disable interrupt
**/
VOID OurSetInterruptState (
    IN BOOLEAN  Enable
) {
    EFI_STATUS  Status;
    BOOLEAN     InSmm;

    if (zCpu == NULL) {
        return;
    }

    if (!Enable) {
        zCpu->DisableInterrupt (zCpu);
        return;
    }

    if (zSmmBase2 == NULL) {
        zCpu->EnableInterrupt (zCpu);
        return;
    }

    Status = zSmmBase2->InSmm (zSmmBase2, &InSmm);
    if (!EFI_ERROR(Status) && !InSmm) {
        zCpu->EnableInterrupt (zCpu);
    }
} // VOID OurSetInterruptState()

/**
  Dispatches pending events.
  @param Priority: Task priority level of event notifications to dispatch
**/
VOID OurDispatchEventNotifies (
    IN EFI_TPL  Priority
) {
    IEVENT        *Event;
    LIST_ENTRY    *Head;

    OurAcquireLock (&zEventQueueLock);
    ASSERT (zEventQueueLock.OwnerTpl == Priority);
    Head = &zEventQueue[Priority];

    // Dispatch pending notifications
    while (!IsListEmpty (Head)) {
        Event = REFIT_CALL_4_WRAPPER(
            CR, Head->ForwardLink,
            IEVENT, NotifyLink, EVENT_SIGNATURE
        );
        REFIT_CALL_1_WRAPPER(RemoveEntryList, &Event->NotifyLink);
        Event->NotifyLink.ForwardLink = NULL;

        // Only clear the SIGNAL status if it is a SIGNAL type event.
        // WAIT type events are only cleared in CheckEvent()
        if ((Event->Type & EVT_NOTIFY_SIGNAL) != 0) {
            Event->SignalCount = 0;
        }

        OurReleaseLock (&zEventQueueLock);

        // Notify this event
        ASSERT (Event->NotifyFunction != NULL);

        REFIT_CALL_2_WRAPPER(Event->NotifyFunction, Event, Event->NotifyContext);

        // Check for next pending event
        OurAcquireLock (&zEventQueueLock);
    } // while

    zEventPending &= ~((UINTN) (1) << Priority);
    OurReleaseLock (&zEventQueueLock);
} // VOID OurDispatchEventNotifies()

/**
  Raise the task priority level to the new level.
  High level is implemented by disabling processor interrupts.
  @param  NewTpl:  New task priority level
  @return The previous task priority level
**/
EFI_TPL EFIAPI OurRaiseTpl (
    IN EFI_TPL  NewTpl
) {
    EFI_TPL     OldTpl;

    OldTpl = zEfiCurrentTpl;
    if (OldTpl > NewTpl) {
        #if REFIT_DEBUG > 0
        LOG_MSG(
            "FATAL ERROR: RaiseTpl with OldTpl (0x%x) > NewTpl (0x%x)",
            OldTpl,
            NewTpl
        );
        LOG_MSG("\n\n");
        #endif

        ASSERT (FALSE);
    }
    ASSERT (VALID_TPL (NewTpl));

    // Disable interrupts if raising to high level
    if (NewTpl >= TPL_HIGH_LEVEL  &&  OldTpl < TPL_HIGH_LEVEL) {
        OurSetInterruptState (FALSE);
    }

    // Set the new value
    zEfiCurrentTpl = NewTpl;

    return OldTpl;
} // EFI_TPL EFIAPI OurRaiseTpl()

/**
  Lowers the task priority to the previous value.   If the new
  priority unmasks events at a higher priority, they are dispatched.
  @param  NewTpl:  New, lower, task priority
**/
VOID EFIAPI OurRestoreTpl (
    IN EFI_TPL NewTpl
) {
    EFI_TPL    OldTpl;
    EFI_TPL    PendingTpl;

    OldTpl = zEfiCurrentTpl;
    if (NewTpl > OldTpl) {
        #if REFIT_DEBUG > 0
        LOG_MSG(
            "FATAL ERROR: RestoreTpl with NewTpl (0x%x) > OldTpl (0x%x)",
            NewTpl,
            OldTpl
        );
        LOG_MSG("\n");
        #endif

        ASSERT (FALSE);
    }
    ASSERT (VALID_TPL (NewTpl));

    // Ensure interrupts are enabled if lowering below HIGH_LEVEL
    if (OldTpl >= TPL_HIGH_LEVEL  &&  NewTpl < TPL_HIGH_LEVEL) {
        zEfiCurrentTpl = TPL_HIGH_LEVEL;
    }

    // Dispatch pending events
    while (zEventPending != 0) {
        PendingTpl = (UINTN) HighBitSet64 (zEventPending);
        if (PendingTpl <= NewTpl) {
            break;
        }

        zEfiCurrentTpl = PendingTpl;
        if (zEfiCurrentTpl < TPL_HIGH_LEVEL) {
            OurSetInterruptState (TRUE);
        }
        OurDispatchEventNotifies (zEfiCurrentTpl);
    } // while

    // Set new value
    zEfiCurrentTpl = NewTpl;

    // Ensure interrupts are enabled if lowering below HIGH_LEVEL
    if (zEfiCurrentTpl < TPL_HIGH_LEVEL) {
        OurSetInterruptState (TRUE);
    }
} // VOID EFIAPI OurRestoreTpl()

/**
  Raising to the task priority level of the mutual exclusion
  lock, and then acquires ownership of the lock.
  @param  Lock:  The lock to acquire
  @return Lock owned
**/
VOID OurAcquireLock (
    IN EFI_LOCK  *Lock
) {
    ASSERT (Lock != NULL);
    ASSERT (Lock->Lock == EfiLockReleased);

    Lock->OwnerTpl = OurRaiseTpl (Lock->Tpl);
    Lock->Lock     = EfiLockAcquired;
} // VOID OurAcquireLock()

/**
  Releases ownership of the mutual exclusion lock, and
  restores the previous task priority level.
  @param  Lock:  The lock to release
  @return Lock unowned
**/
VOID OurReleaseLock (
    IN EFI_LOCK  *Lock
) {
    EFI_TPL Tpl;

    ASSERT (Lock != NULL);
    ASSERT (Lock->Lock == EfiLockAcquired);

    Tpl        = Lock->OwnerTpl;
    Lock->Lock = EfiLockReleased;

    OurRestoreTpl (Tpl);
} // VOID OurReleaseLock()


EFI_STATUS OurCreateEventEx (
    IN        UINT32             Type,
    IN        EFI_TPL            NotifyTpl,
    IN        EFI_EVENT_NOTIFY   NotifyFunction OPTIONAL,
    IN  const VOID              *NotifyContext  OPTIONAL,
    IN  const EFI_GUID          *EventGroup     OPTIONAL,
    OUT       EFI_EVENT         *Event
) {
    EFI_STATUS         Status;
    IEVENT            *IEvent;
    INTN               Index;

    Status = EFI_SUCCESS;

    // Check for invalid NotifyTpl if a notify event type
    if ((Type & (EVT_NOTIFY_WAIT | EVT_NOTIFY_SIGNAL)) != 0) {
        if (NotifyTpl != TPL_APPLICATION &&
            NotifyTpl != TPL_CALLBACK    &&
            NotifyTpl != TPL_NOTIFY
        ) {
            return EFI_INVALID_PARAMETER;
        }
    }

    if (Event == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    // Ensure no reserved flags are set
    Status = EFI_INVALID_PARAMETER;
    for (Index = 0; Index < (sizeof (rEventTable) / sizeof (UINT32)); Index++) {
        if (Type == rEventTable[Index]) {
            Status = EFI_SUCCESS;
            break;
        }
    }
    if (EFI_ERROR(Status)) {
        return EFI_INVALID_PARAMETER;
    }

    // Convert Event type for pre-defined Event groups
    if (EventGroup != NULL) {
        // EVT_SIGNAL_EXIT_BOOT_SERVICES and EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE
        // are invalid For EventGroup
        if ((Type == EVT_SIGNAL_EXIT_BOOT_SERVICES) ||
            (Type == EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE)
        ) {
            return EFI_INVALID_PARAMETER;
        }

        if (CompareGuid (EventGroup, &gEfiEventExitBootServicesGuid)) {
            Type = EVT_SIGNAL_EXIT_BOOT_SERVICES;
        }
        else if (CompareGuid (EventGroup, &gEfiEventVirtualAddressChangeGuid)) {
            Type = EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE;
        }
    }
    else {
        // Convert EFI 1.x Events to their UEFI 2.x CreateEventEx mapping
        if (Type == EVT_SIGNAL_EXIT_BOOT_SERVICES) {
            EventGroup = &gEfiEventExitBootServicesGuid;
        }
        else if (Type == EVT_SIGNAL_VIRTUAL_ADDRESS_CHANGE) {
            EventGroup = &gEfiEventVirtualAddressChangeGuid;
        }
    }

    // Check parameters if notify event type
    if ((Type & (EVT_NOTIFY_WAIT | EVT_NOTIFY_SIGNAL)) != 0) {
        // Check for invalid NotifyFunction or NotifyTpl
        if ((NotifyFunction == NULL)       ||
            (NotifyTpl <= TPL_APPLICATION) ||
            (NotifyTpl >= TPL_HIGH_LEVEL)
        ) {
            return EFI_INVALID_PARAMETER;
        }
    }
    else {
        // No notification needed ... Zero out ignored values
        NotifyTpl      = 0;
        NotifyFunction = NULL;
        NotifyContext  = NULL;
    }

    // Allocate and initialize a new event structure.
    IEvent = ((Type & EVT_RUNTIME) != 0)
        ? AllocateRuntimeZeroPool (sizeof (IEVENT))
        : AllocateZeroPool (sizeof (IEVENT));

    if (IEvent == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    IEvent->Signature      = EVENT_SIGNATURE;
    IEvent->Type           = Type;
    IEvent->NotifyTpl      = NotifyTpl;
    IEvent->NotifyFunction = NotifyFunction;
    IEvent->NotifyContext  = (VOID *) NotifyContext;

    if (EventGroup != NULL) {
        CopyGuid (&IEvent->EventGroup, EventGroup);
        IEvent->ExFlag |= EVT_EXFLAG_EVENT_GROUP;
    }

    *Event = IEvent;

    if ((Type & EVT_RUNTIME) != 0) {
        // Keep a list of all RT events so we can tell the RT AP.
        IEvent->RuntimeData.Type           = Type;
        IEvent->RuntimeData.NotifyTpl      = NotifyTpl;
        IEvent->RuntimeData.NotifyFunction = NotifyFunction;
        IEvent->RuntimeData.NotifyContext  = (VOID *) NotifyContext;
        IEvent->RuntimeData.Event          = (EFI_EVENT *) IEvent;
        InsertTailList (&zRuntime->EventHead, &IEvent->RuntimeData.Link);
    }

    OurAcquireLock (&zEventQueueLock);

    if ((Type & EVT_NOTIFY_SIGNAL) != 0x00000000) {
        // The Event's NotifyFunction must be queued whenever the event is signaled
        InsertHeadList (&zEventSignalQueue, &IEvent->SignalLink);
    }

    OurReleaseLock (&zEventQueueLock);

    // Done
    return EFI_SUCCESS;
} // EFI_STATUS OurCreateEventEx()

/**
  @retval EFI_SUCCESS               The command completed successfully.
  @retval EFI_OUT_OF_RESOURCES      Out of memory.
  @retval EFI_ALREADY_STARTED       Already on UEFI 2.0 or later.
  @retval EFI_PROTOCOL_ERROR        Unexpected Field Offset.
  @retval EFI_NOT_STARTED           Aborted Process ... PreferUGA
**/
EFI_STATUS AmendSysTable (VOID) {
    EFI_BOOT_SERVICES *uBS;

    /* Check EFI Revision */
    if (gBS->Hdr.Revision >= EFI_REVISION_MIN ||
        gRT->Hdr.Revision >= EFI_REVISION_MIN ||
        gST->Hdr.Revision >= EFI_REVISION_MIN
    ) {
        // Early Return
        return EFI_ALREADY_STARTED;
    }

    if (gBS->Hdr.HeaderSize > EFI_FIELD_OFFSET(EFI_BOOT_SERVICES, CreateEventEx)) {
        // Early Return
        return EFI_PROTOCOL_ERROR;
    }

    uBS = (EFI_BOOT_SERVICES *) AllocateCopyPool (sizeof (EFI_BOOT_SERVICES), gBS);
    if (uBS == NULL) {
        // Early Return
        return EFI_OUT_OF_RESOURCES;
    }

    /* Amend SystemTable */
    gST->BootServices    = gBS;
    gST->RuntimeServices = gRT;
    gST->Hdr.HeaderSize  = sizeof (EFI_SYSTEM_TABLE);
    gST->Hdr.Revision    = EFI_REVISION_MOD;
    gST->Hdr.CRC32       = 0;
    REFIT_CALL_3_WRAPPER(
        gBS->CalculateCrc32, gST,
        gST->Hdr.HeaderSize, &gST->Hdr.CRC32
    );

    /* Amend BootServices */
    uBS->CreateEventEx   = OurCreateEventEx;
    uBS->Hdr.HeaderSize  = sizeof (EFI_BOOT_SERVICES);
    uBS->Hdr.Revision    = EFI_REVISION_MOD;
    uBS->Hdr.CRC32       = 0;
    REFIT_CALL_3_WRAPPER(
        uBS->CalculateCrc32, uBS,
        uBS->Hdr.HeaderSize, &uBS->Hdr.CRC32
    );
    gBS = uBS;

    /* Flag Amendment */
    SetSysTab = TRUE;

    return EFI_SUCCESS;
} // EFI_STATUS AmendSysTable()

#endif
// Check Compile Type - End

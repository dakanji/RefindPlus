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
#ifdef __MAKEWITH_GNUEFI // Compile Type = GNU_EFI

/**
  @retval EFI_INCOMPATIBLE_VERSION  Running on incompatible GNUEFI compiled version
**/
EFI_STATUS AmendSysTable (VOID) {
    // NOOP if not compiled using EDK II
    return EFI_INCOMPATIBLE_VERSION;
}

#else // Compile Type = OTHER - TIANOCORE

#include "../BootMaster/global.h"
#include "../include/refit_call_wrapper.h"
#include "../../MdeModulePkg/Core/Dxe/DxeMain.h"
#include "../../MdeModulePkg/Core/Dxe/Event/Event.h"

#define EFI_FIELD_OFFSET(TYPE, Field) ((UINTN) (&(((TYPE *) 0)->Field)))
#define TARGET_EFI_REVISION  EFI_2_30_SYSTEM_TABLE_REVISION

EFI_CPU_ARCH_PROTOCOL   *gCpu       = NULL;
EFI_SMM_BASE2_PROTOCOL  *gSmmBase2  = NULL;

EFI_RUNTIME_ARCH_PROTOCOL gRuntimeTemplate = {
    INITIALIZE_LIST_HEAD_VARIABLE (gRuntimeTemplate.ImageHead),
    INITIALIZE_LIST_HEAD_VARIABLE (gRuntimeTemplate.EventHead),
    sizeof (EFI_MEMORY_DESCRIPTOR) +
        sizeof (UINT64) -
        (sizeof (EFI_MEMORY_DESCRIPTOR) % sizeof (UINT64)),
    EFI_MEMORY_DESCRIPTOR_VERSION, 0,
    NULL, NULL, FALSE, FALSE
};

UINTN                       gEventPending      = 0;
EFI_TPL                     gEfiCurrentTpl     = TPL_APPLICATION;
EFI_LOCK                    gEventQueueLock    = EFI_INITIALIZE_LOCK_VARIABLE (TPL_HIGH_LEVEL);
EFI_RUNTIME_ARCH_PROTOCOL  *gRuntime           = &gRuntimeTemplate;
LIST_ENTRY                  gEventSignalQueue  = INITIALIZE_LIST_HEAD_VARIABLE (gEventSignalQueue);
LIST_ENTRY                  gEventQueue[TPL_HIGH_LEVEL + 1];

UINT32 mEventTable[] = {
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

    if (gCpu == NULL) {
        return;
    }

    if (!Enable) {
        gCpu->DisableInterrupt (gCpu);
        return;
    }

    if (gSmmBase2 == NULL) {
        gCpu->EnableInterrupt (gCpu);
        return;
    }

    Status = gSmmBase2->InSmm (gSmmBase2, &InSmm);
    if (!EFI_ERROR(Status) && !InSmm) {
        gCpu->EnableInterrupt (gCpu);
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

    OurAcquireLock (&gEventQueueLock);
    ASSERT (gEventQueueLock.OwnerTpl == Priority);
    Head = &gEventQueue[Priority];

    // Dispatch pending notifications
    while (!IsListEmpty (Head)) {
        Event = REFIT_CALL_4_WRAPPER(
            CR,
            Head->ForwardLink,
            IEVENT,
            NotifyLink,
            EVENT_SIGNATURE
        );
        REFIT_CALL_1_WRAPPER(RemoveEntryList, &Event->NotifyLink);
        Event->NotifyLink.ForwardLink = NULL;

        // Only clear the SIGNAL status if it is a SIGNAL type event.
        // WAIT type events are only cleared in CheckEvent()
        if ((Event->Type & EVT_NOTIFY_SIGNAL) != 0) {
            Event->SignalCount = 0;
        }

        OurReleaseLock (&gEventQueueLock);

        // Notify this event
        ASSERT (Event->NotifyFunction != NULL);

        REFIT_CALL_2_WRAPPER(
            Event->NotifyFunction,
            Event,
            Event->NotifyContext
        );

        // Check for next pending event
        OurAcquireLock (&gEventQueueLock);
    } // while

    gEventPending &= ~((UINTN) (1) << Priority);
    OurReleaseLock (&gEventQueueLock);
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

    OldTpl = gEfiCurrentTpl;
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
    gEfiCurrentTpl = NewTpl;

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

    OldTpl = gEfiCurrentTpl;
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
        gEfiCurrentTpl = TPL_HIGH_LEVEL;
    }

    // Dispatch pending events
    while (gEventPending != 0) {
        PendingTpl = (UINTN) HighBitSet64 (gEventPending);
        if (PendingTpl <= NewTpl) {
            break;
        }

        gEfiCurrentTpl = PendingTpl;
        if (gEfiCurrentTpl < TPL_HIGH_LEVEL) {
            OurSetInterruptState (TRUE);
        }
        OurDispatchEventNotifies (gEfiCurrentTpl);
    } // while

    // Set new value
    gEfiCurrentTpl = NewTpl;

    // Ensure interrupts are enabled if lowering below HIGH_LEVEL
    if (gEfiCurrentTpl < TPL_HIGH_LEVEL) {
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
    for (Index = 0; Index < (sizeof (mEventTable) / sizeof (UINT32)); Index++) {
        if (Type == mEventTable[Index]) {
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
        // Convert EFI 1.10 Events to their UEFI 2.0 CreateEventEx mapping
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
        // No notification needed. Zero out ignored values
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
        InsertTailList (&gRuntime->EventHead, &IEvent->RuntimeData.Link);
    }

    OurAcquireLock (&gEventQueueLock);

    if ((Type & EVT_NOTIFY_SIGNAL) != 0x00000000) {
        // The Event's NotifyFunction must be queued whenever the event is signaled
        InsertHeadList (&gEventSignalQueue, &IEvent->SignalLink);
    }

    OurReleaseLock (&gEventQueueLock);

    // Done
    return EFI_SUCCESS;
} // EFI_STATUS OurCreateEventEx()

/**
  @retval EFI_SUCCESS               The command completed successfully.
  @retval EFI_OUT_OF_RESOURCES      Out of memory.
  @retval EFI_ALREADY_STARTED       Already on UEFI 2.3 or later.
  @retval EFI_PROTOCOL_ERROR        Unexpected Field Offset.
**/
EFI_STATUS AmendSysTable (VOID) {
    EFI_BOOT_SERVICES *uBS;

    if (gST->Hdr.Revision >= TARGET_EFI_REVISION) {
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

    // Amend BootServices
    uBS->CreateEventEx  = OurCreateEventEx;
    uBS->Hdr.HeaderSize = sizeof (EFI_BOOT_SERVICES);
    uBS->Hdr.Revision   = TARGET_EFI_REVISION;
    uBS->Hdr.CRC32      = 0;
    uBS->CalculateCrc32 (
        uBS,
        uBS->Hdr.HeaderSize,
        &uBS->Hdr.CRC32
    );
    gBS = uBS;

    // Amend RuntimeServices
    gST->RuntimeServices->Hdr.HeaderSize = sizeof (EFI_RUNTIME_SERVICES);
    gRT->Hdr.HeaderSize = sizeof (EFI_RUNTIME_SERVICES);
    gRT->Hdr.Revision   = TARGET_EFI_REVISION;
    gRT->Hdr.CRC32      = 0;
    gBS->CalculateCrc32 (
        gRT,
        gRT->Hdr.HeaderSize,
        &gRT->Hdr.CRC32
    );

    // Amend SystemTable
    gST->BootServices   = gBS;
    gST->Hdr.HeaderSize = sizeof (EFI_SYSTEM_TABLE);
    gST->Hdr.Revision   = TARGET_EFI_REVISION;
    gST->Hdr.CRC32      = 0;
    gST->BootServices->CalculateCrc32 (
        gST,
        gST->Hdr.HeaderSize,
        &gST->Hdr.CRC32
    );

    // Flag Amendment
    SetSysTab = TRUE;

    return EFI_SUCCESS;
} // EFI_STATUS AmendSysTable()

#endif
// Check Compile Type - End

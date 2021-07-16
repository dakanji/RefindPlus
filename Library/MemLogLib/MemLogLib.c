/** @file
  Default instance of MemLogLib library for simple log services to memory buffer.
**/
/*
 * This file is from the Clover Boot Loader
 * Copyright (c) 2019, CloverHackyColor
 * https://github.com/CloverHackyColor/CloverBootloader
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* Modified for RefindPlus
 * Copyright (c) 2020-2021 Dayo Akanji (sf.net/u/dakanji/profile)
 *
 * Modifications distributed under the preceding terms.
 */


#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "MemLogLib.h"
#include <Library/DebugLib.h>

#include <Library/IoLib.h>
#include <Library/PciLib.h>
#include "GenericIch.h"

// Struct for holding mem buffer.
typedef struct {
    CHAR8             *Buffer;
    CHAR8             *Cursor;
    UINTN             BufferSize;
    MEM_LOG_CALLBACK  Callback;

    /// Start debug ticks.
    UINT64            TscStart;
    /// Last debug ticks.
    UINT64            TscLast;
    /// TSC ticks per second.
    UINT64            TscFreqSec;
} MEM_LOG;


// Guid for internal protocol for publishing mem log buffer.
EFI_GUID  mMemLogProtocolGuid = {0x74B91DA4, 0x2B4C, 0x11E2, {0x99, 0x03, 0x22, 0xF0, 0x61, 0x88, 0x70, 0x9B }};

// Pointer to mem log buffer.
MEM_LOG   *mMemLog = NULL;

// Buffer for debug time.
CHAR8     mTimingTxt[32];



/**
  Inits mem log.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.
**/
CHAR8 * GetTiming (VOID) {
	UINT64    dTStartSec;
	UINT64    dTStartMs;
	UINT64    dTLastSec;
	UINT64    dTLastMs;
	UINT64    CurrentTsc;

	mTimingTxt[0] = '\0';

	if (mMemLog != NULL && mMemLog->TscFreqSec != 0) {
		CurrentTsc = AsmReadTsc();

		dTStartMs = DivU64x64Remainder (
            MultU64x32 (
                CurrentTsc - mMemLog->TscStart,
                1000
            ),
            mMemLog->TscFreqSec,
            NULL
        );

        dTStartSec = DivU64x64Remainder (dTStartMs, 1000, &dTStartMs);

        // Limit logged value to 9999
        UINT64 dTStartSecLog;

        if (dTStartSec > 9999) {
            dTStartSecLog = 9999;
        }
        else {
            dTStartSecLog = dTStartSec;
        }

		dTLastMs = DivU64x64Remainder (
            MultU64x32 (
                CurrentTsc - mMemLog->TscLast,
                1000
            ),
            mMemLog->TscFreqSec,
            NULL
        );

        dTLastSec = DivU64x64Remainder (dTLastMs, 1000, &dTLastMs);

        // Limit logged value to 999
        UINT64 dTLastSecLog;
        if (dTLastSec > 9999) {
            dTLastSecLog = 9999;
        }
        else {
            dTLastSecLog = dTLastSec;
        }

		AsciiSPrint (
            mTimingTxt,
            sizeof (mTimingTxt),
            "%4ld:%03ld %4ld:%03ld",
            dTStartSecLog,
            dTStartMs,
            dTLastSecLog,
            dTLastMs
        );
		mMemLog->TscLast = CurrentTsc;
	}

	return mTimingTxt;
}



/**
  Inits mem log.

  @retval EFI_SUCCESS   The constructor always returns EFI_SUCCESS.
**/
EFI_STATUS EFIAPI MemLogInit (VOID) {
    EFI_STATUS      Status;
    UINT32          TimerAddr = 0;
    UINT64          Tsc0, Tsc1;
    UINT32          AcpiTick0, AcpiTick1, AcpiTicksDelta, AcpiTicksTarget;
    CHAR8           InitError[50];

    if (mMemLog != NULL) {
        return  EFI_SUCCESS;
    }

    // Try to use existing MEM_LOG
    Status = gBS->LocateProtocol (&mMemLogProtocolGuid, NULL, (VOID **) &mMemLog);
    if (Status == EFI_SUCCESS && mMemLog != NULL) {
        // We are inited with an existing MEM_LOG
        return EFI_SUCCESS;
    }

    // Set up and publish new MEM_LOG
    mMemLog = AllocateZeroPool ( sizeof (MEM_LOG) );
    if (mMemLog == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }
    mMemLog->BufferSize = MEM_LOG_INITIAL_SIZE;
    mMemLog->Buffer     = AllocateZeroPool (MEM_LOG_INITIAL_SIZE);
    mMemLog->Cursor     = mMemLog->Buffer;
    mMemLog->Callback   = NULL;

    // Calibrate TSC for timings
    InitError[0]='\0';

    // We will try to calibrate TSC frequency according to the ACPI Power Management Timer.
    // The ACPI PM Timer is running at a universal known frequency of 3579545Hz.
    // So, we wait 357954 clocks of the ACPI timer (100ms), and compare with how much TSC advanced.
    // This seems to provide a much more accurate calibration than using gBS->Stall(),
    // especially on UEFI machines, and is important as this value is used later to calculate FSBFrequency.

    // Check if we can use the timer - we need to be on Intel ICH,
    //  get ACPI PM Timer Address from PCI, and check that it is sane
    if ((PciRead16( PCI_ICH_LPC_ADDRESS(0))) != 0x8086) {
        // Intel ICH device was not found
        TimerAddr = 0;
        AsciiSPrint (InitError, sizeof (InitError), "Intel ICH Device Not Found");
    }
    else if ((PciRead8 (PCI_ICH_LPC_ADDRESS(R_ICH_LPC_ACPI_CNT)) & B_ICH_LPC_ACPI_CNT_ACPI_EN) == 0) {
        AsciiSPrint (InitError, sizeof (InitError), "ACPI I/O Space Not Enabled");
    }
    else {
        TimerAddr = ((PciRead16 (PCI_ICH_LPC_ADDRESS(R_ICH_LPC_ACPI_BASE))) & B_ICH_LPC_ACPI_BASE_BAR) +
            R_ACPI_PM1_TMR;

        if (TimerAddr < 9) {
            TimerAddr = 0;
            AsciiSPrint(InitError, sizeof (InitError), "Timer Address Not Obtained");
        }
        else {
            // Check that Timer is advancing
            AcpiTick0 = IoRead32 (TimerAddr);
            gBS->Stall(1000); // 1ms
            AcpiTick1 = IoRead32 (TimerAddr);

            if (AcpiTick0 == AcpiTick1) {
                TimerAddr = 0;
                AsciiSPrint (InitError, sizeof (InitError), "Timer Not Advancing");
            }
        }
    }

    // We prefer to use the ACPI PM Timer when possible. If it is not available we fallback to old method.
    if (TimerAddr == 0) {
        // ACPI PM Timer is not working, fall back on the old method

        // Read Current Tsc
        Tsc0 = AsmReadTsc();

        // Wait for 100ms
        gBS->Stall(100000); // 100ms

        // Read New Current Tsc
        Tsc1 = AsmReadTsc();

        // Get Frequency from Tsc Difference
        mMemLog->TscFreqSec = MultU64x32 ((Tsc1 - Tsc0), 10);
    }
    else {
        // ACPI PM Timer seems to be working
        // ACPI PM timers are usually of 24-bit length but there are some less common cases of 32-bit lengths.
        //   When the maximal number is reached, it overflows.
        // The code below can handle overflow with AcpiTicksTarget of up to 24-bit size,
        AcpiTicksTarget = V_ACPI_TMR_FREQUENCY/10; // 357954 clocks of ACPI timer (100ms)
        AcpiTick0       = IoRead32 (TimerAddr); // read ACPI tick
        Tsc0            = AsmReadTsc(); // read TSC

        do {
            CpuPause();

            // check how many AcpiTicks have passed since we started
            AcpiTick1 = IoRead32 (TimerAddr);
            if (AcpiTick0 <= AcpiTick1) {
                // no overflow
                AcpiTicksDelta = AcpiTick1 - AcpiTick0;
            }
            else if (AcpiTick0 - AcpiTick1 <= 0x00FFFFFF) {
                // overflow, 24-bit timer
                AcpiTicksDelta = (0x00FFFFFF - AcpiTick0) + AcpiTick1;
            }
            else {
                // overflow, 32-bit timer
                AcpiTicksDelta = (0xFFFFFFFF - AcpiTick0) + AcpiTick1;
            }
        } while (AcpiTicksDelta < AcpiTicksTarget); // keep checking Acpi ticks until target is reached

        Tsc1 = AsmReadTsc();

        // Done ... get another TSC
        mMemLog->TscFreqSec = DivU64x32 (
            MultU64x32 (
                (Tsc1 - Tsc0),
                V_ACPI_TMR_FREQUENCY
            ),
            AcpiTicksDelta
        );
    }

    mMemLog->TscStart = Tsc0;
    mMemLog->TscLast  = Tsc0;

    // Install (publish) MEM_LOG
    Status = gBS->InstallMultipleProtocolInterfaces (
        &gImageHandle,
        &mMemLogProtocolGuid,
        mMemLog,
        NULL
    );

    // Show Notice if Required
    if (InitError[0] != '\0') {
        MemLog(FALSE, 1, "INFO: Could Not Calibrate ACPI PM Timer\n");
        MemLog(FALSE, 1, "      %a\n\n", InitError);
    }

    return Status;
}

/**
  Prints a log message to memory buffer.

  @param  Timing      TRUE to prepend timing to log.
  @param  DebugMode   DebugMode will be passed to Callback function if it is set.
  @param  Format      The format string for the debug message to print.
  @param  Marker      VA_LIST with variable arguments for Format.
**/
VOID EFIAPI MemLogVA (
    IN  const BOOLEAN Timing,
    IN  const INTN    DebugMode,
    IN  const CHAR8   *Format,
    IN  VA_LIST       Marker
) {
    EFI_STATUS      Status;
    UINTN           DataWritten;
    CHAR8           *LastMessage;

    if (Format == NULL) {
        return;
    }

    Status = MemLogInit();
    if (EFI_ERROR (Status)) {
        return;
    }

    // Check if buffer can accept MEM_LOG_MAX_LINE_SIZE chars.
    // Increase buffer if not.
    if ((UINTN)(mMemLog->Cursor - mMemLog->Buffer) + MEM_LOG_MAX_LINE_SIZE > mMemLog->BufferSize) {
        UINTN Offset;
        // not enough place for max line - make buffer bigger
        // but not too big (if something gets out of control)
        if (mMemLog->BufferSize + MEM_LOG_INITIAL_SIZE > MEM_LOG_MAX_SIZE) {
            // Out of resources!
            return;
        }

        Offset = mMemLog->Cursor - mMemLog->Buffer;
        mMemLog->Buffer = ReallocatePool (
            mMemLog->BufferSize,
            mMemLog->BufferSize + MEM_LOG_INITIAL_SIZE,
            mMemLog->Buffer
        );

        if (mMemLog->Buffer == NULL) {
            return;
        }

        mMemLog->BufferSize += MEM_LOG_INITIAL_SIZE;
        mMemLog->Cursor = mMemLog->Buffer + Offset;
    }

    // Add log to buffer
    LastMessage = mMemLog->Cursor;
    if (Timing) {
        // Write timing only when starting a new line
        if ((mMemLog->Buffer[0] == '\0') || (mMemLog->Cursor[-1] == '\n')) {
            DataWritten = AsciiSPrint (
                mMemLog->Cursor,
                mMemLog->BufferSize - (mMemLog->Cursor - mMemLog->Buffer),
                "%a  ",
                GetTiming()
            );
            mMemLog->Cursor += DataWritten;
        }
    }

    DataWritten = AsciiVSPrint (
        mMemLog->Cursor,
        mMemLog->BufferSize - (mMemLog->Cursor - mMemLog->Buffer),
        Format,
        Marker
    );
    mMemLog->Cursor += DataWritten;

    // Pass this last message to callback if defined
    if (mMemLog->Callback != NULL) {
        mMemLog->Callback(DebugMode, LastMessage);
    }

    // Write to standard debug device also
    DebugPrint (DEBUG_INFO, LastMessage);
}

/**
  Prints a log to message memory buffer.

  If Format is NULL, then does nothing.

  @param  Timing      TRUE to prepend timing to log.
  @param  DebugMode   DebugMode will be passed to Callback function if it is set.
  @param  Format      The format string for the debug message to print.
  @param  ...         The variable argument list whose contents are accessed
  based on the format string specified by Format.
 **/
VOID EFIAPI MemLog (
    IN  const BOOLEAN Timing,
    IN  const INTN    DebugMode,
    IN  const CHAR8   *Format,
    ...
) {
    VA_LIST           Marker;

    if (Format == NULL) {
        return;
    }

    VA_START (Marker, Format);
    MemLogVA (Timing, DebugMode, Format, Marker);
    VA_END (Marker);
}


/**
 Returns pointer to MemLog buffer.
 **/
CHAR8 * EFIAPI GetMemLogBuffer (VOID) {
    EFI_STATUS        Status;

    Status = MemLogInit ();
    if (EFI_ERROR (Status)) {
        return NULL;
    }

    return mMemLog != NULL ? mMemLog->Buffer : NULL;
}


/**
 Returns the length of log (number of chars written) in mem buffer.
 **/
UINTN EFIAPI GetMemLogLen (VOID) {
    EFI_STATUS        Status;

    Status = MemLogInit ();
    if (EFI_ERROR (Status)) {
        return 0;
    }

    return mMemLog != NULL ? mMemLog->Cursor - mMemLog->Buffer : 0;
}

/**
  Sets callback that will be called when message is added to mem log.
 **/
VOID EFIAPI SetMemLogCallback (
    MEM_LOG_CALLBACK  Callback
) {
    EFI_STATUS        Status;

    Status = MemLogInit ();
    if (EFI_ERROR (Status)) {
        return;
    }

    mMemLog->Callback = Callback;
}

/**
  Returns TSC ticks per second.
 **/
UINT64 EFIAPI GetMemLogTscTicksPerSecond (VOID) {
    EFI_STATUS        Status;

    Status = MemLogInit ();
    if (EFI_ERROR (Status)) {
        return 0;
    }

    return mMemLog->TscFreqSec;
}

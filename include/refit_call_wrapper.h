#ifndef __REFIT_CALL_WRAPPER_H__
#define __REFIT_CALL_WRAPPER_H__

#ifdef __MAKEWITH_GNUEFI

#if defined (EFIX64) | defined (AARCH64)
# define REFIT_CALL_1_WRAPPER(f, a1) \
  uefi_call_wrapper(f, 1, (UINT64)(a1))
# define REFIT_CALL_2_WRAPPER(f, a1, a2) \
  uefi_call_wrapper(f, 2, (UINT64)(a1), (UINT64)(a2))
# define REFIT_CALL_3_WRAPPER(f, a1, a2, a3) \
  uefi_call_wrapper(f, 3, (UINT64)(a1), (UINT64)(a2), (UINT64)(a3))
# define REFIT_CALL_4_WRAPPER(f, a1, a2, a3, a4) \
  uefi_call_wrapper(f, 4, (UINT64)(a1), (UINT64)(a2), (UINT64)(a3), (UINT64)(a4))
# define REFIT_CALL_5_WRAPPER(f, a1, a2, a3, a4, a5) \
  uefi_call_wrapper(f, 5, (UINT64)(a1), (UINT64)(a2), (UINT64)(a3), (UINT64)(a4), (UINT64)(a5))
# define REFIT_CALL_6_WRAPPER(f, a1, a2, a3, a4, a5, a6) \
  uefi_call_wrapper(f, 6, (UINT64)(a1), (UINT64)(a2), (UINT64)(a3), (UINT64)(a4), (UINT64)(a5), (UINT64)(a6))
# define REFIT_CALL_7_WRAPPER(f, a1, a2, a3, a4, a5, a6, a7) \
  uefi_call_wrapper(f, 6, (UINT64)(a1), (UINT64)(a2), (UINT64)(a3), (UINT64)(a4), (UINT64)(a5), (UINT64)(a6), (UINT64)(a7))
# define REFIT_CALL_8_WRAPPER(f, a1, a2, a3, a4, a5, a6, a7, a8) \
  uefi_call_wrapper(f, 10, (UINT64)(a1), (UINT64)(a2), (UINT64)(a3), (UINT64)(a4), (UINT64)(a5), (UINT64)(a6), (UINT64)(a7), (UINT64)(a8))
# define REFIT_CALL_9_WRAPPER(f, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
  uefi_call_wrapper(f, 10, (UINT64)(a1), (UINT64)(a2), (UINT64)(a3), (UINT64)(a4), (UINT64)(a5), (UINT64)(a6), (UINT64)(a7), (UINT64)(a8), (UINT64)(a9))
# define REFIT_CALL_10_WRAPPER(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) \
  uefi_call_wrapper(f, 10, (UINT64)(a1), (UINT64)(a2), (UINT64)(a3), (UINT64)(a4), (UINT64)(a5), (UINT64)(a6), (UINT64)(a7), (UINT64)(a8), (UINT64)(a9), (UINT64)(a10))
#else
# define REFIT_CALL_1_WRAPPER(f, a1) \
  uefi_call_wrapper(f, 1, a1)
# define REFIT_CALL_2_WRAPPER(f, a1, a2) \
  uefi_call_wrapper(f, 2, a1, a2)
# define REFIT_CALL_3_WRAPPER(f, a1, a2, a3) \
  uefi_call_wrapper(f, 3, a1, a2, a3)
# define REFIT_CALL_4_WRAPPER(f, a1, a2, a3, a4) \
  uefi_call_wrapper(f, 4, a1, a2, a3, a4)
# define REFIT_CALL_5_WRAPPER(f, a1, a2, a3, a4, a5) \
  uefi_call_wrapper(f, 5, a1, a2, a3, a4, a5)
# define REFIT_CALL_6_WRAPPER(f, a1, a2, a3, a4, a5, a6) \
  uefi_call_wrapper(f, 6, a1, a2, a3, a4, a5, a6)
# define REFIT_CALL_7_WRAPPER(f, a1, a2, a3, a4, a5, a6, a7) \
  uefi_call_wrapper(f, 10, a1, a2, a3, a4, a5, a6, a7)
# define REFIT_CALL_8_WRAPPER(f, a1, a2, a3, a4, a5, a6, a7, a8) \
  uefi_call_wrapper(f, 10, a1, a2, a3, a4, a5, a6, a7, a8)
# define REFIT_CALL_9_WRAPPER(f, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
  uefi_call_wrapper(f, 10, a1, a2, a3, a4, a5, a6, a7, a8, a9)
# define REFIT_CALL_10_WRAPPER(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) \
  uefi_call_wrapper(f, 10, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)
#endif

#else /* not GNU EFI -- TianoCore EDK2 */

#define REFIT_CALL_1_WRAPPER(f, a1) \
        f(a1)
#define REFIT_CALL_2_WRAPPER(f, a1, a2) \
        f(a1, a2)
#define REFIT_CALL_3_WRAPPER(f, a1, a2, a3) \
        f(a1, a2, a3)
#define REFIT_CALL_4_WRAPPER(f, a1, a2, a3, a4) \
        f(a1, a2, a3, a4)
#define REFIT_CALL_5_WRAPPER(f, a1, a2, a3, a4, a5) \
        f(a1, a2, a3, a4, a5)
#define REFIT_CALL_6_WRAPPER(f, a1, a2, a3, a4, a5, a6) \
        f(a1, a2, a3, a4, a5, a6)
#define REFIT_CALL_7_WRAPPER(f, a1, a2, a3, a4, a5, a6, a7) \
        f(a1, a2, a3, a4, a5, a6, a7)
#define REFIT_CALL_8_WRAPPER(f, a1, a2, a3, a4, a5, a6, a7, a8) \
        f(a1, a2, a3, a4, a5, a6, a7, a8)
#define REFIT_CALL_9_WRAPPER(f, a1, a2, a3, a4, a5, a6, a7, a8, a9) \
        f(a1, a2, a3, a4, a5, a6, a7, a8, a9)
#define REFIT_CALL_10_WRAPPER(f, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) \
        f(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)

#define uefi_call_wrapper(f, n, ...) \
        f(__VA_ARGS__)

#endif /* not GNU EFI -- TianoCore EDK2 */

#endif /* !__REFIT_CALL_WRAPPER_H__ */

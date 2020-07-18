EFI_STATUS
simple_file_open (EFI_HANDLE image, CHAR16 *name, EFI_FILE **file, UINT64 mode);
EFI_STATUS
simple_file_open_by_handle(EFI_HANDLE device, CHAR16 *name, EFI_FILE **file, UINT64 mode);
EFI_STATUS
simple_file_read_all(EFI_FILE *file, UINTN *size, void **buffer);
void
simple_file_close(EFI_FILE *file);


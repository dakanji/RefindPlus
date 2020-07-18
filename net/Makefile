IPXE_GIT=git://git.ipxe.org/ipxe.git
EFI_PREFIX=efi_discovery_prefix.c
EFI_DISCOVERY_BINARY=ipxe_discovery.efi
EFI_DISCOVERY_ENTRY=_efi_discovery_start
IPXE_SRC = ipxe

source:
	rm -rf $(IPXE_SRC)/
	git clone $(IPXE_GIT)
	cp discovery/$(EFI_PREFIX) ipxe/src/arch/x86/prefix/
	cp discovery/Makefile.housekeeping ipxe/src/Makefile.housekeeping
netboot:
	mkdir -p bin
	cp discovery/console.h ipxe/src/config/local/console.h;\
	cd ipxe/src;\
	make bin-x86_64-efi/ipxe.efi TGT_LD_ENTRY=$(EFI_DISCOVERY_ENTRY);\
	cp bin-x86_64-efi/ipxe.efi ../../bin/$(EFI_DISCOVERY_BINARY);\
	rm -r bin-x86_64-efi/;\
	rm config/local/console.h;\
	make bin-x86_64-efi/ipxe.efi;\
	cp bin-x86_64-efi/ipxe.efi ../../bin/ipxe.efi;\
	cd ../../
clean:
	rm -r bin/

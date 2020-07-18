/*
 * Copyright (C) 2009 Michael Brown <mbrown@fensystems.co.uk>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <stdlib.h>
#include <ipxe/efi/efi_driver.h>
#include <ipxe/efi/efi_snp.h>
#include <ipxe/efi/efi_strings.h>
#include <ipxe/umalloc.h>
#include <ipxe/uri.h>
#include <ipxe/init.h>
#include <usr/ifmgmt.h>
#include <usr/route.h>
#include <usr/autoboot.h>

#define MAX_EXIT_BUFFER_SIZE 256

/**
 * Close all open net devices
 *
 * Called before a fresh boot attempt in order to free up memory.  We
 * don't just close the device immediately after the boot fails,
 * because there may still be TCP connections in the process of
 * closing.
 */
static void close_all_netdevs ( void ) {
	struct net_device *netdev;

	for_each_netdev ( netdev ) {
		ifclose ( netdev );
	}
}

static struct uri* try_getting_next_server ( struct net_device *netdev ) {
	struct uri *filename;

	/* Close all other network devices */
	close_all_netdevs();

	/* Open device and display device status */
	if ( ifopen ( netdev ) != 0 )
		goto err_ifopen;
	ifstat ( netdev );

	/* Configure device */
	if (ifconf ( netdev, NULL )!= 0 )
		goto err_dhcp;
	route();

  /* Fetch next server and filename */
	filename = fetch_next_server_and_filename ( NULL );
	if ( ! filename )
		goto err_filename;
	if ( ! uri_has_path ( filename ) ) {
		/* Ignore empty filename */
		uri_put ( filename );
		filename = NULL;
	}
 
	return filename;
  err_filename:
  err_dhcp:
  err_ifopen:
    return NULL;
}


static struct uri* efi_discover ( void ) {
	struct net_device *netdev;
  
  struct uri* filename = NULL;

	for_each_netdev ( netdev ) {
		filename = try_getting_next_server ( netdev );
	}

	return filename;
}

/**
 * EFI entry point
 *
 * @v image_handle	Image handle
 * @v systab		System table
 * @ret efirc		EFI return status code
 */
EFI_STATUS EFIAPI _efi_discovery_start ( EFI_HANDLE image_handle,
			       EFI_SYSTEM_TABLE *systab ) {
	EFI_STATUS efirc;
  struct uri* filename;
  userptr_t user_buf;
  wchar_t* exit_buf;

	/* Initialise EFI environment */
	if ( ( efirc = efi_init ( image_handle, systab ) ) != 0 )
		goto err_init;

  if ( ( user_buf = umalloc(MAX_EXIT_BUFFER_SIZE*2) ) == 0)
  {
     efirc = EFI_OUT_OF_RESOURCES;
     goto err_init;
  }
  
  exit_buf = (wchar_t *)user_to_phys(user_buf,0);

  initialise();
  startup();

  if ( ( filename = efi_discover() ) == NULL)
  {
     efirc = EFI_NOT_FOUND;
     goto err_filename;
  }
  
	efi_snp_release();
	efi_loaded_image->Unload ( image_handle );
	efi_driver_reconnect_all();
 
  efi_snprintf(exit_buf,MAX_EXIT_BUFFER_SIZE,"%s - %s", filename->host, filename->path);
  uri_put(filename);

  systab->BootServices->Exit(image_handle, efirc, MAX_EXIT_BUFFER_SIZE, (CHAR16 *) exit_buf);

 err_filename:
 err_init:
	systab->BootServices->Exit(image_handle, efirc, 0, NULL);
	return efirc;
}

/**
 * Probe EFI root bus
 *
 * @v rootdev		EFI root device
 */
static int efi_probe ( struct root_device *rootdev __unused ) {

	return efi_driver_connect_all();
}

/**
 * Remove EFI root bus
 *
 * @v rootdev		EFI root device
 */
static void efi_remove ( struct root_device *rootdev __unused ) {

	efi_driver_disconnect_all();
}

/** EFI root device driver */
static struct root_driver efi_root_driver = {
	.probe = efi_probe,
	.remove = efi_remove,
};

/** EFI root device */
struct root_device efi_root_device __root_device = {
	.dev = { .name = "EFI" },
	.driver = &efi_root_driver,
};

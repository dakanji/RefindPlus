This is the Snowy theme for rEFInd (http://www.rodsbooks.com/refind/). To
use it:

1) Unpack the .zip file (which you've presumably already done).

2) Copy the "snowy" subdirectory to your rEFInd installation. rEFInd can be
   located various places, depending on your OS:
   * On most Linux systems, the ESP is mounted at /boot/efi, so rEFInd is
     at /boot/efi/EFI/refind.
   * On OS X, if you haven't configured the ESP to auto-mount, you should
     run the "mountesp" script that comes with rEFInd 0.10.0 and later.
     This will mount the ESP, typically at /Volumes/ESP, so rEFInd will
     probably be at /Volumes/ESP/EFI/refind.
   * On Windows, the ESP is not mounted by default. In an Administrator
     Command Prompt window, you can type "mountvol S: /S" to mount the ESP
     as S:. (You can change the device letter, if necessary.) rEFInd should
     then normally be S:\EFI\refind.

3) Open the refind.conf file in the rEFInd directory in your favorite
   text editor.

4) Add the following line to the END of your refind.conf file:
   include refind-snowy.conf

You can swap out any element, including the banner/background image, as you
see fit. The banner-snowy.png image is 1920x1080. If your display is
another size, the image will be scaled to fit. If you see artifacts, or if
your display does not use a 16:9 aspect ratio, you may want to load the
file into a graphics editor to scale it or to crop it.

This theme is derived from icons from several sources; see snowy/README for
details.

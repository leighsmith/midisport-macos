Installation instructions for the
MIDIMAN MIDISPORT USB MIDI device driver V1.0.0
for MacOS 10.14 (Mojave)

This driver supports the following devices:

    - MIDISPORT 1x1
    - MIDISPORT 2x2
    - MIDISPORT 4x4
    - MIDISPORT 8x8

1. Decompress the tarball (MIDISPORT-2.1.2.b.MOX.pkg.tar) to create the package MIDISPORT.pkg and this file.

Double-Click on the .tar file to create MIDISPORT.pkg and this file using StuffIt Expander
or OpenUp.app (obtainable via www.stepwise.com) or open Terminal.app and run:

	     tar xvf MIDISPORT-2.1.2.b.MOX.pkg.tar

to extract MIDISPORT.pkg.

2. Double-Click the .pkg package, and follow the standard installation operation to
install the plugin. When prompted for authorization to install the plugin, click on the
lock icon in the lower left corner of the dialog and enter an adminstrator's password. The
installer should place the plugin into /Library/Audio/MIDI Drivers/MIDISPORT.plugin.

3. Connect the MIDISPORT device to the USB chain. The status light will not glow
immediately after connecting, only after the MIDISPORT firmware has been downloaded to the
interface.

4. The first time an application attempts to open the MIDI driver, the firmware for the
MIDISPORT will be downloaded which will be indicated by its LED labelled "USB"
pulsing. Subsequent opening of the device will then use the already downloaded firmware.

5. Copyright (c) 2001 by Leigh M. Smith <leigh@leighsmith.com>.  All rights reserved.

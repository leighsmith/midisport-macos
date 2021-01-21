#!/bin/sh
DMGFILE=~/Downloads/MIDI_USB_OSX_3.5.3.dmg
# Download MIDI_USB_OSX_3.5.3.dmg
# https://mac.softpedia.com/get/Drivers/M-Audio-MIDISport-Series.shtml#download
# The URL provided changes on each request, so we assume the user manually downloads and
# places the file into their Downloads folder.
# curl --output $DMGFILE https://us.softpedia-secure-download.com/dl/c05c654f4d8f1c1d214be8f5e881923e/6004d9a1/400050371/mac/Drivers/MIDI_USB_OSX_3.5.3.dmg
# Check the file downloaded ok.
if [ -f $DMGFILE ]; then
    echo "Found the M-Audio driver, now extracting the firmware files."
    # Mount the DMG file
    hdiutil attach $DMGFILE
    # Extract the .rsrc file
    /usr/bin/xar -x --exclude Distribution --exclude Resources -f "/Volumes/M-Audio USB MIDI Support 3.5.3/M-Audio USB MIDI Support Installer.pkg"
    gunzip -c - < com.m-audio.usbmidisupport.macos.installer_0.pkg/Payload | cpio -i -d "./Library/StartupItems/M-Audio Firmware Loader/MA Firmware Loader.rsrc"
    rm -r com.m-audio.usbmidisupport.macos.installer_0.pkg
    # Decode the resource file.
    /usr/bin/DeRez "./Library/StartupItems/M-Audio Firmware Loader/MA Firmware Loader.rsrc" -useDF > MA_Firmware_Loader.rsrc.decoded
    python $(dirname $0)/rsrc2ihex.py < MA_Firmware_Loader.rsrc.decoded
    # Move all of the generated .ihx files into the expected location
    sudo mv *.ihx /usr/local/etc/midisport_firmware/
    # Now unmount the disk image.
    rm -r ./Library
    rm MA_Firmware_Loader.rsrc.decoded
    umount "/Volumes/M-Audio USB MIDI Support 3.5.3/"
else
    echo "Unable to find and extract the M-Audio driver firmware."
fi

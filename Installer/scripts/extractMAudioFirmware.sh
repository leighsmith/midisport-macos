#!/bin/sh
# TODO Download MIDI_USB_OSX_3.5.3.dmg
curl MIDI_USB_OSX_3.5.3.dmg
# Mount MIDI_USB_OSX_3.5.3.dmg
hdiutil attach MIDI_USB_OSX_3.5.3.dmg
# Extract the .rsrc file
/usr/bin/xar -x --exclude Distribution --exclude Resources -f "/Volumes/M-Audio USB MIDI Support 3.5.3/M-Audio USB MIDI Support Installer.pkg"
gunzip -c - < com.m-audio.usbmidisupport.macos.installer_0.pkg/Payload | cpio -i -d "./Library/StartupItems/M-Audio Firmware Loader/MA Firmware Loader.rsrc"
rm -r com.m-audio.usbmidisupport.macos.installer_0.pkg
# Decode the resource file.
/usr/bin/DeRez "./Library/StartupItems/M-Audio Firmware Loader/MA Firmware Loader.rsrc" -useDF > MA_Firmware_Loader.rsrc.decoded
python rsrc2ihex.py < MA_Firmware_Loader.rsrc.decoded
# TODO copy a subset of the generated .ihx files
sudo cp *.ihx /usr/local/etc/midisport_firmware/
# Now unmount the disk image.
rm -r ./Library
rm MA_Firmware_Loader.rsrc.decoded
umount "/Volumes/M-Audio USB MIDI Support 3.5.3/"

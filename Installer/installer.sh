#!/bin/zsh
# TODO Install the MIDISPORT_devices.xml file to /usr/local/etc/midisport_firmware
cp MIDISPORT_devices.xml /usr/local/etc/midisport_firmware/MIDISPORT_devices.xml
# Download and install all Intel Hex firmware files from the Linux distribution to /usr/local/etc/midisport_firmware
curl --location --output /tmp/midisport_firmware.tar.gz https://downloads.sourceforge.net/project/usb-midi-fw/midisport-firmware/1.2/midisport-firmware-1.2.tar.gz
tar -C /tmp/ -x -z -f /tmp/midisport_firmware.tar.gz
cp /tmp/midisport-firmware-1.2/*.ihx /usr/local/etc/midisport_firmware/

# TODO Install MIDISPORTFirmwareDownloader to /usr/local/libexec/MIDISPORTFirmwareDownloader
# TODO Install com.leighsmith.midisportfirmwaredownloader.plist /Library/LaunchDaemons/



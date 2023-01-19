M-Audio MIDISPORT USB 64-bit MIDI device driver for MacOS v10.14+ (Mojave/Catalina/Big Sur)
===========================================================================================

![MIDISPORT 2x2](https://raw.githubusercontent.com/leighsmith/midisport-macos/main/MyMIDISPORT.jpeg)

This project provides an open source MacOS v10.14, v10.15, v11.6, v12.6 (Mojave, Catalina,
Big Sur, Monterey) compatible CoreMIDI 64 bit device driver for M-Audio's MIDISPORT range
of USB MIDI interfaces. This driver supports the following devices:

+ MIDISPORT 1x1
+ MIDISPORT 2x2
+ MIDISPORT 4x4
+ MIDISPORT 8x8/S
+ Oxygen 8 Keyboard

Other devices which are compatible may also work, although presently, these are the only
ones which have been tested. See the disclaimer below before testing this software on any
device not listed above.

History:
--------

I originally wrote the first MacOS X version of the MIDISPORT device driver back in 2000
and donated it to M-Audio (then named MIDIMan), as part of a start-up project (tomandandy
music inc.) that needed a MIDI device driver running on what was then a pre-release
version of MacOS X. M-Audio took the code and updated it many times without giving me back
their updated source code. M-Audio now seem to have abandoned the hardware and no longer
support their version of the driver since 2009, as their newer devices do not use that
driver. With the move by Apple to v10.14 and onwards to no longer support 32 bit drivers, I
have modified and updated my original code that was donated, to now compile as 64 bit
versions on these latest MacOS versions, so MIDISPORT owners can continue to support and
operate their hardware.

Necessary Disclaimer:
---------------------

This project has no support from M-Audio, and M-Audio is in no way responsible for this
code. In addition, any authors listed in this code are not responsible for the fitness and
suitability of purpose, freedom from defects, or behaviour of this software. There is no
warranty for this code. It is essential to understand that any software interacting with a
piece of hardware can potentially damage it. You therefore use this software at your own
risk, and are solely responsible for deciding it's fitness to your purpose. See the
LICENSE.txt file for full license and warranty declarations.

MIDISPORT is a trademark, and MIDIMAN is a registered trademark, of the M-Audio and/or MIDIMAN
companies.

Project Structure
-----------------

The project consists of two parts:

1. The MacOS X CoreMIDI device driver itself, consisting of a modified version of Apple's
   publicly available (and now very old) MIDI device driver example code.

2. A firmware downloader running as a [launchd](https://www.launchd.info/) daemon,
   which downloads to the EZ-USB
   [8051](https://www.electronicshub.org/8051-microcontroller-architecture/)
   compatible microcontroller within the MIDISPORT devices, the firmware to transmit
   and receive to and from MIDI and USB ports on the devices.

MacOS X CoreMIDI Device Driver
------------------------------

The MacOS X CoreMIDI device driver is a modified version of Apple's publicly available
(and now very old) CoreMIDI device driver example code. This was adapted for the
MIDISPORT. All technical details of the MIDISPORT devices within the driver are also publicly available from
the [open source Linux version of the MIDISPORT driver](https://www.alsa-project.org/wiki/Usb-midi-fw),
together with publicly available [Linux](https://github.com/esden/fxload)
and [MacOS](https://developer.apple.com/library/archive/documentation/DeviceDrivers/Conceptual/USBBook/USBDeviceInterfaces/USBDevInterfaces.html#//apple_ref/doc/uid/TP40002645-TPXREF105)
example code for downloading firmware for the EZ-USB microcontroller inside the MIDISPORT devices.

Currently the code has been tested on MacOS 10.14, 10.15, and v11.7.2, v12.6.2.

MIDISPORT Firmware
------------------

In order to avoid distributing any M-Audio supplied code, or infringe on NDAs, none of the
MIDISPORT firmware is distributed as part of this project. The firmware downloader code
reads Intel Hex format versions of firmware files that were publicly distributed by
M-Audio as part of their [Linux driver effort](http://usb-midi-fw.sourceforge.net/). Users
will therefore need to download those files to their Mac and save them into the
appropriate folder location for the firmware downloader to find them. However the driver
installation script will perform this downloading as part of the installation of the
driver.

Not all firmware for every MIDISPORT device M-Audio produced is distributed with the Linux
driver. Support is currently missing for some of the later models which used different
firmware. Those firmware files are distributed as part of the download for M-Audio's
last 32 bit driver v3.5.3. If the .dmg file for that driver is placed in `~/Downloads`,
this project's installer will also install the firmware files from that old .dmg file.

Installation instructions
-------------------------

1. Make sure you first remove the old M-Audio MIDI device driver from your system before
installing this driver. Ideally use their `M-Audio USB MIDI Uninstaller.app` which is part
of their DMG file for their installer. However, that won't run on Catalina/Big Sur as it's a 32
bit app. In that case, you will need to manually remove:

* Their driver from `/Library/Audio/MIDI Drivers/M-Audio USB Support.plugin`
* Their firmware downloaders from `/Library/Extensions/M-AudioUSBMIDISupport.kext` and `/System/Library/Extensions/M-AudioUSBMIDISupport.kext`.
* Their launch daemon config from `/Library/LaunchDaemons/com.m-audio.usb.firmwareloader.plist`
* Their firmware from `/Library/StartupItems/M-Audio Firmware Loader/`

2. If you need to install firmware for devices other than the original MIDISPORTs listed
above (for example, anniversary editions, Oxygen 8 and similar devices), download the
M-Audio v3.5.3 DMG file now archived at
[Softpedia.com](https://mac.softpedia.com/get/Drivers/M-Audio-MIDISport-Series.shtml#download),
and place into your `~/Downloads` folder. The installer will look for the DMG file there.

3. If you are attempting to install on MacOS 10, the M-Audio DMG file extractor now
requires Python v3, which was not installed as standard on that version of the operating
system. You therefore need to have Python 3 installed in a location which is accessible to
the installer, i.e. `/usr/local/bin`. For MacOS 11 and greater, Python v3 is installed by
default.

4. Download the package [MIDISPORT.pkg](https://sourceforge.net/projects/midisport-macos/files/).

5. Double-Click the .pkg package, and follow the standard installation operation to
install the plugin. When prompted for authorization to install the plugin, enter an
administrator's password. Note that installer will attempt to download the firmware files,
so you need to be connected to the Internet. The installer should place:

* The plugin into `/Library/Audio/MIDI Drivers/MIDISPORT.plugin`
* The firmware downloader into `/usr/local/libexec/MIDISPORTFirmwareDownloader`
* A [launchd](https://www.launchd.info/) configuration file into `/Library/LaunchDaemons/`
* The firmware files into `/usr/local/etc/midisport_firmware/`

6. You will need to reboot the operating system in order to launch the
MIDISPORTFirmwareDownloader utility which will wait for MIDISPORT devices to be plugged
into the USB bus.

7. Connect the MIDISPORT device to the USB chain. If the device and the firmware files can
be found, the firmware for the MIDISPORT will be downloaded which will be indicated by its
LED labelled "USB" pulsing.

8. Open `/Applications/Utilities/Audio MIDI Setup.app` and select the MIDI Studio window
of the app. You should see the MIDISPORT MIDI interface device appear, and you can then define MIDI
devices and connect them to the MIDISPORT interface device in the standard operation of
the utility application. Note that if you have run the M-Audio driver in the past, the
previous MIDI interfaces will still appear and any MIDI devices connected to them will
remain. You will need to remove those connections and reconnect the MIDI devices to the
new available MIDISPORT interface.

Building from Source
--------------------

The package .pkg file above will fulfill musicians requirements. If however, you want to develop
the code, the entire package is compiled and built by running:

```
    xcodebuild -project MIDISPORT.xcodeproj -target Package -configuration Deployment install
```

from the Terminal.app command line. This will compile both the firmware downloader and the
MIDI driver plugin, and produce the installable package. The package name will include the
version number. To increment that version number, the following files need to be changed:

* The `CURRENT_PROJECT_VERSION` value for the `Package` target in the `MIDISPORT.xcodeproj/project.pbxproj` XCode project file.
* The value of `CFBundleShortVersionString` in `MIDISPORT/Info-MIDISPORT.plist`.
* Localised (English only, currently) entries in `MIDISPORT/en.lproj/InfoPlist.strings`.

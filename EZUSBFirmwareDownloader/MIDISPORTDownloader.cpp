// $Id: MIDISPORTDownloader.cpp,v 1.2 2000/12/13 05:09:56 leigh Exp $
//
// MacOS X standalone firmware downloader for the EZUSB device, 
// as found in MIDIMan MIDISPORT boxes.
//
// This is the MIDISPORT specific main function. The rest of the utility is applicable
// to all EZUSB boxes.
//
// By Leigh Smith <leigh@tomandandy.com>
//
// Copyright (c) 2000 tomandandy music inc. All Rights Reserved.
// Permission is granted to use and modify this code for commercial and
// non-commercial purposes so long as the author attribution and this
// copyright message remains intact and accompanies all derived code.
//
#include <iostream>

#include "EZLoader.h"

#define midimanVendorID 0x0763
#define coldBootProductID 0x1001

int main (int argc, const char * argv[])
{
    IOUSBDeviceRef ezusbDev;
    extern INTEL_HEX_RECORD firmware[];
    EZUSBLoader ezusb;
    
    cout << "Downloading MIDISPORT Firmware\n";
    
    ezusbDev = ezusb.FindDevice(midimanVendorID, coldBootProductID);
    if (ezusbDev != NULL) {
        ezusb.setFirmware(firmware);
        ezusb.StartDevice(ezusbDev);
    }
    else {
        cout << "No EZUSB (i.e MIDISPORT) device found\n";
    }
    return 0;
}

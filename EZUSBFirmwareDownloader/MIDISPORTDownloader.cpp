// $Id: MIDISPORTDownloader.cpp,v 1.1 2000/10/22 02:22:32 leigh Exp $
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
    
    cout << "Downloading MIDISPORT Firmware\n";
    
    ezusbDev = Ezusb_FindDevice(midimanVendorID, coldBootProductID);
    if (ezusbDev != NULL) {
        Ezusb_setFirmware(firmware);
        Ezusb_StartDevice(ezusbDev);
    }
    else {
        cout << "No EZUSB (i.e MIDISPORT) device found\n";
    }
    return 0;
}

// $Id: EZLoader.cpp,v 1.1.1.1 2000/10/22 02:22:32 leigh Exp $
//
// MacOS X standalone firmware downloader for the EZUSB device, 
// as found in MIDIMan MIDISPORT boxes.
//
// This is the code for downloading any firmware to pre-renumerated EZUSB devices.
// This is a rewrite of EZLOADER.C which was supplied example code with the EZUSB device.
//
// By Leigh Smith <leigh@tomandandy.com>
//
// Copyright (c) 2000 tomandandy music inc. All Rights Reserved.
// Permission is granted to use and modify this code for commercial and
// non-commercial purposes so long as the author attribution and this
// copyright message remains intact and accompanies all derived code.
//

#include <stdarg.h>
#include <stdio.h>

// Include file for the Ezusb Device
//
#include "EZLoader.h"
#include "USBUtils.h"

#define VERBOSE (DEBUG && 1)

// 0 is the standard USB interface which we need to download to/on.
#define kTheInterfaceToUse	0	

// these files contain images of the device firmware
//
static INTEL_HEX_RECORD *firmware;
extern INTEL_HEX_RECORD loader[];

static XUSBInterface EZUSBinterface = NULL;
static IOUSBDeviceRef EZUSBdevice = NULL;

// This class finds USB interface instances
class EZUSBInterfaceLocator : public USBDeviceLocator {
public:
	
    virtual int	InterfaceIndexToUse(IOUSBDeviceRef device)
    {
        return kTheInterfaceToUse;
    }

    // Triggered when kTheInterfaceToUse has been found.
    virtual bool FoundInterface(IOUSBDeviceRef device, XUSBInterface interface)
    {
#if VERBOSE
        printf("yep found it\n");
#endif
        EZUSBinterface = interface;
        EZUSBdevice = device;
        return true;		// keep device/interface allocated
    }
};


//EZUSB::EZUSB()
//{
// EZUSBinterface = NULL;
//EZUSBdevice = NULL;
//}

/*++

Routine Description:
   Uses the ANCHOR LOAD vendor specific command to either set or release the
   8051 reset bit in the EZ-USB chip.

Arguments:
   fdo - pointer to the device object for this instance of an Ezusb Device
   resetBit - 1 sets the 8051 reset bit (holds the 8051 in reset)
              0 clears the 8051 reset bit (8051 starts running)
              
Return Value:
   kIOReturnSuccess if we reset correctly.
--*/
IOReturn Ezusb_8051Reset(IOUSBDeviceRef device, unsigned char resetBit)
{
    IOReturn status;
    UInt16 bufSize = 1;
    UInt8 bmreqType;
    
#if VERBOSE
    printf("setting 8051 reset bit to %d\n", resetBit);
#endif
    bmreqType = USBMakeBMRequestType(kUSBOut, kUSBVendor, kUSBDevice);
    status = IOUSBDeviceRequest(device, bmreqType, ANCHOR_LOAD_INTERNAL, CPUCS_REG, 0,
                                (void *) &resetBit, &bufSize);
    return status;
}

/*
****************************************************************************
*  Routine Description:
*	This function downloads Intel Hex Records to the EZ-USB device.  
*	If any of the hex records are destined for external RAM, then 
*	the caller must have previously downloaded firmware to the device 
*	that knows how to download to external RAM (ie. firmware that 
*	implements the ANCHOR_LOAD_EXTERNAL vendor specific command).
* 
*  Arguments:
*	fdo       - Pointer to the device object for this instance of an Ezusb Device
*       hexRecord - Pointer to an array of INTEL_HEX_RECORD structures.
*                   This array is terminated by an Intel Hex End record (Type = 1).
* 
*  Return Value:
*		STATUS_SUCCESS if successful,
*		STATUS_UNSUCCESSFUL otherwise
****************************************************************************/
bool Ezusb_DownloadIntelHex(IOUSBDeviceRef device, PINTEL_HEX_RECORD hexRecord)
{
    PINTEL_HEX_RECORD ptr = hexRecord;
    UInt16 bufSize;
    IOReturn status;
    UInt8 bmreqType = USBMakeBMRequestType(kUSBOut, kUSBVendor, kUSBDevice);

    //
    // The download must be performed in two passes.  The first pass loads all of the
    // external addresses, and the 2nd pass loads to all of the internal addresses.
    // why?  because downloading to the internal addresses will probably wipe out the firmware
    // running on the device that knows how to receive external ram downloads.
    //
    //
    // First download all the records that go in external ram
    //
    while (ptr->Type == 0) {
        if (!INTERNAL_RAM(ptr->Address)) {
#if VERBOSE
            printf("Downloading %d bytes to external 0x%x\n", ptr->Length, ptr->Address);
#endif

            bufSize = ptr->Length;
            status = IOUSBDeviceRequest(device, bmreqType, ANCHOR_LOAD_EXTERNAL, ptr->Address,
                                        0, (void *) ptr->Data, &bufSize);

            if (status != kIOReturnSuccess)
               return status;
        }
        ptr++;
    }

    //
    // Now download all of the records that are in internal RAM.  Before starting
    // the download, stop the 8051.
    //
    Ezusb_8051Reset(device, 1);
    ptr = hexRecord;
    while (ptr->Type == 0) {
        if (INTERNAL_RAM(ptr->Address)) {
#if VERBOSE
            printf("Downloading %d bytes to internal 0x%x\n", ptr->Length, ptr->Address);
#endif
            bufSize = ptr->Length;
            status = IOUSBDeviceRequest(device, bmreqType, ANCHOR_LOAD_INTERNAL, ptr->Address,
                                        0, (void *) ptr->Data, &bufSize);

            if (status != kIOReturnSuccess)
                return status;
        }
        ptr++;
    }
    
    return status;
}

/*
****************************************************************************
*  Routine Description:
*     Initializes a given instance of the Ezusb Device on the USB.
*
*  Arguments:
*     device - Pointer to the device object for this instance of a
*                      Ezusb Device.
*
****************************************************************************/
IOReturn Ezusb_StartDevice(IOUSBDeviceRef device)
{
    IOReturn status;
#if VERBOSE
    printf("enter Ezusb_StartDevice\n");
#endif

    //-----	First download loader firmware.  The loader firmware 
    //		implements a vendor-specific command that will allow us 
    //		to anchor load to external ram.
    //
#if VERBOSE
    printf("downloading loader\n");
#endif
    status = Ezusb_8051Reset(device, 1);
    status = Ezusb_DownloadIntelHex(device, loader);
    status = Ezusb_8051Reset(device, 0);

    //-----	Now download the device firmware.  //
#if VERBOSE
    printf("downloading firmware\n");
#endif
    status = Ezusb_DownloadIntelHex(device, firmware);
    status = Ezusb_8051Reset(device, 1);
    status = Ezusb_8051Reset(device, 0);

#if VERBOSE
    printf("exit Ezusb_StartDevice (%x)\n", 0);
#endif

    return status;
}

// to pass in the application firmware
void Ezusb_setFirmware(PINTEL_HEX_RECORD newFirmware)
{
    firmware = newFirmware;
}

// Determine the interface the EZUSB device responds to, create the device for Vendor
// connection and return it ready for downloading to.
IOUSBDeviceRef Ezusb_FindDevice(unsigned int vendorID, unsigned int coldBootProductID)
{
    EZUSBInterfaceLocator interfaceLocator;
    IOUSBMatch match;
    
    match.usbClass    = kIOUSBAnyClass;
    match.usbSubClass = kIOUSBAnySubClass;
    match.usbProtocol = kIOUSBAnyProtocol;
    match.usbVendor   = vendorID;
    match.usbProduct  = coldBootProductID;
    
    EZUSBdevice = NULL;
    interfaceLocator.FindDevices(&match);
#if VERBOSE
    printf("ezusb device = %u\n", EZUSBdevice);
#endif
    return EZUSBdevice;
}


// $Id: EZLoader.cpp,v 1.6 2001/10/26 20:22:54 leigh Exp $
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
#include <CoreFoundation/CFNumber.h>
#include <mach/mach_port.h>


// Include file for the Ezusb Device
//
#include "EZLoader.h"

#define VERBOSE (DEBUG && 0)

// 0 is the standard USB interface which we need to download to/on.
#define kTheInterfaceToUse	0	

// these files contain images of the device firmware
//
extern INTEL_HEX_RECORD loader[];

void EZUSBLoader::GetInterfaceToUse(IOUSBDeviceInterface **device, 
                                    UInt8 &outInterfaceNumber,
                                    UInt8 &outAltSetting)
{
    outInterfaceNumber = kTheInterfaceToUse;
    outAltSetting = 0;
}

//
// Once we have found the interface, we need to remember the device interface and
// return true to have the interface kept open.
//
bool EZUSBLoader::FoundInterface(io_service_t ioDevice,
                                 io_service_t ioInterface,
                                 IOUSBDeviceInterface **device,
                                 IOUSBInterfaceInterface **interface,
                                 UInt16 devVendor,
                                 UInt16 devProduct,
                                 UInt8 interfaceNumber,
				 UInt8 altSetting)
{
    ezUSBDevice = device;
#if VERBOSE
    printf("yep found it, leaving open = %d\n", usbLeaveOpenWhenFound);
#endif
    return usbLeaveOpenWhenFound;
}

//
// Return YES if this is the device we are looking for.
//
bool EZUSBLoader::MatchDevice(IOUSBDeviceInterface **device,
                             UInt16 devVendor,
                             UInt16 devProduct)
{
    return devVendor == usbVendor && devProduct == usbProduct;
}

//
// Set the EZUSB device to respond to, and create the device for vendor connection.
//
bool EZUSBLoader::FindVendorsProduct(UInt16 vendorID, 
                                     UInt16 productID,
                                     bool leaveOpenWhenFound)
{
    kern_return_t kr;
    mach_port_t masterPort;
    CFMutableDictionaryRef  matchingDict;
    
    usbVendor   = vendorID;
    usbProduct  = productID;
    usbLeaveOpenWhenFound = leaveOpenWhenFound;
    ezUSBDevice = NULL;  // Used to indicate we have found the device    
#if VERBOSE
    printf("Finding ezusb vendor = 0x%x, product = 0x%x\n", usbVendor, usbProduct);
#endif
    
    //Create a master port for communication with the I/O Kit
    kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (kr || !masterPort) {
        printf("ERR: Couldn’t create a master I/O Kit port(%08x)\n", kr);
        return false;
    }
    // Set up matching dictionary for class IOUSBDevice and its subclasses
    matchingDict = IOServiceMatching(kIOUSBDeviceClassName);
    
    if (!matchingDict) {
        printf("Couldn’t create a USB matching dictionary\n");
        mach_port_deallocate(mach_task_self(), masterPort);
        return -1;
    }
    
    // Add the vendor and product IDs to the matching dictionary.
    // This is the second key in the table of device-matching keys of the USB Common Class Specification
    CFDictionarySetValue(matchingDict, CFSTR(kUSBVendorName),
                         CFNumberCreate(kCFAllocatorDefault,
                                        kCFNumberSInt32Type, &usbVendor));
    
    CFDictionarySetValue(matchingDict, CFSTR(kUSBProductName),
                         CFNumberCreate(kCFAllocatorDefault,
                                        kCFNumberSInt32Type, &usbProduct));
#if VERBOSE
    printf("Finished scanning devices\n");
#endif    
    return ezUSBDevice != NULL;
}

// constructor doing very little.
EZUSBLoader::EZUSBLoader()
{
    usbVendor = 0xFFFF;
    usbProduct = 0xFFFF;
    ezUSBDevice = NULL;
}

// And a destructor doing even less.
//EZUSBLoader::~EZUSBLoader()
//{
//}

/*++

Routine Description:
   Uses the ANCHOR LOAD vendor specific command to either set or release the
   8051 reset bit in the EZ-USB chip.

Arguments:
   device - pointer to the device object for this instance of an Ezusb Device
   resetBit - 1 sets the 8051 reset bit (holds the 8051 in reset)
              0 clears the 8051 reset bit (8051 starts running)
              
Return Value:
   kIOReturnSuccess if we reset correctly.
--*/
IOReturn EZUSBLoader::Reset8051(IOUSBDeviceInterface **device, unsigned char resetBit)
{
    IOReturn status;
    IOUSBDevRequest resetRequest;
    
#if VERBOSE
    printf("setting 8051 reset bit to %d\n", resetBit);
#endif
    resetRequest.bmRequestType = USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice);
    resetRequest.bRequest = ANCHOR_LOAD_INTERNAL;
    resetRequest.wValue = CPUCS_REG;
    resetRequest.wIndex = 0;
    resetRequest.wLength = 1;
    resetRequest.pData = (void *) &resetBit;
    status = (*device)->DeviceRequest(device, &resetRequest);
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
bool EZUSBLoader::DownloadIntelHex(IOUSBDeviceInterface **device, PINTEL_HEX_RECORD hexRecord)
{
    PINTEL_HEX_RECORD ptr = hexRecord;
    IOReturn status = kIOReturnError;
    UInt8 bmreqType = USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice);

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
            IOUSBDevRequest loadExternalRequest;
#if VERBOSE
            printf("Downloading %d bytes to external 0x%x\n", ptr->Length, ptr->Address);
#endif
            loadExternalRequest.bmRequestType = bmreqType;
            loadExternalRequest.bRequest = ANCHOR_LOAD_EXTERNAL;
            loadExternalRequest.wValue = ptr->Address;
            loadExternalRequest.wIndex = 0;
            loadExternalRequest.wLength = ptr->Length;
            loadExternalRequest.pData = (void *) ptr->Data;
            status = (*device)->DeviceRequest(device, &loadExternalRequest);
            if (status != kIOReturnSuccess)
               return status;
        }
        ptr++;
    }

    //
    // Now download all of the records that are in internal RAM.  Before starting
    // the download, stop the 8051.
    //
    Reset8051(device, 1);
    ptr = hexRecord;
    while (ptr->Type == 0) {
        if (INTERNAL_RAM(ptr->Address)) {
            IOUSBDevRequest loadInternalRequest;
#if VERBOSE
            printf("Downloading %d bytes to internal 0x%x\n", ptr->Length, ptr->Address);
#endif
            loadInternalRequest.bmRequestType = bmreqType;
            loadInternalRequest.bRequest = ANCHOR_LOAD_INTERNAL;
            loadInternalRequest.wValue = ptr->Address;
            loadInternalRequest.wIndex = 0;
            loadInternalRequest.wLength = ptr->Length;
            loadInternalRequest.pData = (void *) ptr->Data;
            status = (*device)->DeviceRequest(device, &loadInternalRequest);
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
IOReturn EZUSBLoader::StartDevice()
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
    status = Reset8051(ezUSBDevice, 1);
    status = DownloadIntelHex(ezUSBDevice, loader);
    status = Reset8051(ezUSBDevice, 0);

    //-----	Now download the device firmware.  //
#if VERBOSE
    printf("downloading firmware\n");
#endif
    status = DownloadIntelHex(ezUSBDevice, firmware);
    status = Reset8051(ezUSBDevice, 1);
    status = Reset8051(ezUSBDevice, 0);

#if VERBOSE
    printf("exit Ezusb_StartDevice (%x)\n", 0);
#endif

    return status;
}

// to pass in the application firmware
void EZUSBLoader::setFirmware(PINTEL_HEX_RECORD newFirmware)
{
    firmware = newFirmware;
}


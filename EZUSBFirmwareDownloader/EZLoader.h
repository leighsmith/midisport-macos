// $Id: EZLoader.h,v 1.1.1.1 2000/10/22 02:22:32 leigh Exp $
//
// MacOS X standalone firmware downloader for the EZUSB device, 
// as found in MIDIMan MIDISPORT boxes.
//
// This is a slight variation on EZLOADER.H which was supplied example code with the EZUSB device.
// The Win32 specifics have been removed.
//
// Modifications By Leigh Smith <leigh@tomandandy.com>
//
// Modifications Copyright (c) 2000 tomandandy music inc. All Rights Reserved.
// Permission is granted to use and modify this code for commercial and
// non-commercial purposes so long as the author attribution and this
// copyright message remains intact and accompanies all derived code.
//
//////////////////////////////////////////////////////////////////////
//
// File:      ezloader.h
// $Archive: /EZUSB/ezloader/ezloader.h $
//
// Purpose:
//    Header file for the Ezloader device driver
//
// Environment:
//    kernel mode
//
// $Author: leigh $
//
// $History: ezloader.h $           
//  
//  *****************  Version 2  *****************
//  User: Markm        Date: 4/10/98    Time: 2:06p
//  Updated in $/EZUSB/ezloader
//  Support for downloading Intel Hex
//  
//  *****************  Version 1  *****************
//  User: Markm        Date: 2/24/98    Time: 5:26p
//  Created in $/EZUSB/ezloader
//  
// Copyright (c) 1997 Anchor Chips, Inc.  May not be reproduced without
// permission.  See the license agreement for more details.
//
//////////////////////////////////////////////////////////////////////

#include <stdarg.h>
#include <stdio.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>

#ifndef _BYTE_DEFINED
#define _BYTE_DEFINED
typedef unsigned char BYTE;
#endif // !_BYTE_DEFINED

#ifndef _WORD_DEFINED
#define _WORD_DEFINED
typedef unsigned short WORD;
#endif // !_WORD_DEFINED

//
// Vendor specific request code for Anchor Upload/Download
//
// This one is implemented in the core
//
#define ANCHOR_LOAD_INTERNAL  0xA0

//
// This command is not implemented in the core.  Requires firmware
//
#define ANCHOR_LOAD_EXTERNAL  0xA3

//
// This is the highest internal RAM address for the AN2131Q
//
#define MAX_INTERNAL_ADDRESS  0x1B3F

#define INTERNAL_RAM(address) ((address <= MAX_INTERNAL_ADDRESS) ? 1 : 0)

//
// EZ-USB Control and Status Register.  Bit 0 controls 8051 reset
//
#define CPUCS_REG    0x7F92

#define MAX_INTEL_HEX_RECORD_LENGTH 16

typedef struct _INTEL_HEX_RECORD
{
   BYTE  Length;
   WORD  Address;
   BYTE  Type;
   BYTE  Data[MAX_INTEL_HEX_RECORD_LENGTH];
} INTEL_HEX_RECORD, *PINTEL_HEX_RECORD;

IOReturn Ezusb_StartDevice(IOUSBDeviceRef device);
IOUSBDeviceRef Ezusb_FindDevice(unsigned int vendorID, unsigned int coldBootProductID);
void Ezusb_setFirmware(PINTEL_HEX_RECORD firmware);
void Ezusb_setLoader(PINTEL_HEX_RECORD loader);



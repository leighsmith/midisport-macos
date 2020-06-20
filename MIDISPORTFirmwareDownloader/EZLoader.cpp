//
// MacOS X standalone firmware downloader for the EZUSB device, 
// as found in MIDIMan MIDISPORT boxes.
//
// This is the code for downloading any firmware to pre-renumerated EZUSB devices.
// This is a rewrite of EZLOADER.C which was supplied example code with the EZUSB device.
//
// By Leigh Smith <leigh@leighsmith.com>
//

#include "EZLoader.h"
#include <iostream>
#include <map>

#define VERBOSE (DEBUG && 1)

// 0 is the standard USB interface which we need to download to/on.
#define kTheInterfaceToUse	0	

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
    usbVendorFound = devVendor;
    usbProductFound = devProduct;
#if VERBOSE
    std::cout << "Found " << usbVendorFound << ", " << usbProductFound << ", leaving open = " << usbLeaveOpenWhenFound << std::endl;
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
    // check the vendor, then look for the devProduct in the hardware configuration.
    return devVendor == usbVendorToSearchFor && deviceList.find(devProduct) != deviceList.end();
}

//
// Set the EZUSB device to respond to, and create the device for vendor connection.
//
bool EZUSBLoader::FindVendorsProduct(UInt16 vendorID, 
                                     UInt16 productID,
                                     bool leaveOpenWhenFound)
{
    usbVendorToSearchFor = vendorID;
    usbProductFound  = productID;
    usbLeaveOpenWhenFound = leaveOpenWhenFound;
    ezUSBDevice = NULL;  // Used to indicate we have found the device
#if VERBOSE
    std::cout << "Finding ezusb vendor = 0x" << std::hex << usbVendorToSearchFor << ", product = 0x" << usbProductFound << std::endl;
#endif
    ScanDevices();  // Start the scanning of the devices.
#if VERBOSE
    std::cout << "Finished scanning device" << std::endl;
#endif    
    return ezUSBDevice != NULL;
}

EZUSBLoader::EZUSBLoader(UInt16 newUSBVendor, DeviceList newDeviceList)
{
    // TODO do we need to construct the superclass, passing in CFRunLoopGetCurrent()?
    deviceList = newDeviceList;
    usbVendorToSearchFor = newUSBVendor;
    usbVendorFound = 0xFFFF;
    usbProductFound = 0xFFFF;
    ezUSBDevice = NULL;
}

// TODO destructor to remove the deviceList.
//EZUSBLoader::~EZUSBLoader()
//{
//}

//
// Uses the ANCHOR LOAD vendor specific command to either set or release the
// 8051 reset bit in the EZ-USB chip.
//
// Arguments:
//   device - pointer to the device object for this instance of an Ezusb Device
//   resetBit - 1 sets the 8051 reset bit (holds the 8051 in reset)
//              0 clears the 8051 reset bit (8051 starts running)
//
// Returns: kIOReturnSuccess if we reset correctly.
//
IOReturn EZUSBLoader::Reset8051(IOUSBDeviceInterface **device, unsigned char resetBit)
{
    IOReturn status;
    IOUSBDevRequest resetRequest;
    
#if VERBOSE
    std::cout << "Setting 8051 reset bit to " << int(resetBit) << std::endl;
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

IOReturn EZUSBLoader::DownloadFirmwareToRAM(IOUSBDeviceInterface **device,
                                            std::vector<INTEL_HEX_RECORD> firmware,
                                            bool internalRAM)
{
    IOReturn status = kIOReturnSuccess;
    UInt8 bmreqType = USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice);

    for(std::vector<INTEL_HEX_RECORD>::iterator hexRecord = firmware.begin(); hexRecord != firmware.end() && hexRecord->Type == 0; ++hexRecord) {
        if ((internalRAM && INTERNAL_RAM_ADDRESS(hexRecord->Address)) || (!internalRAM && !INTERNAL_RAM_ADDRESS(hexRecord->Address))) {
            IOUSBDevRequest loadRequest;
            
#if VERBOSE
            std::string RAMname = internalRAM ? "internal" : "external";
            std::cout << "Downloading " << std::dec << int(hexRecord->Length) <<
                         " bytes to " << RAMname << " 0x" << std::hex << hexRecord->Address << std::endl;
#endif
            loadRequest.bmRequestType = bmreqType;
            loadRequest.bRequest = internalRAM ? ANCHOR_LOAD_INTERNAL : ANCHOR_LOAD_EXTERNAL;
            loadRequest.wValue = hexRecord->Address;
            loadRequest.wIndex = 0;
            loadRequest.wLength = hexRecord->Length;
            loadRequest.pData = (void *) hexRecord->Data;
            status = (*device)->DeviceRequest(device, &loadRequest);
            if (status != kIOReturnSuccess)
                return status;
        }
    }
    return status;
}
//
//	This function downloads Intel Hex Records to the EZ-USB device.
//	If any of the hex records are destined for external RAM, then
//	the caller must have previously downloaded firmware to the device
//	that knows how to download to external RAM (ie. firmware that
//	implements the ANCHOR_LOAD_EXTERNAL vendor specific command).
//
//  Arguments:
//	device   - Pointer to the IOUSBDeviceInterface instance of an Ezusb Device
//  firmware - Vector of INTEL_HEX_RECORD structures.
//             This array is terminated by an Intel Hex End record (Type = 1).
//  Returns: true if successful, false otherwise
//
bool EZUSBLoader::DownloadFirmware(IOUSBDeviceInterface **device, std::vector<INTEL_HEX_RECORD> firmware)
{
    IOReturn status;

    // The download must be performed in two passes.  The first pass loads all of the
    // external addresses, and the 2nd pass loads to all of the internal addresses.
    // why? Because downloading to the internal addresses will probably wipe out the firmware
    // running on the device that knows how to receive external RAM downloads.
    //
    // First download all the records that go in external RAM
    status = EZUSBLoader::DownloadFirmwareToRAM(device, firmware, false);
    if (status != kIOReturnSuccess)
        return false;

    // Now download all of the records that are in internal RAM.
    // Before starting the download, stop the 8051.
    Reset8051(device, 1);
    status = EZUSBLoader::DownloadFirmwareToRAM(device, firmware, true);
    return status == kIOReturnSuccess;
}

//
// Initializes a given instance of the EZUSB Device on the USB
// and downloads the application firmware.
//
bool EZUSBLoader::StartDevice(std::vector<INTEL_HEX_RECORD> applicationFirmware)
{
#if VERBOSE
    std::cout << "enter EZUSBLoader::StartDevice" << std::endl;
#endif

    //-----	First download loader firmware.  The loader firmware 
    //		implements a vendor-specific command that will allow us 
    //		to anchor load to external RAM.
    //
#if VERBOSE
    std::cout << "Downloading bootstrap loader." << std::endl;
#endif
    if (Reset8051(ezUSBDevice, 1) != kIOReturnSuccess)
        return false;

    if (!DownloadFirmware(ezUSBDevice, loader)) {
        std::cout << "Failed to download bootstrap loader." << std::endl;
        return false;
    }
    if (Reset8051(ezUSBDevice, 0) != kIOReturnSuccess)
        return false;
    
    //-----	Now download the device firmware.  //
#if VERBOSE
    std::cout << "Downloading application firmware." << std::endl;
#endif
    if (!DownloadFirmware(ezUSBDevice, applicationFirmware)) {
        std::cout << "Failed to download application firmware." << std::endl;
        return false;
    }
    if (Reset8051(ezUSBDevice, 1) != kIOReturnSuccess)
        return false;
    if (Reset8051(ezUSBDevice, 0) != kIOReturnSuccess)
        return false;
#if VERBOSE
    std::cout << "Exit EZUSBLoader::StartDevice." << std::endl;
#endif

    return true;
}

//
// Sets the application firmware to be downloaded next.
//
void EZUSBLoader::SetApplicationLoader(std::vector<INTEL_HEX_RECORD> newLoader)
{
    loader = newLoader;
}

//
// MacOS X standalone firmware downloader for the EZUSB device, 
// as found in MIDIMan MIDISPORT boxes.
//
// This is the code for downloading any firmware to pre-renumerated EZUSB devices.
// This is a rewrite of EZLOADER.C which was supplied example code with the EZUSB device.
//
// By Leigh Smith <leigh@leighsmith.com>
//

#include <fstream>
#include <iostream>
#include "EZLoader.h"

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
#if VERBOSE
    std::cout << "yep found it, leaving open = " << usbLeaveOpenWhenFound << std::endl;
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
    usbVendor   = vendorID;
    usbProduct  = productID;
    usbLeaveOpenWhenFound = leaveOpenWhenFound;
    ezUSBDevice = NULL;  // Used to indicate we have found the device
#if VERBOSE
    std::cout << "Finding ezusb vendor = 0x" << std::hex << usbVendor << ", product = 0x" << usbProduct << std::endl;
#endif
    ScanDevices();  // Start the scanning of the devices.
#if VERBOSE
    std::cout << "Finished scanning device" << std::endl;
#endif    
    return ezUSBDevice != NULL;
}

// constructor doing very little.
EZUSBLoader::EZUSBLoader()
{
    // TODO do we need to construct the superclass, passing in CFRunLoopGetCurrent()?
    usbVendor = 0xFFFF;
    usbProduct = 0xFFFF;
    ezUSBDevice = NULL;
}

// And a destructor doing even less.
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
    std::cout << "setting 8051 reset bit to " << int(resetBit) << std::endl;
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
    IOReturn status = kIOReturnError;
    UInt8 bmreqType = USBmakebmRequestType(kUSBOut, kUSBVendor, kUSBDevice);

    //
    // The download must be performed in two passes.  The first pass loads all of the
    // external addresses, and the 2nd pass loads to all of the internal addresses.
    // why? Because downloading to the internal addresses will probably wipe out the firmware
    // running on the device that knows how to receive external RAM downloads.
    //
    // First download all the records that go in external RAM
    //
    for(std::vector<INTEL_HEX_RECORD>::iterator hexRecord = firmware.begin(); hexRecord != firmware.end() && hexRecord->Type == 0; ++hexRecord) {
        if (!INTERNAL_RAM(hexRecord->Address)) {
            IOUSBDevRequest loadExternalRequest;
#if VERBOSE
            std::cout << "Downloading " << std::dec << int(hexRecord->Length) << " bytes to external 0x" << std::hex << hexRecord->Address << std::endl;
#endif
            loadExternalRequest.bmRequestType = bmreqType;
            loadExternalRequest.bRequest = ANCHOR_LOAD_EXTERNAL;
            loadExternalRequest.wValue = hexRecord->Address;
            loadExternalRequest.wIndex = 0;
            loadExternalRequest.wLength = hexRecord->Length;
            loadExternalRequest.pData = (void *) hexRecord->Data;
            status = (*device)->DeviceRequest(device, &loadExternalRequest);
            if (status != kIOReturnSuccess)
               return status;
        }
    }

    //
    // Now download all of the records that are in internal RAM.
    // Before starting the download, stop the 8051.
    //
    Reset8051(device, 1);
    for(std::vector<INTEL_HEX_RECORD>::iterator hexRecord = firmware.begin(); hexRecord != firmware.end() && hexRecord->Type == 0; ++hexRecord) {
        if (INTERNAL_RAM(hexRecord->Address)) {
            IOUSBDevRequest loadInternalRequest;
#if VERBOSE
            std::cout << "Downloading " << std::dec << int(hexRecord->Length) << " bytes to internal 0x" << std::hex << hexRecord->Address << std::endl;
#endif
            loadInternalRequest.bmRequestType = bmreqType;
            loadInternalRequest.bRequest = ANCHOR_LOAD_INTERNAL;
            loadInternalRequest.wValue = hexRecord->Address;
            loadInternalRequest.wIndex = 0;
            loadInternalRequest.wLength = hexRecord->Length;
            loadInternalRequest.pData = (void *) hexRecord->Data;
            status = (*device)->DeviceRequest(device, &loadInternalRequest);
            if (status != kIOReturnSuccess)
                return status;
        }
    }
    return status;
}

//
// Initializes a given instance of the Ezusb Device on the USB.
//
IOReturn EZUSBLoader::StartDevice()
{
    IOReturn status;
#if VERBOSE
    std::cout << "enter EZUSBLoader::StartDevice" << std::endl;
#endif

    //-----	First download loader firmware.  The loader firmware 
    //		implements a vendor-specific command that will allow us 
    //		to anchor load to external ram.
    //
#if VERBOSE
    std::cout << "downloading loader" << std::endl;
#endif
    status = Reset8051(ezUSBDevice, 1);
    status = DownloadFirmware(ezUSBDevice, loader);
    status = Reset8051(ezUSBDevice, 0);

    //-----	Now download the device firmware.  //
#if VERBOSE
    std::cout << "downloading application firmware" << std::endl;
#endif
    status = DownloadFirmware(ezUSBDevice, applicationFirmware);
    status = Reset8051(ezUSBDevice, 1);
    status = Reset8051(ezUSBDevice, 0);

#if VERBOSE
    std::cout << "exit EZUSBLoader::StartDevice" << std::endl;
#endif

    return status;
}

//
// Sets the application firmware to be downloaded next.
//
void EZUSBLoader::SetApplicationFirmware(std::vector<INTEL_HEX_RECORD> newFirmware)
{
    applicationFirmware = newFirmware;
}

//
// Sets the application firmware to be downloaded next.
//
void EZUSBLoader::SetApplicationLoader(std::vector<INTEL_HEX_RECORD> newLoader)
{
    loader = newLoader;
}

//
// Reads the Intel Hex File from fileName into the firmware memory structure,
// suitable for downloading.
// Returns true if able to load the file, false if there was format error.
//
bool EZUSBLoader::ReadFirmwareFromHexFile(std::string fileName, std::vector<INTEL_HEX_RECORD> &firmware)
{
    // Open the text file
    std::ifstream hexFile(fileName);
    std::string hexLine;
    // Read each line of the file.
    while (std::getline(hexFile, hexLine)) {
        INTEL_HEX_RECORD hexRecord;
        int calculatedChecksum = 0;

        {   // Skip over comment lines.
            int cursor = 0;
            while (cursor < hexLine.length() && hexLine[cursor] == ' ')
                cursor++;
            if (hexLine[cursor] == '#')
                continue;
        }
        // verify ':' is the first character.
        if (hexLine[0] != ':') {
            std::cerr << "Missing ':' as first character on line, not an Intel hex file?" << std::endl;
            return false;
        }
        // read and convert the next two characters as Length
        hexRecord.Length = stoi(hexLine.substr(1, 2), NULL, 16);
        calculatedChecksum += hexRecord.Length;
        hexRecord.Address = stoi(hexLine.substr(3, 4), NULL, 16);
        calculatedChecksum += ((hexRecord.Address >> 8) & 0xff) + (hexRecord.Address & 0xff);
        hexRecord.Type = stoi(hexLine.substr(7, 2), NULL, 16);
        calculatedChecksum += hexRecord.Type;
        if (hexRecord.Length <= MAX_INTEL_HEX_RECORD_LENGTH) {
            int readLocation = 9;
            int dataIndex = 0;
            while (dataIndex < hexRecord.Length) {
                hexRecord.Data[dataIndex] = stoi(hexLine.substr(readLocation + dataIndex * 2, 2), NULL, 16);
                calculatedChecksum += hexRecord.Data[dataIndex++];
            }
            // Verify the checksum, calculated by summing the values of all hexadecimal digit pairs in the record,
            // modulo 256 and taking the two's complement.
            int checksum = stoi(hexLine.substr(9 + dataIndex * 2, 2), NULL, 16);
            calculatedChecksum = (-calculatedChecksum) & 0xff;
            if (checksum != calculatedChecksum) {
                std::cerr << "Checksum 0x" << std::hex << checksum << " did not match calculated 0x" << calculatedChecksum << std::endl;
                return false;
            }
        }
        else {
            std::cerr << "More bytes on line (" << int(hexRecord.Length) << ") than maximum record length (" << MAX_INTEL_HEX_RECORD_LENGTH << ")" << std::endl;
            return false;
        }
        firmware.push_back(hexRecord);
    }
    return true;
}

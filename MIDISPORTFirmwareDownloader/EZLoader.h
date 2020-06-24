//
// MacOS X standalone firmware downloader for the EZUSB device, 
// as found in MIDIMan/M-Audio MIDISPORT boxes.
//
// This code includes portions of EZLOADER.H which was supplied example code with the EZUSB device.
//

#include <vector>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include "USBUtils.h"
#include "HardwareConfiguration.h"
#include "IntelHexFile.h"

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

#define INTERNAL_RAM_ADDRESS(address) ((address <= MAX_INTERNAL_ADDRESS) ? 1 : 0)

//
// EZ-USB Control and Status Register.  Bit 0 controls 8051 reset
//
#define CPUCS_REG    0x7F92

class EZUSBLoader : public USBDeviceManager {
protected:
    IOReturn Reset8051(IOUSBDeviceInterface **device, unsigned char resetBit);
    IOReturn DownloadFirmwareToRAM(IOUSBDeviceInterface **device, std::vector<INTEL_HEX_RECORD> firmware, bool internalRAM);
    bool DownloadFirmware(IOUSBDeviceInterface **device, std::vector<INTEL_HEX_RECORD> hexRecord);

    // instance variables
    IOUSBDeviceInterface **ezUSBDevice;
    // These hex records that contain the application loader.
    std::vector<INTEL_HEX_RECORD> loader;
    // The devices supported and their firmware paths.
    DeviceList deviceList;
    UInt16 usbVendorToSearchFor;
    UInt16 usbVendorFound;
    UInt16 usbProductFound;
    bool usbLeaveOpenWhenFound;
    bool (*foundDeviceCallback)(EZUSBLoader *instance, struct DeviceFirmware);
public:
    EZUSBLoader(UInt16 newUSBVendor, DeviceList deviceList, bool leaveOpenWhenFound);

//    ~EZUSBLoader();
    virtual bool MatchDevice(IOUSBDeviceInterface **device,
                                          UInt16 devVendor,
                                          UInt16 devProduct);

    virtual void GetInterfaceToUse(IOUSBDeviceInterface **device, 
                                   UInt8 &outInterfaceNumber,
                                   UInt8 &outAltSetting);
    bool FoundInterface(io_service_t ioDevice,
                        io_service_t ioInterface,
                        IOUSBDeviceInterface **device,
                        IOUSBInterfaceInterface **interface,
                        UInt16 devVendor,
                        UInt16 devProduct,
                        UInt8 interfaceNumber,
                        UInt8 altSetting);
    bool FindVendorsProduct(UInt16 vendorID, UInt16 coldBootProductID, bool leaveOpenWhenFound);
    bool StartDevice(std::vector<INTEL_HEX_RECORD> applicationFirmware);
    //
    // Sets the application firmware to be downloaded next.
    //
    void SetApplicationLoader(std::vector<INTEL_HEX_RECORD> newLoader) { loader = newLoader; };

    //
    // Sets the notification callback for when a device is found.
    //
    void SetFoundDeviceNotification(bool (*newFoundDeviceCallback)(EZUSBLoader *instance, struct DeviceFirmware)) {
        foundDeviceCallback = newFoundDeviceCallback;
    };
};

/*
	Copyright (c) 2000 Apple Computer, Inc., All Rights Reserved.

	You may incorporate this Apple sample source code into your program(s) without
	restriction. This Apple sample source code has been provided "AS IS" and the
	responsibility for its operation is yours. You are not permitted to redistribute
	this Apple sample source code as "Apple sample source code" after having made
	changes. If you're going to re-distribute the source, we require that you make
	it clear in the source that the code was descended from Apple sample source
	code, but that you've made changes.
	
	NOTE: THIS IS EARLY CODE, NOT NECESSARILY SUITABLE FOR SHIPPING PRODUCTS.
	IT IS INTENDED TO GIVE HARDWARE DEVELOPERS SOMETHING WITH WHICH TO GET
	DRIVERS UP AND RUNNING AS SOON AS POSSIBLE.
	
	In particular, the implementation is much more complex and ugly than is
	necessary because of limitations of I/O Kit's USB user client code in DP4.
	As I/O Kit evolves, this code will be updated to be much simpler.
*/

#ifndef __MIDISPORTUSBDriver_h__
#define __MIDISPORTUSBDriver_h__

#include "USBMIDIDriverBase.h"

enum WarmFirmwareProductIDs {
    MIDISPORT1x1 = 0x1011,
    MIDISPORT2x2 = 0x1002,
    MIDISPORT4x4 = 0x1021,
    MIDISPORT8x8 = 0x1031
};

class MIDISPORT : public USBMIDIDriverBase {
public:
    MIDISPORT();
    ~MIDISPORT();
    
    // MIDIDriver overrides
/*	virtual OSStatus	EnableSource(MIDIEndpointRef src, Boolean enabled);*/

    // USBMIDIDriverBase overrides
    virtual bool UseDevice(IOUSBDeviceInterface **device,
                                          UInt16 devVendor,
                                          UInt16 devProduct);

    virtual void GetInterfaceToUse(IOUSBDeviceInterface **device, 
                                   UInt8 &outInterfaceNumber,
                                   UInt8 &outAltSetting);

    virtual void FoundDevice(IOUSBDeviceInterface **device,
                             IOUSBInterfaceInterface **interface,
                             UInt16 devVendor,
                             UInt16 devProduct,
                             UInt8 interfaceNumber,
                             UInt8 altSetting,
                             MIDIDeviceListRef deviceList);

    virtual void GetInterfaceInfo(InterfaceState *intf, InterfaceInfo &info);

    virtual void StartInterface(InterfaceState *intf);
    virtual void StopInterface(InterfaceState *intf);
    virtual void HandleInput( InterfaceState *intf, MIDITimeStamp when, Byte *readBuf, ByteCount readBufSize);
    virtual void PrepareOutput(InterfaceState *intf, WriteQueue &writeQueue,
                                 Byte *destBuf1, ByteCount *bufCount1,
                                 Byte *destBuf2, ByteCount *bufCount1);
private:
    int connectedMIDISPORTIndex;
};

#endif // __MIDISPORTUSBDriver_h__

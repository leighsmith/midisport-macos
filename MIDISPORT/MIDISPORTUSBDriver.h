/*

    Portions Copyright (c) 2000 Apple Computer, Inc., All Rights Reserved.

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
    virtual bool MatchDevice(USBDevice *inUSBDevice);

    virtual void GetInterfaceToUse(IOUSBDeviceInterface **device, 
                                   UInt8 &outInterfaceNumber,
                                   UInt8 &outAltSetting);


    virtual MIDIDeviceRef CreateDevice(USBDevice *inUSBDevice,
                                       USBInterface *inUSBInterface);

    virtual void GetInterfaceInfo(InterfaceState *intf, InterfaceInfo &info);

    // pipes are opened, do any extra initialization (send config msgs etc)
    virtual void StartInterface(USBMIDIDevice *usbmDev);

    // pipes are about to be closed, do any preliminary cleanup
    virtual void StopInterface(USBMIDIDevice *usbmDev);

    // a USB message arrived, parse it into a MIDIPacketList and call MIDIReceived
    virtual void HandleInput(USBMIDIDevice *usbmDev,
                             MIDITimeStamp when,
                             Byte *readBuf,
                             ByteCount readBufSize);

    static ByteCount USBMIDIPrepareOutput(USBMIDIDevice *usbmDev,
                                          WriteQueue &writeQueue,
                                          Byte * destBuf,
                                          ByteCount bufSize);


    // overrides of MIDIDriver methods
    virtual OSStatus        Send(                    const MIDIPacketList *pktlist,
                                 void *            endptRef1,
                                 void *            endptRef2);

    // our abstract methods - required overrides


    virtual void            PreExistingDeviceFound(    MIDIDeviceRef    inMIDIDevice,
                                                   USBDevice *        inUSBDevice,
                                                   USBInterface *    inUSBInterface) { }

    virtual USBInterface *    CreateInterface(        USBMIDIDevice *    inDevice) = 0;


    virtual ByteCount        PrepareOutput(            USBMIDIDevice *    usbmDev,
                                           WriteQueue &    writeQueue,
                                           Byte *            destBuf) = 0;
    // dequeue from WriteQueue into a single USB message, return
    // length of the message.  Called with the queue mutex locked.

    virtual USBMIDIDevice *    CreateUSBMIDIDevice(    USBDevice *        inUSBDevice,
                                                   USBInterface *    inUSBInterface,
                                                   MIDIDeviceRef    inMIDIDevice);
    // may override to create a subclass

    // Utilities to implement the USB MIDI class spec methods of encoding MIDI in USB packets
    static void                USBMIDIHandleInput(        USBMIDIDevice *    usbmDev,
                                                  MIDITimeStamp    when,
                                                  Byte *            readBuf,
                                                  ByteCount        bufSize);

private:
    int connectedMIDISPORTIndex;
};

#endif // __MIDISPORTUSBDriver_h__

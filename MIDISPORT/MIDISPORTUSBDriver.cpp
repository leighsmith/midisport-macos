// $Id: MIDISPORTUSBDriver.cpp,v 1.3 2000/11/05 01:09:59 leigh Exp $
//
// MacOS X driver for MIDIMan MIDISPORT 2x2 USB MIDI interfaces.
//
//    So I've made changes :-) This class is specific to the MIDIMAN MIDISPORT USB devices.
//    The most unique thing about these is they require their firmware downloaded before they can
//    operate. When they are first cold booted, their product ID will indicate they are in a pre-download
//    state. The firmware is then downloaded, the device "re-enumerates" itself, such that the ID will
//    then indicate it is an operating MIDI device.
//
// Leigh Smith <leigh@tomandandy.com>
//
// Copyright (c) 2000 tomandandy music inc. All Rights Reserved.
// Permission is granted to use and modify this code for commercial and
// non-commercial purposes so long as the author attribution and this
// copyright message remains intact and accompanies all derived code.
//
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

#include <stddef.h>
#include <stdio.h>
#include <algorithm>
#include "MIDISPORTUSBDriver.h"
#include "USBUtils.h"
#include "EZLoader.h"

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// things to customize

// Unique UUID (Universally Unique Identifier) For the MIDIMAN MIDISPORT 2x2 USB MIDI interface
#define kFactoryUUID CFUUIDGetConstantUUIDWithBytes(NULL, 0x6E, 0x44, 0x3F, 0xE8, 0x9E, 0xF4, 0x11, 0xD4, 0xA2, 0xFE, 0x00, 0x05, 0x02, 0xB6, 0x21, 0x33)
// ########

#define kTheInterfaceToUse	0		// 0 is the interface which we need to access the 5 endpoints.
#define kReadBufSize		32
#define kWriteBufSize		32
#define midimanVendorID		0x0763		// midiman
#define coldBootProductID	0x1001		// product ID indicating the firmware has not been loaded.
#define firmwareProductID	0x1002		// product ID indicating the firmware has been loaded and is working.

// and these
#define kMyBoxName		"MIDISPORT"
#define kMyManufacturerName	"MIDIMAN"
#define kMyModelName		"2x2"

#define kNumPorts		2

#define MIDIPACKETLEN		4		// number of bytes in a dword packet received and sent to the MIDISPORT
#define CMDINDEX		(MIDIPACKETLEN - 1)  // which byte in the packet has the length and port number.

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// This struct is used to match against USB devices, it is shared by the InterfaceLocator and
// MIDISPORTUSBDriver instances.
IOUSBMatch midisportMatch = {
	kIOUSBAnyClass,
	kIOUSBAnySubClass,
	kIOUSBAnyProtocol,
	midimanVendorID,
	firmwareProductID
};

// __________________________________________________________________________________________________

// Implementation of the factory function for this type.
extern "C" void *NewMIDISPORT2x2(CFAllocatorRef allocator, CFUUIDRef typeID);
extern "C" void *NewMIDISPORT2x2(CFAllocatorRef allocator, CFUUIDRef typeID) 
{
	// If correct type is being requested, allocate an
	// instance of TestType and return the IUnknown interface.
	if (CFEqual(typeID, kMIDIDriverTypeID)) {
		MIDISPORT2x2 *result = new MIDISPORT2x2;
		return result->Self();
	} else {
		// If the requested type is incorrect, return NULL.
		return NULL;
	}
}

// __________________________________________________________________________________________________

// Determine if the MIDISPORT is in firmware downloaded or unloaded state.
// If cold booted, we need to download the firmware and restart the device to
// enable the firmware product code to be found.
MIDISPORT2x2::MIDISPORT2x2() :
	USBMIDIDriverBase(kFactoryUUID)
{
    IOUSBDeviceRef ezusbDev;
    IOUSBDeviceRef midisportDev;
    extern INTEL_HEX_RECORD firmware[];
    EZUSBLoader ezusb;
    
    // printf("MIDISPORTUSBDriver init\n");
    ezusbDev = ezusb.FindDevice(midimanVendorID, coldBootProductID);
    if (ezusbDev != NULL) {
        // printf("in cold booted state, downloading firmware\n");
        ezusb.setFirmware(firmware);
        ezusb.StartDevice(ezusbDev);
        // check that we re-enumerated the USB bus properly.
        do { // busy wait = bad!
            midisportDev = ezusb.FindDevice(midimanVendorID, firmwareProductID);
        } while(midisportDev == NULL);
        // printf("Can't find re-enumerated MIDISPORT device, probable failure in downloading firmware.\n");
    }
}

MIDISPORT2x2::~MIDISPORT2x2()
{
  // printf("~MIDISPORTUSBDriver\n");
}

// __________________________________________________________________________________________________

// This class finds interface instances, called from FindDevices()
class InterfaceLocator : public USBDeviceLocator {
public:
	void		Find(MIDIDeviceListRef devices, MIDIDriverRef driver)
	{
		mDeviceList = devices;
		mDriver = driver;
		if(FindDevices(&midisportMatch) != 0)
                    printf("Error finding devices\n");
	}
	
	virtual int	InterfaceIndexToUse(IOUSBDeviceRef device)
	{
		return kTheInterfaceToUse;
	}

	virtual bool	FoundInterface(IOUSBDeviceRef device, XUSBInterface interface)
	{
		MIDIDeviceRef dev;
		MIDIEntityRef ent;
//		IOUSBDeviceDescriptor deviceDesc;
//                IOReturn ret;

                // We want to determine the firmware version.
                // This could be tucked into USBUtils.cpp
//		IOUSBNewDeviceRef(devIter, &device);
//                ret = IOUSBGetDeviceDescriptor(devIter, &deviceDesc, sizeof(deviceDesc));
//                if (ret != kIOReturnSuccess) {
//                    printerr("IOUSBGetDeviceDescriptor", ret);
//                }
		
		MIDIDeviceCreate(mDriver,
			CFSTR(kMyBoxName),
			CFSTR(kMyManufacturerName),
			CFSTR(kMyModelName),
			&dev);

		// make kNumPorts entities with 1 source, 1 destination
		for (int port = 1; port <= kNumPorts; ++port) {
			char portname[64];
			sprintf(portname, "Port %d", port);
			CFStringRef str = CFStringCreateWithCString(NULL, portname, 0);
			MIDIDeviceAddEntity(dev, str, false, 1, 1, &ent);
			CFRelease(str);
		}

		MIDIDeviceListAddDevice(mDeviceList, dev);
		return false;		// don't keep device/interface allocated
	}
private:
	MIDIDeviceListRef	mDeviceList;
	MIDIDriverRef		mDriver;
};

OSStatus MIDISPORT2x2::FindDevices(MIDIDeviceListRef devices)
{
    InterfaceLocator loc;
    
    // Register the device and the MIDISPORTUSBDriver with the interface locator instance, 
    // and search for devices matching the warm boot (firmware product ID and manufacturer ID).
    loc.Find(devices, Self());
    return noErr;
}

IOUSBMatch *MIDISPORT2x2::GetUSBMatch()
{
    return &midisportMatch;
}

int MIDISPORT2x2::InterfaceIndexToUse(IOUSBDeviceRef device)
{
    return kTheInterfaceToUse;
}

// note that we're using bulk endpoint for output; interrupt for input...
void MIDISPORT2x2::GetInterfaceInfo(InterfaceState *intf, InterfaceInfo &info)
{
    info.inEndptType = kUSBInterrupt;
    info.outEndptType = kUSBBulk;
    info.readBufSize = kReadBufSize;
    info.writeBufSize = kWriteBufSize;
}

void MIDISPORT2x2::StartInterface(InterfaceState *intf)
{
    // printf("StartInterface\n");
}

void MIDISPORT2x2::StopInterface(InterfaceState *intf)
{
    // printf("StopInterface\n");
}

// The MIDI bytes are transmitted from the MIDISPORT in little-endian dword (4 byte) "packets",
// these are termed mspackets to avoid confusion with the MIDIServices concept of packet.
// The format of mspackets in received memory order is:
// d0, d1, d2, cmd, d0, d1, d2, cmd, ...
// d0 is typically (but not always!) the status MIDI byte, d1, d2 the subsequent message bytes in order.
// cmd is: 000x00yy
// Where the upper nibble (x) indicates the source MIDI in port, 0=MIDI-IN A, 1=MIDI-IN B.
// The lower nibble (yy) indicates the byte count of valid data in the preceding three bytes.
// A byte count of 0 indicates a null packet and marks the end of the multiplex input buffer
// for transmitting less than a full kReadBufSize of data.
void MIDISPORT2x2::HandleInput(InterfaceState *intf, MIDITimeStamp when, Byte *readBuf, int readBufSize)
{
    int prevInputPort = -1;	                         // signifies none
    static bool inSysex[kNumPorts] = {false, false};     // is it possible to make this mInSysEx?
    static Byte runningStatus[kNumPorts] = {0x90, 0x90}; // we gotta start somewhere... or make it mRunningStatus
    static int remainingBytesInMsg[kNumPorts] = {0, 0};  // how many bytes remain to be processed per MIDI message
    static Byte completeMessage[MIDIPACKETLEN];
    static int numCompleted = 0;
    int preservedMsgCount = 0;	// to preserve the message length when encountering a real-time msg
                                // embedded in another channel message.
    Byte *src = readBuf, *srcend = src + readBufSize;
    static Byte pbuf[512];
    MIDIPacketList *pktlist = (MIDIPacketList *)pbuf;
    MIDIPacket *pkt = MIDIPacketListInit(pktlist);

    for ( ; src < srcend; src += MIDIPACKETLEN) {
        int bytesInPacket = src[CMDINDEX] & 0x03;   // number of valid bytes in a packet.
        int inputPort = src[CMDINDEX] >> 4;

        if (bytesInPacket == 0)	      // Indicates the end of the buffer, early out.
            break;		

        // printf("%c %d: %02X %02X %02X %02X  ", inputPort + 'A', bytesInPacket, src[0], src[1], src[2], src[3]);

        // if input came from a different input port, flush the packet list.
        if (prevInputPort != -1 && inputPort != prevInputPort) {
            MIDIReceived(intf->mSources[inputPort], pktlist);
            pkt = MIDIPacketListInit(pktlist);
            inSysex[inputPort] = false;
        }
        prevInputPort = inputPort;        

        for(int byteIndex = 0; byteIndex < bytesInPacket; byteIndex++) {
            if(src[byteIndex] & 0x80) {   // status was present
                int dataInMessage;
                Byte status = src[byteIndex];
                // running status applies to channel (voice and mode) messages only
                if (status < 0xF0)
                    runningStatus[inputPort] = status;  // remember it, including the MIDI channel...

                dataInMessage = MIDIDataBytes(status);
                // if the message is a single real-time message, save the previous remainingBytesInMsg
                // (since a real-time message can occur within another message) until we have shipped 
                // the real-time packet.
                if(remainingBytesInMsg[inputPort] > 0 && dataInMessage == 0) {
                    preservedMsgCount = remainingBytesInMsg[inputPort];
                }
                else {
                    preservedMsgCount = 0;
                }
                remainingBytesInMsg[inputPort] = dataInMessage;

                if(status == 0xF0) {
                    inSysex[inputPort] = true;
                }
                if(status == 0xF7) {
                    if(numCompleted > 0)
                        pkt = MIDIPacketListAdd(pktlist, sizeof(pbuf), pkt, when, numCompleted, completeMessage);
                    inSysex[inputPort] = false;
                }

                numCompleted = 1;
                // store ready for packetting.
                completeMessage[0] = status;
                // printf("new status %02X, remainingBytesInMsg = %d\n", status, remainingBytesInMsg[inputPort]);
            }
            else if(remainingBytesInMsg[inputPort] > 0) {   // still within a message
                remainingBytesInMsg[inputPort]--;
                // store ready for packetting.
                completeMessage[numCompleted++] = src[byteIndex];
                // printf("in message remainingBytesInMsg = %d\n", remainingBytesInMsg[inputPort]);
            }
            else if(inSysex[inputPort]) {          // fill the packet with sysex bytes
                // printf("in sysex numCompleted = %d\n", numCompleted);
                completeMessage[numCompleted++] = src[byteIndex];
            }
            else {  // assume a running status message, assign status from the retained runnning status.
                Byte status = runningStatus[inputPort];
                // printf("assuming runningStatus %02X\n", status);
                completeMessage[0] = status;
                completeMessage[1] = src[byteIndex];
                numCompleted = 2;
                remainingBytesInMsg[inputPort] = MIDIDataBytes(status) - 1;
                // assert(remainingBytesInMsg[inputPort] > 0); // since System messages are prevented from being running status.
            }

            if(remainingBytesInMsg[inputPort] == 0 || numCompleted >= (MIDIPACKETLEN - 1)) { // completed
                // printf("Shipping a packet: ");
                // for(int i = 0; i < numCompleted; i++)
                //    printf("%02X ", completeMessage[i]);
                pkt = MIDIPacketListAdd(pktlist, sizeof(pbuf), pkt, when, numCompleted, completeMessage);
                numCompleted = 0;
                // printf("shipped\n");
            }
            if(preservedMsgCount != 0) {
                remainingBytesInMsg[inputPort] = preservedMsgCount;
            }
        }
    }
    if (pktlist->numPackets > 0 && prevInputPort != -1) {
        // printf("receiving %ld packets\n", pktlist->numPackets);
        MIDIReceived(intf->mSources[prevInputPort], pktlist);
        // printf("\n");
    }
}

// WriteQueue is an STL list of VLMIDIPacket's to be transmitted, presumably containing
// at least one element.
// Fill one USB buffer, destBuf, with a size of bufSize, with outgoing data in USB-MIDI format.
// Return the number of bytes written.
int MIDISPORT2x2::PrepareOutput(InterfaceState *intf, WriteQueue &writeQueue, Byte *destBuf)
{
    Byte *dest = destBuf, *destend = dest + kWriteBufSize;
    
    while (true) {
        if (writeQueue.empty()) {
            // printf("dest buffer = ");
            // for(int i = 0; i < dest - destBuf; i++)
            // 	printf("%02X ", destBuf[i]);
            // printf("\n");
            memset(dest, 0, MIDIPACKETLEN);  // signal the conclusion with a single null packet
            return kWriteBufSize;	// dest - destBuf;
        }
                
        WriteQueueElem *wqe = &writeQueue.front();
        Byte cableNibble = wqe->portNum << 4;
        VLMIDIPacket *pkt = wqe->packet;
        Byte *src = pkt->data + wqe->bytesSent;
        Byte *srcend = &pkt->data[pkt->length];

        while (src < srcend && dest < destend) {
            int outPacketLen;
            Byte c = *src++;
            
            switch (c >> 4) {
            case 0x0: case 0x1: case 0x2: case 0x3:
            case 0x4: case 0x5: case 0x6: case 0x7:
                // printf("databyte %02X\n", c);
                // data byte, presumably a sysex continuation
                *dest++ = c;
                // sysex ends with preceding 2 bytes or sysex continues 
                outPacketLen = (pkt->length >= 3) ? 2 : pkt->length - 1;	
                
                memcpy(dest, src, outPacketLen);
                memset(dest + outPacketLen, 0, 2 - outPacketLen);
                dest[2] = cableNibble | (outPacketLen + 1); // mark length and cable
                dest += 3;
                src += outPacketLen;
                break;
            case 0x8:	// note-on
            case 0x9:	// note-off
            case 0xA:	// poly pressure
            case 0xB:	// control change
            case 0xE:	// pitch bend
                // printf("channel %02X\n", c);
                *dest++ = c;
                *dest++ = *src++;
                *dest++ = *src++;
                *dest++ = cableNibble | 0x03;
                break;
            case 0xC:	// program change
            case 0xD:	// mono pressure
                // printf("prch,pres %02X\n", c);
                *dest++ = c;
                *dest++ = *src++;
                *dest++ = 0;
                *dest++ = cableNibble | 0x02;
                break;
            case 0xF:	// system message
                // printf("system %02X\n", c);
                switch (c) {
                case 0xF0:	// sysex start
                    *dest++ = c;
                    *dest++ = *src++;
                    *dest++ = *src++;
                    *dest++ = cableNibble | 0x03;	// sysex start or continued
                    break;
                case 0xF6:	// tune request (0)
                case 0xF7:	// sysex conclude (0)
                case 0xF8:	// clock
                case 0xFA:	// start
                case 0xFB:	// continue
                case 0xFC:	// stop
                case 0xFE:	// active sensing
                case 0xFF:	// system reset
                    *dest++ = c;
                    *dest++ = 0;
                    *dest++ = 0;
                    *dest++ = cableNibble | 0x01;      // 1-byte system realtime or system common
                    break;
                case 0xF1:	// MTC (1)
                case 0xF3:	// song select (1)
                    *dest++ = c;
                    *dest++ = *src++;
                    *dest++ = 0;
                    *dest++ = cableNibble | 0x02;	// 2-byte system common
                    break;
                case 0xF2:	// song pointer (2)
                    *dest++ = c;
                    *dest++ = *src++;
                    *dest++ = *src++;
                    *dest++ = cableNibble | 0x03;	// 3-byte system common
                    break;
                default:
                    // printf("unknown %02X\n", c);
                    // unknown MIDI message! advance until we find a status byte
                    while (src < srcend && *src < 0x80)
                        ++src;
                    break;
                }
                break;
            }

            if (src == srcend) {
                // source packet completely sent
                delete wqe->packet;
                writeQueue.pop_front();
            }
            else
                wqe->bytesSent = src - pkt->data;

            if (dest > destend - 4) {
                // destBuf completely filled
                return dest - destBuf;
            }
            // we didn't fill the output buffer, is there more source data in the write queue?
        }
    }
}

// $Id: MIDISPORTUSBDriver.cpp,v 1.9 2001/03/29 22:33:47 leigh Exp $
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
*/

#include <stddef.h>
#include <stdio.h>
#include <algorithm>
#include "MIDISPORTUSBDriver.h"
#include "USBUtils.h"
#include "EZLoader.h"

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// things to customize

// Unique UUID (Universally Unique Identifier) For the MIDIMAN MIDISPORT USB MIDI interface driver
#define kFactoryUUID CFUUIDGetConstantUUIDWithBytes(NULL, 0x6E, 0x44, 0x3F, 0xE8, 0x9E, 0xF4, 0x11, 0xD4, 0xA2, 0xFE, 0x00, 0x05, 0x02, 0xB6, 0x21, 0x33)
// ########

#define kTheInterfaceToUse	0		// 0 is the interface which we need to access the 5 endpoints.
#define kReadBufSize		32
#define kWriteBufSize		32
#define midimanVendorID		0x0763		// midiman

// and these
#define kMyBoxName		"MIDISPORT"
#define kMyManufacturerName	"MIDIMAN"

#define kNumMaxPorts		8

#define MIDIPACKETLEN		4		// number of bytes in a dword packet received and sent to the MIDISPORT
#define CMDINDEX		(MIDIPACKETLEN - 1)  // which byte in the packet has the length and port number.

#define DEBUG_OUTBUFFER		0		// 1 to printout whenever a msg is to be sent.
#define VERBOSE (DEBUG && 0)

enum WarmFirmwareProductIDs {
    MIDISPORT1x1 = 0x1011,
    MIDISPORT2x2 = 0x1002,
    MIDISPORT4x4 = 0x1021,
    MIDISPORT8x8 = 0x1031
};

extern INTEL_HEX_RECORD firmware1x1[];
extern INTEL_HEX_RECORD firmware2x2[];
extern INTEL_HEX_RECORD firmware4x4[];

struct HardwareConfigurationDescription {
    WarmFirmwareProductIDs warmFirmwareProductID;   // product ID indicating the firmware has been loaded and is working.
    int coldBootProductID;                          // product ID indicating the firmware has not been loaded.
    int numberOfPorts;
    int readBufSize;
    int writeBufSize;
    char *modelName;
    INTEL_HEX_RECORD *firmware;
} productTable[] = {
    { MIDISPORT1x1, 0x1010, 1, 32, 32, "1x1", firmware1x1 },
    { MIDISPORT2x2, 0x1001, 2, 32, 32, "2x2", firmware2x2 },
    { MIDISPORT4x4, 0x1020, 4, 64, 64, "4x4", firmware4x4 },
    { MIDISPORT8x8, 0x1030, 8, 64, 32, "8x8", NULL }  // strictly speacking, the endPoint 2 can sustain 40 bytes output
};

#define PRODUCT_TOTAL (sizeof(productTable) / sizeof(struct HardwareConfigurationDescription))

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// Implementation of the factory function for this type.
extern "C" void *NewMIDISPORT2x2(CFAllocatorRef allocator, CFUUIDRef typeID) 
{
    // If correct type is being requested, allocate an
    // instance of TestType and return the IUnknown interface.
    if (CFEqual(typeID, kMIDIDriverTypeID)) {
            MIDISPORT *result = new MIDISPORT;
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
MIDISPORT::MIDISPORT() : USBMIDIDriverBase(kFactoryUUID)
{
    EZUSBLoader ezusb;
    unsigned int i;
    unsigned int testCount; // Number of 2 second interval tests we'll do for the firmware.
    
#if VERBOSE
    printf("MIDISPORTUSBDriver init\n");
#endif
    for(i = 0; i < PRODUCT_TOTAL; i++) {
        if (ezusb.FindVendorsProduct(midimanVendorID, productTable[i].coldBootProductID, true)) {
#if VERBOSE
            printf("in cold booted state, downloading firmware\n");
#endif
            ezusb.setFirmware(productTable[i].firmware);
            ezusb.StartDevice();
            // check that we re-enumerated the USB bus properly.
            for(testCount = 0; 
                testCount < 10 && !ezusb.FindVendorsProduct(midimanVendorID, productTable[i].warmFirmwareProductID, false);
                testCount++) {
#if VERBOSE
                printf("loop searching\n");
#endif
                sleep(2);
            }
            if(testCount == 10) {
#if VERBOSE
                printf("Can't find re-enumerated MIDISPORT device, probable failure in downloading firmware.\n");
#endif
            }
            return;
        }
    }
}

MIDISPORT::~MIDISPORT()
{
    //printf("~MIDISPORTUSBDriver\n");
}

// __________________________________________________________________________________________________

bool MIDISPORT::UseDevice(IOUSBDeviceInterface **device,
                             UInt16 devVendor,
                             UInt16 devProduct)
{
    unsigned int i;
    
    if(devVendor == midimanVendorID) {
#if VERBOSE
        printf("looking for MIDISPORT device 0x%x\n", devProduct);
#endif
        for(i = 0; i < PRODUCT_TOTAL; i++) {
            if(productTable[i].warmFirmwareProductID == devProduct) {
#if VERBOSE
                printf("found it\n");
#endif
                return true;
            }
        }
    }
    return false;
}

void MIDISPORT::GetInterfaceToUse(IOUSBDeviceInterface **device, 
                                     UInt8 &outInterfaceNumber,
                                     UInt8 &outAltSetting)
{
    outInterfaceNumber = kTheInterfaceToUse;
    outAltSetting = 0;
}

void MIDISPORT::FoundDevice(IOUSBDeviceInterface **device,
                               IOUSBInterfaceInterface **interface,
                               UInt16 devVendor,
                               UInt16 devProduct,
                               UInt8 interfaceNumber,
                               UInt8 altSetting,
                               MIDIDeviceListRef deviceList)
{
    MIDIDeviceRef dev;
    MIDIEntityRef ent;
    unsigned int i;

    for(i = 0; i < PRODUCT_TOTAL; i++) {
        if(productTable[i].warmFirmwareProductID == devProduct) {
            break;
        }
    }
    if(i == PRODUCT_TOTAL) {
        printf("Unable to recognize MIDIMan device %x\n", devProduct);
        return;
    }
    
    MIDIDeviceCreate(Self(),
            CFSTR(kMyBoxName),
            CFSTR(kMyManufacturerName),
            CFStringCreateWithCString(NULL, productTable[i].modelName, 0),
            &dev);

    // make numberOfPorts entities with 1 source, 1 destination
    for (int port = 0; port < productTable[i].numberOfPorts; port++) {
        char portname[64];
        switch(productTable[i].warmFirmwareProductID) {
        case MIDISPORT1x1:
            sprintf(portname, "%s %s %s", kMyManufacturerName, kMyBoxName, productTable[i].modelName);
            break;
        case MIDISPORT2x2:
        case MIDISPORT4x4:
            sprintf(portname, "%s %s %s Port %c", kMyManufacturerName, kMyBoxName, productTable[i].modelName, port + 'A');
            break;
        case MIDISPORT8x8:
        default:
            sprintf(portname, "%s %s %s Port %d", kMyManufacturerName, kMyBoxName, productTable[i].modelName, port + 1);
            break;
        }
        CFStringRef str = CFStringCreateWithCString(NULL, portname, 0);
        MIDIDeviceAddEntity(dev, str, false, 1, 1, &ent);
        CFRelease(str);
    }

    MIDIDeviceListAddDevice(deviceList, dev);
}

// note that we're using bulk endpoint for output; interrupt for input...
void MIDISPORT::GetInterfaceInfo(InterfaceState *intf, InterfaceInfo &info)
{
    info.inEndpointType = kUSBInterrupt;    // this differs from the SampleUSB and is correct.
    info.outEndpointType = kUSBBulk;
    info.readBufferSize = kReadBufSize; // TODO needs to be product specific
    info.writeBufferSize = kWriteBufSize; // TODO needs to be product specific
}

void MIDISPORT::StartInterface(InterfaceState *intf)
{
    //printf("StartInterface\n");
}

void MIDISPORT::StopInterface(InterfaceState *intf)
{
    //printf("StopInterface\n");
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
void MIDISPORT::HandleInput(InterfaceState *intf, MIDITimeStamp when, Byte *readBuf, ByteCount readBufSize)
{
    int prevInputPort = -1;	                         // signifies none
    static bool inSysex[kNumMaxPorts] = {false, false, false, false, false, false, false, false};     // is it possible to make this mInSysEx?
    static Byte runningStatus[kNumMaxPorts] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90}; // we gotta start somewhere... or make it mRunningStatus
    static int remainingBytesInMsg[kNumMaxPorts] = {0, 0, 0, 0, 0, 0, 0, 0};  // how many bytes remain to be processed per MIDI message
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

        printf("%c %d: %02X %02X %02X %02X  ", inputPort + 'A', bytesInPacket, src[0], src[1], src[2], src[3]);

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
// Fill two USB buffers, destBuf1 and destBuf2, each with a size of kWriteBufSize, with outgoing data
// in MIDISPORT-MIDI format.
// Return the number of bytes written.
// From the 8x8 Spec:
// "To ease the load on the MidiSport 8x8 processor, this limitation has been added: the host should send
// no more than two packets per each MIDI OUT or SMPTE port in a given OUT transfer.  This limitation 
// still allows MIDI data to be transferred at almost double bandwidth across the USB bus while reducing
// the MidiSport’s internal buffer requirements."
// Now that's going to be tricky to implement... :-(
void MIDISPORT::PrepareOutput(InterfaceState *intf, WriteQueue &writeQueue,
                                 Byte *destBuf1, ByteCount *bufCount1,
                                 Byte *destBuf2, ByteCount *bufCount2)
{
    Byte *dest[2] = {destBuf1, destBuf2};
    Byte *destEnd[2] = {dest[0] + kWriteBufSize, dest[1] + kWriteBufSize};
   
    while (true) {
        if (writeQueue.empty()) {
#if DEBUG_OUTBUFFER
            printf("dest buffer = ");
            for(int i = 0; i < dest[0] - destBuf1; i++)
                printf("%02X ", destBuf1[i]);
            printf("\n");
#endif
            memset(dest[0], 0, MIDIPACKETLEN);  // signal the conclusion with a single null packet
            memset(dest[1], 0, MIDIPACKETLEN);  // signal the conclusion with a single null packet
            *bufCount1 = dest[0] - destBuf1;
            *bufCount2 = dest[1] - destBuf2;
            return;
        }
                
        WriteQueueElem *wqe = &writeQueue.front();
        Byte cableNibble = wqe->portNum << 4;
        // put Port 1,3,5,7 to destBuf1, Port 2,4,6,8 to destBuf2
        Byte cableEndpoint = wqe->portNum & 0x01; 
        VLMIDIPacket *pkt = wqe->packet;
        Byte *src = pkt->data + wqe->bytesSent;
        Byte *srcend = &pkt->data[pkt->length];

        // printf("cableNibble = 0x%x, portNum = %d\n", cableNibble, wqe->portNum);
        while (src < srcend && dest[cableEndpoint] < destEnd[cableEndpoint]) {
            int outPacketLen;
            Byte c = *src++;
            
            switch (c >> 4) {
            case 0x0: case 0x1: case 0x2: case 0x3:
            case 0x4: case 0x5: case 0x6: case 0x7:
                // printf("databyte %02X\n", c);
                // data byte, presumably a sysex continuation
                *dest[cableEndpoint]++ = c;
                // sysex ends with preceding 2 bytes or sysex continues 
                outPacketLen = (pkt->length >= 3) ? 2 : pkt->length - 1;	
                
                memcpy(dest[cableEndpoint], src, outPacketLen);
                memset(dest[cableEndpoint] + outPacketLen, 0, 2 - outPacketLen);
                dest[cableEndpoint][2] = cableNibble | (outPacketLen + 1); // mark length and cable
                dest[cableEndpoint] += 3;
                src += outPacketLen;
                break;
            case 0x8:	// note-on
            case 0x9:	// note-off
            case 0xA:	// poly pressure
            case 0xB:	// control change
            case 0xE:	// pitch bend
                // printf("channel %02X\n", c);
                *dest[cableEndpoint]++ = c;
                *dest[cableEndpoint]++ = *src++;
                *dest[cableEndpoint]++ = *src++;
                *dest[cableEndpoint]++ = cableNibble | 0x03;
                break;
            case 0xC:	// program change
            case 0xD:	// mono pressure
                // printf("prch,pres %02X\n", c);
                *dest[cableEndpoint]++ = c;
                *dest[cableEndpoint]++ = *src++;
                *dest[cableEndpoint]++ = 0;
                *dest[cableEndpoint]++ = cableNibble | 0x02;
                break;
            case 0xF:	// system message
                // printf("system %02X\n", c);
                switch (c) {
                case 0xF0:	// sysex start
                    *dest[cableEndpoint]++ = c;
                    *dest[cableEndpoint]++ = *src++;
                    *dest[cableEndpoint]++ = *src++;
                    *dest[cableEndpoint]++ = cableNibble | 0x03;	// sysex start or continued
                    break;
                case 0xF6:	// tune request (0)
                case 0xF7:	// sysex conclude (0)
                case 0xF8:	// clock
                case 0xFA:	// start
                case 0xFB:	// continue
                case 0xFC:	// stop
                case 0xFE:	// active sensing
                case 0xFF:	// system reset
                    *dest[cableEndpoint]++ = c;
                    *dest[cableEndpoint]++ = 0;
                    *dest[cableEndpoint]++ = 0;
                    *dest[cableEndpoint]++ = cableNibble | 0x01;       // 1-byte system realtime or system common
                    break;
                case 0xF1:	// MTC (1)
                case 0xF3:	// song select (1)
                    *dest[cableEndpoint]++ = c;
                    *dest[cableEndpoint]++ = *src++;
                    *dest[cableEndpoint]++ = 0;
                    *dest[cableEndpoint]++ = cableNibble | 0x02;	// 2-byte system common
                    break;
                case 0xF2:	// song pointer (2)
                    *dest[cableEndpoint]++ = c;
                    *dest[cableEndpoint]++ = *src++;
                    *dest[cableEndpoint]++ = *src++;
                    *dest[cableEndpoint]++ = cableNibble | 0x03;	// 3-byte system common
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

            if (dest[cableEndpoint] > destEnd[cableEndpoint] - 4) {
                // one of the destBuf's are completely filled
                *bufCount1 = dest[0] - destBuf1;
                *bufCount2 = dest[1] - destBuf2;
                return; 
            }
            // we didn't fill the output buffer, is there more source data in the write queue?
        }
    }
}

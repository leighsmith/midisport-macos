// $Id: MIDISPORTUSBDriver.cpp,v 1.15 2001/10/29 23:28:38 leigh Exp $
//
// MacOS X driver for MIDIMan MIDISPORT USB MIDI interfaces.
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
 IMPORTANT: This Apple software is supplied to you by Apple Computer,
 Inc. ("Apple") in consideration of your agreement to the following terms,
 and your use, installation, modification or redistribution of this Apple
 software constitutes acceptance of these terms.  If you do not agree with
 these terms, please do not use, install, modify or redistribute this Apple
 software.
 
 In consideration of your agreement to abide by the following terms, and
 subject to these terms, Apple grants you a personal, non-exclusive
 license, under Apple’s copyrights in this original Apple software (the
 "Apple Software"), to use, reproduce, modify and redistribute the Apple
 Software, with or without modifications, in source and/or binary forms;
 provided that if you redistribute the Apple Software in its entirety and
 without modifications, you must retain this notice and the following text
 and disclaimers in all such redistributions of the Apple Software.
 Neither the name, trademarks, service marks or logos of Apple Computer,
 Inc. may be used to endorse or promote products derived from the Apple
 Software without specific prior written permission from Apple. Except as
 expressly stated in this notice, no other rights or licenses, express or
 implied, are granted by Apple herein, including but not limited to any
 patent rights that may be infringed by your derivative works or by other
 works in which the Apple Software may be incorporated.
 
 The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES
 NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE
 IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION
 ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
 
 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
 MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND
 WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT
 LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY
 OF SUCH DAMAGE.  */

#include <stddef.h>
#include <stdio.h>
#include <algorithm>
#include "MIDISPORTUSBDriver.h"
#include "USBUtils.h"

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// things to customize

// Unique UUID (Universally Unique Identifier) For the MIDIMAN MIDISPORT USB MIDI interface driver
#define kFactoryUUID CFUUIDGetConstantUUIDWithBytes(NULL, 0x6E, 0x44, 0x3F, 0xE8, 0x9E, 0xF4, 0x11, 0xD4, 0xA2, 0xFE, 0x00, 0x05, 0x02, 0xB6, 0x21, 0x33)
// ########

#define kTheInterfaceToUse	0		// 0 is the interface which we need to access the 5 endpoints.
#define midimanVendorID		0x0763		// midiman

// and these
#define kMyBoxName		"MIDISPORT"
#define kMyManufacturerName	"MIDIMAN"

#define kNumMaxPorts		9

#define MIDIPACKETLEN		4		// number of bytes in a dword packet received and sent to the MIDISPORT
#define CMDINDEX		(MIDIPACKETLEN - 1)  // which byte in the packet has the length and port number.

#define DEBUG_OUTBUFFER		0		// 1 to printout whenever a msg is to be sent.
#define VERBOSE             (DEBUG || 1)

struct HardwareConfigurationDescription {
    WarmFirmwareProductIDs warmFirmwareProductID;   // product ID indicating the firmware has been loaded and is working.
    int numberOfPorts;
    int readBufSize;
    int writeBufSize;
    const char *modelName;
} productTable[] = {
    { MIDISPORT1x1, 1, 32, 32, "1x1" },
    { MIDISPORT2x2, 2, 32, 32, "2x2" },
    { MIDISPORT4x4, 4, 64, 64, "4x4" },
    // Strictly speaking, the endPoint 2 can sustain 40 bytes output on the 8x8. 
    // There are 9 ports including the SMPTE control.
    { MIDISPORT8x8, 9, 64, 32, "8x8" }
};

#define PRODUCT_TOTAL (sizeof(productTable) / sizeof(struct HardwareConfigurationDescription))

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

#if 0
// Implementation of the factory function for this type.
extern "C" void *NewMIDISPORT2x2(CFAllocatorRef allocator, CFUUIDRef typeID) 
{
    // If correct type is being requested, allocate an
    // instance of TestType and return the IUnknown interface.
    if (CFEqual(typeID, kMIDIDriverTypeID)) {
        MIDISPORT *result = new MIDISPORT;
        return result->Self();
    }
    else {
        // If the requested type is incorrect, return NULL.
        return NULL;
    }
}
#endif

// __________________________________________________________________________________________________

MIDISPORT::MIDISPORT() : USBMIDIDriverBase(kFactoryUUID)
{
#if VERBOSE
    printf("MIDISPORTUSBDriver init\n");
#endif
    // if (CFEqual(typeID, kMIDIDriverInterfaceID))
    connectedMIDISPORTIndex = -1;   // error condition
}

MIDISPORT::~MIDISPORT()
{
#if VERBOSE
    printf("~MIDISPORTUSBDriver\n");
#endif
}

// __________________________________________________________________________________________________

bool MIDISPORT::MatchDevice(IOUSBDeviceInterface **device,
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
                connectedMIDISPORTIndex = i;
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
#if VERBOSE
    printf("MIDISPORT::GetInterfaceToUse outInterfaceNumber = %d\n", outInterfaceNumber);
#endif
    outInterfaceNumber = kTheInterfaceToUse;
    outAltSetting = 0;
}

MIDIDeviceRef MIDISPORT::CreateDevice(io_service_t ioDevice,
                             io_service_t ioInterface,
                             IOUSBDeviceInterface **device,
                             IOUSBInterfaceInterface **interface,
                             UInt16 devVendor,
                             UInt16 devProduct,
                             UInt8 interfaceNumber,
                             UInt8 altSetting)
{
    MIDIDeviceRef dev;
    MIDIEntityRef ent;
    unsigned int i;

#if VERBOSE
    printf("MIDISPORT::CreateDevice\n");
#endif
#if 1 // Is this necessary? Probably worthwhile for robustness of error messages.
    for(i = 0; i < PRODUCT_TOTAL; i++) {
        if(productTable[i].warmFirmwareProductID == devProduct) {
            connectedMIDISPORTIndex = i;
            break;
        }
    }
    if(i == PRODUCT_TOTAL) {
        printf("Unable to recognize MIDIMan device %x\n", devProduct);
        return NULL;  // TODO this needs to be checked if this is legitimate to return in case of error.
    }
#endif
    
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
            if (port == 8) // be descriptive in naming the SMPTE channel
                sprintf(portname, "%s %s %s SMPTE Port", kMyManufacturerName, kMyBoxName, productTable[i].modelName);
            else
                sprintf(portname, "%s %s %s Port %d", kMyManufacturerName, kMyBoxName, productTable[i].modelName, port + 1);
            break;
        }
        CFStringRef str = CFStringCreateWithCString(NULL, portname, 0);
        MIDIDeviceAddEntity(dev, str, false, 1, 1, &ent);
        CFRelease(str);
    }

    return dev;
}

// note that we're using bulk endpoint for output; interrupt for input...
void MIDISPORT::GetInterfaceInfo(InterfaceState *intf, InterfaceInfo &info)
{
#if VERBOSE
    printf("MIDISPORT::GetInterfaceInfo\n");
#endif
    info.inEndpointType = kUSBInterrupt;    // this differs from the SampleUSB and is correct.
    info.outEndpointType = kUSBBulk;
    if(connectedMIDISPORTIndex != -1) {
        info.readBufferSize  = productTable[connectedMIDISPORTIndex].readBufSize;
        info.writeBufferSize = productTable[connectedMIDISPORTIndex].writeBufSize;
#if VERBOSE
        printf("setting readBufferSize = %ld, writeBufferSize = %ld\n", info.readBufferSize, info.writeBufferSize);
#endif
    }
    else
        printf("Assertion failed: connectedMIDISPORTIndex == -1\n");
}

void MIDISPORT::StartInterface(InterfaceState *intf)
{
#if VERBOSE
    printf("StartInterface\n");
#endif
}

void MIDISPORT::StopInterface(InterfaceState *intf)
{
#if VERBOSE
    printf("StopInterface\n");
#endif
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
    // is it possible to make this mInSysEx?
    static bool inSysex[kNumMaxPorts] = {false, false, false, false, false, false, false, false, false};
    // we gotta start somewhere... or make it mRunningStatus
    static Byte runningStatus[kNumMaxPorts] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    // how many bytes remain to be processed per MIDI message
    static int remainingBytesInMsg[kNumMaxPorts] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
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

        printf("%c %d: %02X %02X %02X %02X  \n", inputPort + 'A', bytesInPacket, src[0], src[1], src[2], src[3]);

        // if input came from a different input port, flush the packet list.
        if (prevInputPort != -1 && inputPort != prevInputPort) {
            printf("flushing source %d\n", inputPort);
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
                printf("new status %02X, remainingBytesInMsg = %d\n", status, remainingBytesInMsg[inputPort]);
            }
            else if(remainingBytesInMsg[inputPort] > 0) {   // still within a message
                remainingBytesInMsg[inputPort]--;
                // store ready for packetting.
                completeMessage[numCompleted++] = src[byteIndex];
                printf("in message remainingBytesInMsg = %d\n", remainingBytesInMsg[inputPort]);
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
                printf("Shipping a packet: ");
                for(int i = 0; i < numCompleted; i++)
                    printf("%02X ", completeMessage[i]);
                pkt = MIDIPacketListAdd(pktlist, sizeof(pbuf), pkt, when, numCompleted, completeMessage);
                numCompleted = 0;
                printf("shipped\n");
            }
            if(preservedMsgCount != 0) {
                remainingBytesInMsg[inputPort] = preservedMsgCount;
            }
        }
    }
    if (pktlist->numPackets > 0 && prevInputPort != -1) {
        printf("source %d receiving %d packets\n", prevInputPort, pktlist->numPackets);
        printf("number of entities %d\n", intf->mNumEntities);
        MIDIReceived(intf->mSources[prevInputPort], pktlist);
        printf("\n");
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
    Byte *destEnd[2] = {dest[0] + productTable[connectedMIDISPORTIndex].writeBufSize,
                        dest[1] + productTable[connectedMIDISPORTIndex].writeBufSize};
   
    while (true) {
        if (writeQueue.empty()) {
            long buf1Length = dest[0] - destBuf1;
            long buf2Length = dest[1] - destBuf2;
#if DEBUG_OUTBUFFER
            printf("dest buffer = ");
            for(int i = 0; i < dest[0] - destBuf1; i++)
                printf("%02X ", destBuf1[i]);
            printf("\n");
#endif

            if(buf1Length > 0) {
                memset(dest[0], 0, MIDIPACKETLEN);  // signal the conclusion with a single null packet
                *bufCount1 = buf1Length + MIDIPACKETLEN; // + MIDIPACKETLEN for null packet.
            }
            else
                *bufCount1 = 0;
            if(buf2Length > 0) {
                memset(dest[1], 0, MIDIPACKETLEN);  // signal the conclusion with a single null packet
                *bufCount2 = buf2Length + MIDIPACKETLEN; // + MIDIPACKETLEN for null packet.
            }
            else
                *bufCount2 = 0;
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
            long outPacketLen;
            long numToBeSent;
            Byte c = *src++;
            
            // printf("byte %02X\n", c);
            switch (c >> 4) {
            case 0x0: case 0x1: case 0x2: case 0x3:
            case 0x4: case 0x5: case 0x6: case 0x7:
                // printf("sysex databyte %02X\n", c);
                // data byte, presumably a sysex continuation
                *dest[cableEndpoint]++ = c;
                numToBeSent = srcend - src;
                // sysex ends with 2 preceding data bytes or sysex continues, such that the 
                // sysex end message begins the packet as the cmd.
                outPacketLen = (numToBeSent >= 3) ? 2 : numToBeSent - 1;	
                
                // printf("outPacketLen = %d, numToBeSent = %d\n", outPacketLen, numToBeSent);
                memcpy(dest[cableEndpoint], src, outPacketLen);
                memset(dest[cableEndpoint] + outPacketLen, 0, 2 - outPacketLen);
                dest[cableEndpoint][2] = cableNibble | (outPacketLen + 1); // mark length and cable
                dest[cableEndpoint] += (MIDIPACKETLEN - 1);   // we advance by one packet length (4 bytes)
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

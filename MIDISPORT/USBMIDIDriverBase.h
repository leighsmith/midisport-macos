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

#ifndef __USBMIDIDriverBase_h__
#define __USBMIDIDriverBase_h__

#include <CoreMIDIServer/MIDIDriver.h>
#include <vector.h>
#include <list.h>
#include <pthread.h>
#include "USBUtils.h"
#include "MIDIPacket.h"

class InterfaceState;

typedef vector<InterfaceState *> InterfaceStateList;

class WriteQueueElem {
public:
	VLMIDIPacket *		packet;
	int					portNum;
	ByteCount			bytesSent;	// this much of the packet has been sent
};

typedef list<WriteQueueElem> WriteQueue;

struct InterfaceInfo {
	UInt8				inEndpointType;		// kUSBBulk, etc.
	UInt8				outEndpointType;
	ByteCount			readBufferSize;
	ByteCount			writeBufferSize;
};

// _________________________________________________________________________________________
// USBMIDIDriverBase
//
// MIDIDriver subclass, derive your USB MIDI driver from this
class USBMIDIDriverBase : public MIDIDriver {
public:
	USBMIDIDriverBase(CFUUIDRef factoryID);
	~USBMIDIDriverBase();
	
	// overrides of MIDIDriver virtual methods
	virtual OSStatus	FindDevices(		MIDIDeviceListRef devices );
	virtual OSStatus	Start(				MIDIDeviceListRef devices );
	virtual OSStatus	Stop();
	virtual OSStatus	Send(				const MIDIPacketList *pktlist,
											void *endptRef1,
											void *endptRef2 );

	// our own virtual methods

	virtual bool		UseDevice(			IOUSBDeviceInterface **	device,
											UInt16					devVendor,
											UInt16					devProduct ) = 0;
							// given a USB device and its vendor/product IDs,
							// return a boolean saying whether this driver wants
							// to control this device

	virtual void		GetInterfaceToUse(	IOUSBDeviceInterface **device, 
											UInt8 &outInterfaceNumber,
											UInt8 &outAltSetting ) = 0;
							// given a USB device, return the interface number and
							// alternate setting to use

	virtual void		FoundDevice(	IOUSBDeviceInterface **		device,
										IOUSBInterfaceInterface **	interface,
										UInt16						devVendor,
										UInt16						devProduct,
										UInt8						interfaceNumber,
										UInt8						altSetting,
										MIDIDeviceListRef			deviceList ) = 0;
							// given a USB device, create and add a MIDIDevice representation of it
							// to the MIDIDeviceList

	virtual void		GetInterfaceInfo(	InterfaceState *intf,
											InterfaceInfo &info) = 0;
							// given an interface, get its info: endpoint types to use,
							// read size

	virtual void		StartInterface(		InterfaceState *intf ) = 0;
							// pipes are opened, do any extra initialization (send config msgs etc)
							
	virtual void		StopInterface(		InterfaceState *intf ) = 0;
							// pipes are about to be closed, do any preliminary cleanup
							
	virtual void		HandleInput(		InterfaceState *intf,
											MIDITimeStamp when,
											Byte *readBuf,
											ByteCount readBufSize ) = 0;
							// a USB message arrived, parse it into a MIDIPacketList and
							// call MIDIReceived

	virtual void	PrepareOutput(InterfaceState *intf, WriteQueue &writeQueue, 
                            Byte *destBuf1, ByteCount *bufCount1,
                            Byte *destBuf2, ByteCount *bufCount2) = 0;
            
							// dequeue from WriteQueue into a single USB message, return
							// length of the message.  Called with the queue mutex locked.

	// Utilities to implement the USB MIDI class spec methods of encoding MIDI in USB packets
	static void			USBMIDIHandleInput(	InterfaceState *intf, 
											MIDITimeStamp when,
											Byte *readBuf,
											ByteCount bufSize );

	static ByteCount	USBMIDIPrepareOutput(InterfaceState *intf,
											WriteQueue &writeQueue, 
											Byte *destBuf,
											ByteCount bufSize );

private:
	InterfaceStateList	*mInterfaceStateList;
};

// _________________________________________________________________________________________
// InterfaceState
// 
// This class is the runtime state for one interface instance
class InterfaceState {
public:
	InterfaceState(	USBMIDIDriverBase *driver,
					MIDIDeviceRef midiDevice, 
					IOUSBDeviceInterface **usbDevice, 
					IOUSBInterfaceInterface **usbInterface);

	virtual ~InterfaceState();
	
	void		DoRead();	
	static void	ReadCallback(void *refcon, IOReturn result, void *arg0);
	void		DoWrite();	
	static void	WriteCallback(void *refcon, IOReturn result, void *arg0);

	void	HandleInput(ByteCount bytesReceived);
	void	Send(const MIDIPacketList *pktlist, int portNumber);
	
	void	GetInterfaceInfo(InterfaceInfo &info) 
	{
		mDriver->GetInterfaceInfo(this, info);
	}
	
	// leave data members public, for benefit of driver methods
	USBMIDIDriverBase *	    mDriver;
	IOUSBDeviceInterface **	    mDevice; 
	IOUSBInterfaceInterface	**  mInterface;
	UInt8			    mInPipe, mOutPipe1, mOutPipe2;
	bool			    mHaveInPipe, mHaveOutPipe1, mHaveOutPipe2;
	InterfaceInfo		    mInterfaceInfo;
	int			    mNumEntities;
	MIDIEndpointRef *	    mSources;
	Byte *			    mReadBuf;
	Byte *			    mWriteBuf1;
	Byte *			    mWriteBuf2;

	WriteQueue		    mWriteQueue;
	pthread_mutex_t		    mWriteQueueMutex;

	bool			    mWritePending;
	bool			    mStopRequested;

	// input parse state
	Byte			    mReadCable;
	Byte			    mRunningStatus;
	bool			    mInSysEx;
	
	// output state
	Byte			    mWriteCable;
};


// utilities
int		MIDIDataBytes(Byte statusByte);
			// returns number of data bytes that follow a given status byte
			// (which can be anything except F0, F7, and realtime status bytes)

#endif // __USBMIDIDriverBase_h__

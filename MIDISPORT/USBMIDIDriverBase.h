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

#define USBMIDI_ASYNCIO 0

#if !USBMIDI_ASYNCIO
	#include "XThread.h"

	class IOThread : public XThread {
	public:
		IOThread(InterfaceState *rs) : XThread(XThread::kPrioritySystem, 0, XThread::kPolicyFIFO),
			mInterfaceState(rs) { }

		bool			Running() const { return mRunning; }

	protected:
		InterfaceState *mInterfaceState;
		bool			mRunning;
	};
		
	class ReadThread : public IOThread {
	public:
		ReadThread(InterfaceState *rs) : IOThread(rs) { }

		virtual void	Run();
	};
	
	class WriteThread : public IOThread {
	public:
		WriteThread(InterfaceState *rs) : IOThread(rs) { }

		virtual void	Run();
	};
	
#endif

class WriteQueueElem {
public:
	VLMIDIPacket *		packet;
	int					portNum;
	int					bytesSent;	// this much of the packet has been sent
};

typedef list<WriteQueueElem> WriteQueue;

struct InterfaceInfo {
	UInt8		inEndptType;		// kUSBBulk, etc.
	UInt8		outEndptType;
	ByteCount	readBufSize;
	ByteCount	writeBufSize;
};

// MIDIDriver subclass, derive your driver from this
class USBMIDIDriverBase : public MIDIDriver {
public:
	USBMIDIDriverBase(CFUUIDRef factoryID);
	~USBMIDIDriverBase();
	
	// implementation of MIDIDriver
	virtual OSStatus	Start(MIDIDeviceListRef devices);
	virtual OSStatus	Stop();
	virtual OSStatus	Send(const MIDIPacketList *pktlist, void *endptRef1, void *endptRef2);

	// our own virtual methods
	virtual IOUSBMatch *GetUSBMatch() = 0;
							// return an IOUSBMatch which will determine which devices
							// get scanned in more detail
	virtual int			InterfaceIndexToUse(IOUSBDeviceRef device) = 0;
							// given a USB device, return index of interface to use
	virtual void		GetInterfaceInfo(InterfaceState *intf, InterfaceInfo &info) = 0;
							// given an interface, get its info: endpoint types to use,
							// read size
	virtual void		StartInterface(InterfaceState *intf) = 0;
							// pipes are opened, do any extra initialization (send config msgs etc)
	virtual void		StopInterface(InterfaceState *intf) = 0;
							// pipes are about to be closed, do any preliminary cleanup
	virtual void		HandleInput(InterfaceState *intf, MIDITimeStamp when, Byte *readBuf, int readBufSize) = 0;
							// a USB message arrived, parse it into a MIDIPacketList and
							// call MIDIReceived
	virtual int			PrepareOutput(InterfaceState *intf, WriteQueue &writeQueue, Byte *destBuf) = 0;
							// dequeue from WriteQueue into a single USB message, return
							// length of the message.  Called with the queue mutex locked.

	// Utilities to implement the USB MIDI class spec methods of encoding MIDI in USB packets
	static void			USBMIDIHandleInput(InterfaceState *intf, MIDITimeStamp when, Byte *readBuf,
							int bufSize);
	static int			USBMIDIPrepareOutput(InterfaceState *intf, WriteQueue &writeQueue, 
							Byte *destBuf, int bufSize);

private:
	InterfaceStateList		*mInterfaceStateList;
};



// This class is the runtime state for one interface instance
class InterfaceState {
public:
	InterfaceState(USBMIDIDriverBase *driver, MIDIDeviceRef midiDevice, IOUSBDeviceRef usbDevice, 
					XUSBInterface usbInterface);

	virtual ~InterfaceState();
	
#if USBMIDI_ASYNCIO
	void	DoRead()
	{
		if (mInPipe) {
			IOReturn ret = IOUSBReadPipeAsync(mInPipe, mReadBuf, 1, ReadCallback, this);
			if (ret)
				printerr("IOUSBReadPipeAsync", ret);
		}
	}
	
	static void	ReadCallback(void *refcon, IOReturn result, void *arg0)
	{
		InterfaceState *self = (InterfaceState *)refcon;
		printf("ReadCallback: arg0 is %ld\n", (long)arg0);
		self->HandleInput();
		// chain another async read
		self->DoRead();
	}
#else
	void	DoWrites();	// called from the write thread

#endif // USBMIDI_ASYNCIO

	void	HandleInput(int packetSize);
	void	Send(const MIDIPacketList *pktlist, int portNumber);
	
	void	GetInterfaceInfo(InterfaceInfo &info) 
	{
		mDriver->GetInterfaceInfo(this, info);
	}
	
	// leave data members public, for benefit of driver methods?
	USBMIDIDriverBase *	mDriver;
	IOUSBDeviceRef		mDevice;
	XUSBInterface		mInterface;
	IOUSBPipeRef		mInPipe, mOutPipe;
	int					mNumEntities;
	MIDIEndpointRef *	mSources;
	Byte *				mReadBuf;
	Byte *				mWriteBuf;
	
	WriteQueue			mWriteQueue;
	pthread_mutex_t		mWriteQueueMutex;
	pthread_cond_t		mWriteQueueNonEmpty;

#if !USBMIDI_ASYNCIO
	ReadThread *		mReadThread;
	WriteThread *		mWriteThread;
#endif
	bool				mStopRequested;

	// parse state
	Byte				mReadCable;
	Byte				mRunningStatus;
	bool				mInSysEx;
	
	// output state
	Byte				mWriteCable;
};


// utilities
int		MIDIDataBytes(Byte statusByte);
			// returns number of data bytes that follow a given status byte
			// (which can be anything except F0, F7, and realtime status bytes)

#endif // __USBMIDIDriverBase_h__

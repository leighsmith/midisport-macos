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

#include "USBMIDIDriverBase.h"
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <Carbon/Carbon.h>

//#define VERBOSE (DEBUG && 1)
//#define DUMP_INPUT 1
//#define DUMP_OUTPUT 1

// __________________________________________________________________________________________________
//#define ANALYZE_THRU_TIMING 1
#if ANALYZE_THRU_TIMING

#include <CoreAudio/HostTime.h>

class TimingAnalyzer {
public:
	enum { kMaxSamples = 1000 };

	TimingAnalyzer()
	{
		Clear();
	}
	
	void	Clear()
	{
		mNumSamples = 0;
	}
	
	void	AddSample(UInt32 x)
	{
		int i = mNumSamples;
		if (i < kMaxSamples) {
			mSamples[i] = x;
			mNumSamples = i + 1;
		}
	}

	void	Dump()
	{
		for (int i = 0; i < mNumSamples; ++i)
			printf("%10ld\n", (long)mSamples[i]);
	}

	UInt32		mSamples[kMaxSamples];
	int			mNumSamples;
};

TimingAnalyzer	gTimingAnalyzer;
#endif


// __________________________________________________________________________________________________
// returns number of data bytes which follow the status byte.
// returns -1 for 0xF0 sysex beginning (indicating a variable number of data bytes
//		following).
// returns 0 if an unknown MIDI status byte is received and prints a warning.
int		MIDIDataBytes(Byte status)
{
	if (status >= 0x80 && status < 0xF0)
		return ((status & 0xE0) == 0xC0) ? 1 : 2;

	switch (status) {
	case 0xF0:
		return -1;
	case 0xF1:		// MTC
	case 0xF3:		// song select
		return 1;
	case 0xF2:		// song pointer
		return 2;
	case 0xF6:		// tune request
	case 0xF7:		// sysex conclude, nothing follows.
	case 0xF8:		// clock
	case 0xFA:		// start
	case 0xFB:		// continue
	case 0xFC:		// stop
	case 0xFE:		// active sensing
	case 0xFF:		// system reset
		return 0;
	}

	fprintf(stderr, "MIDIEventLength: illegal status byte %02X\n", status);
	return 0;   // the MIDI spec says we should ignore illegals.
}

// __________________________________________________________________________________________________

InterfaceState::InterfaceState(	USBMIDIDriverBase *driver, 
								MIDIDeviceRef midiDevice, 
								IOUSBDeviceInterface **usbDevice,
								IOUSBInterfaceInterface **usbInterface) :
	mSources(NULL)
{
	UInt8	   		numEndpoints, pipeNum, direction, transferType, interval;
	UInt16			pipeIndex, maxPacketSize; 		
	
	mDriver = driver;
	mDevice = usbDevice;
	mInterface = usbInterface;
	mHaveInPipe = false;
	mHaveOutPipe = false;
 
	pthread_mutex_init(&mWriteQueueMutex, NULL);

	GetInterfaceInfo(mInterfaceInfo); 	// Get endpoint types and buffer sizes
	mReadBuf = new Byte[mInterfaceInfo.readBufferSize];
	mWriteBuf = new Byte[mInterfaceInfo.writeBufferSize];

	numEndpoints = 0;
	require_noerr((*mInterface)->GetNumEndpoints(mInterface, &numEndpoints), errexit);
		// find the number of endpoints for this interface

	require_noerr((*mInterface)->USBInterfaceOpen(mInterface), errexit);

	for (pipeIndex = 1; pipeIndex <= numEndpoints; ++pipeIndex) { 
		require_noerr((*mInterface)->GetPipeProperties(mInterface, pipeIndex, &direction, &pipeNum, &transferType, &maxPacketSize, &interval), nextPipe);
		#if VERBOSE 
			printf("pipe index %d: dir=%d, num=%d, tt=%d, maxPacketSize=%d, interval=%d\n", pipeIndex,  direction, pipeNum, transferType, maxPacketSize, interval);
		#endif
		if (direction == kUSBOut) {
			mOutPipe = pipeIndex;
			mHaveOutPipe = true;
		} else if (direction == kUSBIn) {
			mInPipe = pipeIndex;
			mHaveInPipe = true;
		}
nextPipe: ;
	}
	// don't go any further if we don't have a valid pipe
	require(mHaveOutPipe || mHaveInPipe, errexit);

	#if VERBOSE
		printf("starting MIDI, mOutPipe=0x%lX, mInPipe=0x%lX\n", (long)mOutPipe, (long)mInPipe);
	#endif
	
	// now set up all the sources and destinations
	// !!! this may be too specific; it assumes that every entity has 1 source and 1 destination
	// if this assumption is false, more specific code is needed
	// !!! could factor this into a virtual method with a default implementation.
	mNumEntities = MIDIDeviceGetNumberOfEntities(midiDevice);
	mSources = new MIDIEndpointRef[mNumEntities];

	for (int ient = 0; ient < mNumEntities; ++ient) {
		MIDIEntityRef ent = MIDIDeviceGetEntity(midiDevice, ient);

		// destination refCons: output pipe, cable number (0-based)
		MIDIEndpointRef dest = MIDIEntityGetDestination(ent, 0);
		MIDIEndpointSetRefCons(dest, this, (void *)ient);
		mSources[ient] = MIDIEntityGetSource(ent, 0);
	}
	mStopRequested = false;
	mWriteCable = 0xFF;
	mReadCable = 0;
	mRunningStatus = 0;
	mInSysEx = 0;

	{
		CFRunLoopRef ioRunLoop = MIDIGetDriverIORunLoop();
		CFRunLoopSourceRef source;
		
		if (ioRunLoop != NULL) {
			source = (*mInterface)->GetInterfaceAsyncEventSource(mInterface);
			if (source == NULL) {
				require_noerr((*mInterface)->CreateInterfaceAsyncEventSource(mInterface, &source), errexit);
				require(source != NULL, errexit);
			}
			if (!CFRunLoopContainsSource(ioRunLoop, source, kCFRunLoopDefaultMode))
				CFRunLoopAddSource(ioRunLoop, source, kCFRunLoopDefaultMode);
		}
	}
	mWritePending = false;
	DoRead();

	mDriver->StartInterface(this);
	// Start MIDI.  Do driver specific initialization.
	// Here, the driver can do things like send MIDI to the interface to
	// configure it.
errexit:
	;
}

// __________________________________________________________________________________________________
InterfaceState::~InterfaceState()
{
	CFRunLoopRef ioRunLoop = MIDIGetDriverIORunLoop();
	CFRunLoopSourceRef source;
	
	if (ioRunLoop != NULL) {
		source = (*mInterface)->GetInterfaceAsyncEventSource(mInterface);
		if (source != NULL && CFRunLoopContainsSource(ioRunLoop, source, kCFRunLoopDefaultMode))
			CFRunLoopRemoveSource(ioRunLoop, source, kCFRunLoopDefaultMode);
	}

	if (mHaveOutPipe || mHaveInPipe)
		mDriver->StopInterface(this);

	if (mHaveOutPipe)
		verify_noerr((*mInterface)->AbortPipe(mInterface, mOutPipe));

	if (mHaveInPipe)
		verify_noerr((*mInterface)->AbortPipe(mInterface, mInPipe)); 
			// this should force the read thread to die (if we're not using async)

	if (mInterface) {
		verify_noerr((*mInterface)->USBInterfaceClose(mInterface));
		(*mInterface)->Release(mInterface);
	}
	
	if (mDevice) {
		verify_noerr((*mDevice)->USBDeviceClose(mDevice));
		(*mDevice)->Release(mDevice);
	}
	
	pthread_mutex_destroy(&mWriteQueueMutex);
	delete[] mSources;
	delete[] mReadBuf;
	delete[] mWriteBuf;
	
#if VERBOSE
	printf("driver stopped MIDI\n");
#endif
}

// __________________________________________________________________________________________________
void	InterfaceState::HandleInput(ByteCount bytesReceived)
{
	AbsoluteTime now = UpTime();
//	printf("packetSize = %ld\n", readBufLength);
//	printf("mReadBuf[0-23]: ");
//	for (int i = 0; i < 24; i++)
//		printf("%02X ", mReadBuf[i]);
//	   printf("\n");
	mDriver->HandleInput(this, UnsignedWideToUInt64(now), mReadBuf, bytesReceived);
}

// __________________________________________________________________________________________________
void	InterfaceState::Send(const MIDIPacketList *pktlist, int portNumber)
{
	pthread_mutex_lock(&mWriteQueueMutex);
	const MIDIPacket *srcpkt = pktlist->packet;
	for (int i = pktlist->numPackets; --i >= 0; ) {
		WriteQueueElem wqe;
		
		wqe.packet = NewMIDIPacket(srcpkt);
		wqe.portNum = portNumber;
		wqe.bytesSent = 0;
		mWriteQueue.push_back(wqe);
		
		srcpkt = MIDIPacketNext(srcpkt);
	}
	if (!mWritePending)
		DoWrite();

	pthread_mutex_unlock(&mWriteQueueMutex);
}

// __________________________________________________________________________________________________

void	InterfaceState::DoRead()
{
	if (mHaveInPipe) {
		verify_noerr((*mInterface)->ReadPipeAsync(mInterface, mInPipe, mReadBuf, mInterfaceInfo.readBufferSize, ReadCallback, this));
	}
}

// this is the IOAsyncCallback (static method)
void	InterfaceState::ReadCallback(void *refcon, IOReturn asyncReadResult, void *arg0)
{
	require_noerr(asyncReadResult, done);
	{
		InterfaceState *self = (InterfaceState *)refcon;
		ByteCount bytesReceived = (ByteCount)arg0;
		//printf("ReadCallback: arg0 is %ld\n", (long)bytesReceived);
		self->HandleInput(bytesReceived);
		// chain another async read
		self->DoRead();
	}
done:
	;
}

// must only be called with mWriteQueueMutex acquired and mWritePending false
void	InterfaceState::DoWrite()
{
	if (mHaveOutPipe) {
		if (!mWriteQueue.empty()) {
			ByteCount msglen = mDriver->PrepareOutput(this, mWriteQueue, mWriteBuf);
			if (msglen > 0) {
#if DUMP_OUTPUT
				printf("OUT: ");
				for (ByteCount i = 0; i < msglen; i += 4) {
					if (mWriteBuf[i] == 0)
						break;
					printf("%02X %02X %02X %02X ", mWriteBuf[i], mWriteBuf[i+1], mWriteBuf[i+2], mWriteBuf[i+3]);
				}
				printf("\n");
#endif
				mWritePending = true;
				verify_noerr((*mInterface)->WritePipeAsync(mInterface, mOutPipe, mWriteBuf, msglen, WriteCallback, this));
			}
		}
	}
}

// this is the IOAsyncCallback (static method)
void	InterfaceState::WriteCallback(void *refcon, IOReturn asyncWriteResult, void *arg0)
{
	require_noerr(asyncWriteResult, done);
	{
		InterfaceState *self = (InterfaceState *)refcon;
		// chain another write
		pthread_mutex_lock(&self->mWriteQueueMutex);
		self->mWritePending = false;
		self->DoWrite();
		pthread_mutex_unlock(&self->mWriteQueueMutex);
	}
done:
	;
}

// __________________________________________________________________________________________________
// This class finds interface instances, called from FindDevices()
class InterfaceLocator : public USBDeviceScanner {
public:
	void			Find(MIDIDeviceListRef devices, USBMIDIDriverBase *driver)
	{
		mDeviceList = devices;
		mDriver = driver;
		ScanDevices();
	}
	
	virtual bool	UseDevice(			IOUSBDeviceInterface **	device,
										UInt16					devVendor,
										UInt16					devProduct )
	{
		return mDriver->UseDevice(device, devVendor, devProduct);
	}
	
	virtual void	GetInterfaceToUse(	IOUSBDeviceInterface **	device,
										UInt8 &					outInterfaceNumber, 
										UInt8 &					outAltSetting )
	{
		mDriver->GetInterfaceToUse(device, outInterfaceNumber, outAltSetting);
	}

	virtual bool	FoundInterface(		IOUSBDeviceInterface **		device,
										IOUSBInterfaceInterface **	interface,
										UInt16						devVendor,
										UInt16						devProduct,
										UInt8						interfaceNumber,
										UInt8						altSetting )
	{
		mDriver->FoundDevice(device, interface, devVendor, devProduct, interfaceNumber, altSetting, mDeviceList);
		return false;		// don't keep device/interface open
	}
private:
	MIDIDeviceListRef	mDeviceList;
	USBMIDIDriverBase *	mDriver;
};

// __________________________________________________________________________________________________
// This class creates runtime states for interface instances, called from Start()
class InterfaceStarter : public USBDeviceScanner {
public:
	void	Start(	USBMIDIDriverBase *driver, 
					MIDIDeviceListRef devices, 
					InterfaceStateList *rsl )
	{
		mDriver = driver;
		mNumDevicesFound = 0;
		mDevicesToUse = devices;
		mInterfaceStateList = rsl;
		if (MIDIDeviceListGetNumberOfDevices(mDevicesToUse) > 0)
			// don't bother doing any work if there are no devices
			ScanDevices();
	}

	virtual bool	UseDevice(			IOUSBDeviceInterface **	device,
										UInt16					devVendor,
										UInt16					devProduct )
	{
		return mDriver->UseDevice(device, devVendor, devProduct);
	}
	
	virtual void	GetInterfaceToUse(	IOUSBDeviceInterface **device, 
										UInt8 &outInterfaceNumber, 
										UInt8 &outAltSetting ) 
	{
		mDriver->GetInterfaceToUse(device, outInterfaceNumber, outAltSetting);
	}

	virtual bool	FoundInterface(		IOUSBDeviceInterface **		device,
										IOUSBInterfaceInterface **	interface,
										UInt16						devVendor,
										UInt16						devProduct,
										UInt8						interfaceNumber,
										UInt8						altSetting )
	{
		// We're trying to make matches between the devices that were found during 
		// an earlier search, and being provided to us in mDevicesToUse,
		// vs. the devices that are now present.
		
		// But this is designed for the simplistic case of only one device having been found
		// earlier, and 0 or 1 devices being present now.
		// To support multiple device instances properly, we'd need more complex matching code.
		if (mNumDevicesFound < MIDIDeviceListGetNumberOfDevices(mDevicesToUse)) {
			MIDIDeviceRef midiDevice = MIDIDeviceListGetDevice(mDevicesToUse, mNumDevicesFound);
			InterfaceState *ifs = new InterfaceState(mDriver, midiDevice, device, interface);
			mInterfaceStateList->push_back(ifs);
			++mNumDevicesFound;
			return true;	// keep device/interface open
		}
		return false;	// close device/interface
	}
	
	USBMIDIDriverBase *		mDriver;
	InterfaceStateList *	mInterfaceStateList;
	MIDIDeviceListRef		mDevicesToUse;
	ItemCount				mNumDevicesFound;
};

// __________________________________________________________________________________________________
USBMIDIDriverBase::USBMIDIDriverBase(CFUUIDRef factoryID) :
	MIDIDriver(factoryID)
{
}


USBMIDIDriverBase::~USBMIDIDriverBase()
{
}

// __________________________________________________________________________________________________
OSStatus	USBMIDIDriverBase::FindDevices(MIDIDeviceListRef devices)
{
	InterfaceLocator loc;
	
	loc.Find(devices, this);
	return noErr;
}

// __________________________________________________________________________________________________
OSStatus	USBMIDIDriverBase::Start(MIDIDeviceListRef devices)
{
	InterfaceStarter starter;
	
	mInterfaceStateList = new InterfaceStateList;
	starter.Start(this, devices, mInterfaceStateList);

#if ANALYZE_THRU_TIMING
	gTimingAnalyzer.Clear();
#endif
	return noErr;
}

// __________________________________________________________________________________________________
OSStatus	USBMIDIDriverBase::Stop()
{
	if (mInterfaceStateList != NULL) {
		for (InterfaceStateList::iterator it = mInterfaceStateList->begin(); 
		it != mInterfaceStateList->end(); ++it) {
			InterfaceState *rs = *it;
			delete rs;
		}
		delete mInterfaceStateList;
		mInterfaceStateList = NULL;
	}
#if ANALYZE_THRU_TIMING
	gTimingAnalyzer.Dump();
#endif
	return noErr;
}

// __________________________________________________________________________________________________
OSStatus	USBMIDIDriverBase::Send(const MIDIPacketList *pktlist, void *endptRef1, void *endptRef2)
{
	InterfaceState *intf = (InterfaceState *)endptRef1;
#if ANALYZE_THRU_TIMING
	const MIDIPacket *pkt = &pktlist->packet[0];
	for (int i = pktlist->numPackets; --i >= 0; ) {
		if (pkt->timeStamp != 0) {
			UInt64 now = AudioGetCurrentHostTime();
			UInt32 micros = AudioConvertHostTimeToNanos(now - pkt->timeStamp) / 1000;
			gTimingAnalyzer.AddSample(micros);
		}
		pkt = MIDIPacketNext(pkt);
	}
#endif

	intf->Send(pktlist, (int)endptRef2);	// endptRef2 = port number

	return noErr;
}

// __________________________________________________________________________________________________
void	USBMIDIDriverBase::USBMIDIHandleInput(	InterfaceState *intf, 
												MIDITimeStamp when, 
												Byte *readBuf,
												ByteCount bufSize )
{
	Byte *src = readBuf, *srcend = src + bufSize;
	Byte pbuf[512];
	MIDIPacketList *pktlist = (MIDIPacketList *)pbuf;
	MIDIPacket *pkt = MIDIPacketListInit(pktlist);
	int prevCable = -1;	// signifies none
	bool insysex = false;
	int nbytes;
	
#if DUMP_INPUT
	if (src[0] == 0) return;
	printf("IN:  ");
#endif

	for ( ; src < srcend; src += 4) {
		if (src[0] == 0)
			break;		// done (is this according to spec or Roland-specific???)
		
#if DUMP_INPUT		
		printf("%02X %02X %02X %02X  ", src[0], src[1], src[2], src[3]);
#endif
		
		int cable = src[0] >> 4;
		if (prevCable != -1 && cable != prevCable) {
			MIDIReceived(intf->mSources[prevCable], pktlist);
			pkt = MIDIPacketListInit(pktlist);
			insysex = false;
		}
		prevCable = cable;
		int cin = src[0] & 0x0F;
		switch (cin) {
		case 0x0:		// reserved
		case 0x1:		// reserved
			break;
		case 0xF:		// single byte
			pkt = MIDIPacketListAdd(pktlist, sizeof(pbuf), pkt, when, 1, src + 1);
			break;
		case 0x2:		// 2-byte system common
		case 0xC:		// program change
		case 0xD:		// mono pressure
			pkt = MIDIPacketListAdd(pktlist, sizeof(pbuf), pkt, when, 2, src + 1);
			break;
		case 0x4:		// sysex starts or continues
			insysex = true;
			// --- fall ---
		case 0x3:		// 3-byte system common
		case 0x8:		// note-on
		case 0x9:		// note-off
		case 0xA:		// poly pressure
		case 0xB:		// control change
		case 0xE:		// pitch bend
			pkt = MIDIPacketListAdd(pktlist, sizeof(pbuf), pkt, when, 3, src + 1);
			break;
		case 0x5:		// single byte system-common, or sys-ex ends with one byte
			if (src[1] != 0xF7) {
				pkt = MIDIPacketListAdd(pktlist, sizeof(pbuf), pkt, when, 1, src + 1);
				break;
			}
			// --- fall ---
		case 0x6:		// sys-ex ends with two bytes
		case 0x7:		// sys-ex ends with three bytes
			nbytes = cin - 4;
			if (insysex) {
				insysex = false;
				// MIDIPacketListAdd will make a separate packet unnecessarily,
				// so do our own concatentation onto the current packet
				memcpy(&pkt->data[pkt->length], src + 1, nbytes);
				pkt->length += nbytes;
			} else {
				pkt = MIDIPacketListAdd(pktlist, sizeof(pbuf), pkt, when, nbytes, src + 1);
			}
			break;
		}
	}
	if (pktlist->numPackets > 0 && prevCable != -1) {
		MIDIReceived(intf->mSources[prevCable], pktlist);
#if DUMP_INPUT
		printf("\n");
#endif
	}
}

// __________________________________________________________________________________________________
// WriteQueue is an STL list of VLMIDIPacket's to be transmitted, presumably containing
// at least one element.
// Fill one USB buffer, destBuf, with a size of bufSize, with outgoing data in USB-MIDI format.
// Return the number of bytes written.
ByteCount	USBMIDIDriverBase::USBMIDIPrepareOutput(	InterfaceState *intf, 
														WriteQueue &writeQueue, 			
														Byte *destBuf, 
														ByteCount bufSize )
{
	Byte *dest = destBuf, *destend = dest + bufSize;
	
	while (true) {
		if (writeQueue.empty()) {
			// ??? should we be zeroing the entire remainder of the buffer?
			// that's what the MPU64 wants anyhow
			memset(dest, 0, destend - dest);
			return bufSize;	//dest - destBuf;
		}
		
		WriteQueueElem *wqe = &writeQueue.front();
		Byte cableNibble = wqe->portNum << 4;
		VLMIDIPacket *pkt = wqe->packet;
		Byte *src = pkt->data + wqe->bytesSent;
		Byte *srcend = &pkt->data[pkt->length];

		while (src < srcend && dest < destend) {
			Byte c = *src++;
			
			switch (c >> 4) {
			case 0x0: case 0x1: case 0x2: case 0x3:
			case 0x4: case 0x5: case 0x6: case 0x7:
				// data byte, presumably a sysex continuation
				dest[1] = c;
				if ((dest[2] = *src++) == 0xF7) {
					dest[0] = cableNibble | 6;		// sysex ends with following 2 bytes
					dest[3] = 0;
				} else if ((dest[3] = *src++) == 0xF7)
					dest[0] = cableNibble | 7;		// sysex ends with following 3 bytes
				else
					dest[0] = cableNibble | 4;		// sysex continues
				dest += 4;
				break;
			case 0x8:	// note-on
			case 0x9:	// note-off
			case 0xA:	// poly pressure
			case 0xB:	// control change
			case 0xE:	// pitch bend
				*dest++ = cableNibble | (c >> 4);
				*dest++ = c;
				*dest++ = *src++;
				*dest++ = *src++;
				break;
			case 0xC:	// program change
			case 0xD:	// mono pressure
				*dest++ = cableNibble | (c >> 4);
				*dest++ = c;
				*dest++ = *src++;
				*dest++ = 0;
				break;
			case 0xF:	// system message
				switch (c) {
				case 0xF0:	// sysex start
					*dest++ = cableNibble | 4;	// sysex start or continued
					*dest++ = c;
					*dest++ = *src++;
					*dest++ = *src++;
					break;
				case 0xF8:	// clock
				case 0xFA:	// start
				case 0xFB:	// continue
				case 0xFC:	// stop
				case 0xFE:	// active sensing
				case 0xFF:	// system reset
					*dest++ = cableNibble | 0xF;// 1-byte system realtime
					*dest++ = c;
					*dest++ = 0;
					*dest++ = 0;
					break;
				case 0xF6:	// tune request (0)
					*dest++ = cableNibble | 5;	// 1-byte system common
					*dest++ = c;
					*dest++ = 0;
					*dest++ = 0;
					break;
				case 0xF1:	// MTC (1)
				case 0xF3:	// song select (1)
					*dest++ = cableNibble | 2;	// 2-byte system common
					*dest++ = c;
					*dest++ = *src++;
					*dest++ = 0;
					break;
				case 0xF2:	// song pointer (2)
					*dest++ = cableNibble | 3;	// 3-byte system common
					*dest++ = c;
					*dest++ = *src++;
					*dest++ = *src++;
					break;
				default:
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
			} else
				wqe->bytesSent = src - pkt->data;
			
			if (dest > destend - 4) {
				// destBuf completely filled
				return dest - destBuf;
			}
			// we didn't fill the output buffer, is there more source data in the write queue?
		}
	}
}

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

#include "USBMIDIDriverBase.h"
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <CarbonCore/DriverServices.h>
#include <CarbonCore/Math64.h>

#define VERBOSE (DEBUG && 0)

// __________________________________________________________________________________________________

#define ANALYZE_THRU_TIMING 0
#if ANALYZE_THRU_TIMING

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

// must not pass this F0 or F7 or realtime
// returns number of data bytes which follow the status byte
int		MIDIDataBytes(Byte status)
{
	if (status >= 0x80 && status < 0xF0)
		return ((status & 0xE0) == 0xC0) ? 1 : 2;

	switch (status) {
	case 0xF1:		// MTC
		return 1;
	case 0xF2:		// song pointer
		return 2;
	case 0xF3:		// song select
		return 1;
	case 0xF6:
		return 0;	// tune request
	}

	fprintf(stderr, "MIDIEventLength: illegal status byte %02X\n", status);
	exit(-1);
}

// __________________________________________________________________________________________________

InterfaceState::InterfaceState(USBMIDIDriverBase *driver, MIDIDeviceRef midiDevice, 
	IOUSBDeviceRef usbDevice, XUSBInterface usbInterface) : mSources(NULL)
{
	mDriver = driver;
	mDevice = usbDevice;
	mInterface = usbInterface;

	InterfaceInfo intfInfo;
	GetInterfaceInfo(intfInfo);
	mReadBuf = new Byte[intfInfo.readBufSize];
	mWriteBuf = new Byte[intfInfo.writeBufSize];

	mInPipe = USBFindAndOpenPipe(usbInterface, intfInfo.inEndptType, kUSBIn);
	mOutPipe = USBFindAndOpenPipe(usbInterface, intfInfo.outEndptType, kUSBOut);

	if (mOutPipe) {
#if VERBOSE
		printf("starting MIDI, mOutPipe=0x%lX, mInPipe=0x%lX\n", (long)mOutPipe,
			(long)mInPipe);
#endif

		// start MIDI
		mDriver->StartInterface(this);
	}
	
	// now set up all the sources and destinations
	// $$$ this may be too specific; it assumes that every endpoint has 1 source and 1 destination
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

	pthread_mutex_init(&mWriteQueueMutex, NULL);
	pthread_cond_init(&mWriteQueueNonEmpty, NULL);

	#if USBMIDI_ASYNCIO
		DoRead();
	#else
		mWriteThread = new WriteThread(this);
		mWriteThread->Start();
		while (!mWriteThread->Running())
			sched_yield();
	
		mReadThread = new ReadThread(this);
		mReadThread->Start();
	#endif
}

InterfaceState::~InterfaceState()
{
	IOReturn ret;
	
	if (mOutPipe) {
		mDriver->StopInterface(this);
		IOUSBAbortPipe(mOutPipe);
		ret = IOUSBClosePipe(mOutPipe);
		if (ret) printerr("IOUSBClosePipe(mOutPipe)", ret);
	}
	if (mInPipe)
		IOUSBAbortPipe(mInPipe);	// should force read thread to die

	#if !USBMIDI_ASYNCIO
		mStopRequested = true;
		pthread_cond_signal(&mWriteQueueNonEmpty);
			// wake write thread up so that it will notice stop has been requested, and then exit

		while (mReadThread->Running())
			sched_yield();
		delete mReadThread;
		
		while (mWriteThread->Running())
			sched_yield();
		delete mWriteThread;
	#endif

	if (mInPipe) {
		// need to do this AFTER read has been aborted
		ret = IOUSBClosePipe(mInPipe);
		if (ret) printerr("IOUSBClosePipe(mInPipe)", ret);
	}
	if (mInterface)
		USBInterfaceDispose(mInterface);
	if (mDevice) {
		ret = IOUSBDisposeRef(mDevice);
		if (ret) printerr("IOUSBDisposeRef(mDevice)", ret);
	}
	pthread_mutex_destroy(&mWriteQueueMutex);
	pthread_cond_destroy(&mWriteQueueNonEmpty);
	
	delete[] mSources;
	delete[] mReadBuf;
	delete[] mWriteBuf;
#if VERBOSE
	printf("driver stopped MIDI\n");
#endif
}

void	InterfaceState::HandleInput(int readBufLength)
{
	AbsoluteTime now = UpTime();
//        printf("packetSize = %ld\n", readBufLength);
//        printf("mReadBuf[0-23]: ");
//        for(int i = 0; i < 24; i++)
//            printf("%02X ", mReadBuf[i]);
//        printf("\n");
	mDriver->HandleInput(this, UnsignedWideToUInt64(now), mReadBuf, readBufLength);
}

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
	pthread_mutex_unlock(&mWriteQueueMutex);
	pthread_cond_signal(&mWriteQueueNonEmpty);
}

// called from the separate write thread
void	InterfaceState::DoWrites()
{
	if (mOutPipe) {
		pthread_mutex_lock(&mWriteQueueMutex);
		
		while (true) {
			// wait for the signal that there's something to send
			pthread_cond_wait(&mWriteQueueNonEmpty, &mWriteQueueMutex);
			
			if (mStopRequested)
				break;
			
			// now that we're awake, keep running until we've exhausted the queue
			while (mWriteQueue.begin() != mWriteQueue.end()) {
				int msglen = mDriver->PrepareOutput(this, mWriteQueue, mWriteBuf);
				if (msglen > 0) {
					pthread_mutex_unlock(&mWriteQueueMutex);

					IOUSBWritePipe(mOutPipe, mWriteBuf, msglen);
/*for (int i = 0; i < msglen; ++i)
	printf("%02X ", mWriteBuf[i]);
printf("\n");*/

					pthread_mutex_lock(&mWriteQueueMutex);
				}
			}

			if (mStopRequested)
				break;
		}
	}
}

// __________________________________________________________________________________________________

#if !USBMIDI_ASYNCIO
void	ReadThread::Run()
{
	mRunning = true;
	InterfaceState *rs = mInterfaceState;
	InterfaceInfo intfInfo;
	rs->GetInterfaceInfo(intfInfo);
	
	if (rs->mInPipe) {
		while (true) {
			UInt32 packetSize = intfInfo.readBufSize;
			IOReturn ret = IOUSBReadPipe(rs->mInPipe, rs->mReadBuf, &packetSize);
			if (ret) {
				if (ret != kIOReturnAborted)
					printerr("IOUSBReadPipe", ret);
				break;
			}
			rs->HandleInput(packetSize);
		}
	}
//	printf("read thread terminating\n");
	mRunning = false;
}

void	WriteThread::Run()
{
	mRunning = true;
	mInterfaceState->DoWrites();
//	printf("write thread terminating\n");
	mRunning = false;
}

#endif

// __________________________________________________________________________________________________

// This class creates runtime states for interface instances, called from Start()
class InterfaceStarter : public USBDeviceLocator {
public:
	void			Start(USBMIDIDriverBase *driver, MIDIDeviceListRef devices, 
						InterfaceStateList *rsl)
	{
		mDriver = driver;
		mNumDevicesFound = 0;
		mDevicesToUse = devices;
		mInterfaceStateList = rsl;
		FindDevices(driver->GetUSBMatch());
	}

	virtual int		InterfaceIndexToUse(IOUSBDeviceRef device)
	{
		return mDriver->InterfaceIndexToUse(device);
	}

	virtual bool	FoundInterface(IOUSBDeviceRef usbDevice, XUSBInterface usbInterface)
	{
		// $$$ to support multiple device instances properly, we'd need more complex matching code
		if (mNumDevicesFound < MIDIDeviceListGetNumberOfDevices(mDevicesToUse)) {
			MIDIDeviceRef midiDevice = MIDIDeviceListGetDevice(mDevicesToUse, mNumDevicesFound);
			InterfaceState *ifs = new InterfaceState(mDriver, midiDevice, usbDevice, usbInterface);
			mInterfaceStateList->push_back(ifs);
			++mNumDevicesFound;
			return true;	// keep device/interface around
		}
		return false;	// close device/interface
	}
	
	USBMIDIDriverBase *	mDriver;
	InterfaceStateList	*mInterfaceStateList;
	MIDIDeviceListRef	mDevicesToUse;
	unsigned int		mNumDevicesFound;
};

// __________________________________________________________________________________________________

USBMIDIDriverBase::USBMIDIDriverBase(CFUUIDRef factoryID) :
	MIDIDriver(factoryID)
{
}

USBMIDIDriverBase::~USBMIDIDriverBase()
{
}

OSStatus	USBMIDIDriverBase::Start(MIDIDeviceListRef devices)
{
	// need to set endpoint refcons
	{
		InterfaceStarter starter;
		
		mInterfaceStateList = new InterfaceStateList;
		starter.Start(this, devices, mInterfaceStateList);
	}

#if 0 // test restarting
	Stop();
	{
		InterfaceStarter starter;
		
		mInterfaceStateList = new InterfaceStateList;
		starter.Start(this, devices, mInterfaceStateList);
	}
#endif
	
#if ANALYZE_THRU_TIMING
	gTimingAnalyzer.Clear();
#endif
	return noErr;
}

OSStatus	USBMIDIDriverBase::Stop()
{
	if (mInterfaceStateList) {
		for (InterfaceStateList::iterator it = mInterfaceStateList->begin(); it != mInterfaceStateList->end(); ++it) {
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

OSStatus	USBMIDIDriverBase::Send(const MIDIPacketList *pktlist, void *endptRef1, void *endptRef2)
{
	InterfaceState *intf = (InterfaceState *)endptRef1;
	intf->Send(pktlist, (int)endptRef2);	// endptRef2 = port number

#if 0 // ANALYZE_THRU_TIMING
				if (pkt->timeStamp != 0) {
					UInt64 now = AudioGetHWTime();
					UInt32 micros = AudioHWTimeToNanoseconds(now - pkt->timeStamp) / 1000;
					
					gTimingAnalyzer.AddSample(micros);
				}
#endif

	return noErr;
}

void	USBMIDIDriverBase::USBMIDIHandleInput(InterfaceState *intf, MIDITimeStamp when, Byte *readBuf,
											int bufSize)
{
	Byte *src = readBuf, *srcend = src + bufSize;
	Byte pbuf[512];
	MIDIPacketList *pktlist = (MIDIPacketList *)pbuf;
	MIDIPacket *pkt = MIDIPacketListInit(pktlist);
	int prevCable = -1;	// signifies none
	bool insysex = false;
	int nbytes;
	
	for ( ; src < srcend; src += 4) {
		if (src[0] == 0)
			break;		// done (is this according to spec or Roland-specific???)
		
//		printf("%02X %02X %02X %02X  ", src[0], src[1], src[2], src[3]);
		
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
//		printf("\n");
	}
}

// WriteQueue is an STL list of VLMIDIPacket's to be transmitted, presumably containing
// at least one element.
// Fill one USB buffer, destBuf, with a size of bufSize, with outgoing data in USB-MIDI format.
// Return the number of bytes written.
int		USBMIDIDriverBase::USBMIDIPrepareOutput(InterfaceState *intf, WriteQueue &writeQueue, 
							Byte *destBuf, int bufSize)
{
	Byte *dest = destBuf, *destend = dest + bufSize;
	
	while (true) {
		if (writeQueue.empty()) {
			// ??? should we be zeroing the entire remainder of the buffer?
			// that's what the MPU64 wants anyhow
			//*dest++ = 0;
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

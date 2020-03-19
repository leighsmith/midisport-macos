//
// MacOS X driver for MIDIMan MIDISPORT USB MIDI interfaces.
//
//    This class is a modification of the standard version specific to the MIDIMAN MIDISPORT USB devices.
//    This is necessary as the data format is not USB-MIDI, specifically, the data is communicated down
//    two endPoints, 2 and 4, using the upper nibble of the 4 byte packet to indicate either an even or
//    odd numbered port to use. It's slightly hairy, but a diff against the original will reveal there
//    isn't much that is special. All that has been changed is marked MIDISPORT_SPECIFIC. Unfortunately
//    it's not really possible to do this by subclassing, since the MIDISPORT class never has a chance
//    to create an InterfaceState subclass that is specific to MIDISPORT devices and hand it over to
//    our superclass.
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
#include <AssertMacros.h>
#include "USBMIDIDriverBase.h"
#include <algorithm>

#if DEBUG
	#include <stdio.h>
	//#define VERBOSE 1
	//#define DUMP_INPUT 1			// only works in USBMIDIHandleInput
	//#define DUMP_OUTPUT 1
	//#define ANALYZE_THRU_TIMING 1
#endif

// __________________________________________________________________________________________________
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

IOBuffer::IOBuffer() :
	mBuffer(NULL),
	mMemory(NULL)
{
}

IOBuffer::~IOBuffer()
{
	if (mMemory)
		delete[] mMemory;
}

void	IOBuffer::Allocate(UInt32 size)
{
	mBuffer = mMemory = new Byte[size];
}

// __________________________________________________________________________________________________

Mutex::Mutex()
{
	__Verify_noErr(pthread_mutex_init(&mMutex, NULL));
	mOwner = 0;
}

Mutex::~Mutex()
{
	pthread_mutex_destroy(&mMutex);
}
	
bool	Mutex::Lock()
{
	pthread_t thisThread = pthread_self();
	
	if (mOwner == thisThread)
		return false;	// already acquired
	
	__Require_noErr(pthread_mutex_lock(&mMutex), done);
	mOwner = thisThread;
	return true;	// did acquire lock

done:
	return false;	// error, didn't acquire lock
}

void	Mutex::Unlock()
{
	__Require_String(mOwner == pthread_self(), done, "non-owner thread is unlocking mutex");
	mOwner = 0;
	pthread_mutex_unlock(&mMutex);
done: ;
}

// __________________________________________________________________________________________________

// the device and interface are assumed to have been opened
InterfaceState::InterfaceState(	USBMIDIDriverBase *			driver, 
								MIDIDeviceRef 				midiDevice, 
								io_service_t				ioDevice,
								IOUSBDeviceInterface **		usbDevice,
								IOUSBInterfaceInterface **	usbInterface) :
	mSources(NULL)
{
	UInt8	   		numEndpoints, pipeNum, direction, transferType, interval;
	UInt16			pipeIndex, maxPacketSize; 		
	
	mDriver = driver;
	mMidiDevice = midiDevice;
	mIODevice = ioDevice;
	mDevice = usbDevice;
	mInterface = usbInterface;
	mHaveInPipe = false;
	mHaveOutPipe1 = false;
 	mHaveOutPipe2 = false;
 
	GetInterfaceInfo(mInterfaceInfo); 	// Get endpoint types and buffer sizes
	mReadBuf.Allocate(mInterfaceInfo.readBufferSize);
	mWriteBuf1.Allocate(mInterfaceInfo.writeBufferSize); // assumes both buffers are equal sized.
	mWriteBuf2.Allocate(mInterfaceInfo.writeBufferSize);

#if VERBOSE
	printf("mReadBuf=0x%lx, mWriteBuf1=0x%lx, mWriteBuf2=0x%lx\n", (long)mReadBuf.Buffer(), (long)mWriteBuf1.Buffer(), (long)mWriteBuf2.Buffer());
#endif

	numEndpoints = 0;
	__Require_noErr((*mInterface)->GetNumEndpoints(mInterface, &numEndpoints), errexit);
		// find the number of endpoints for this interface

	for (pipeIndex = 1; pipeIndex <= numEndpoints; ++pipeIndex) { 
		__Require_noErr((*mInterface)->GetPipeProperties(mInterface, pipeIndex, &direction, &pipeNum, &transferType, &maxPacketSize, &interval), nextPipe);
		#if VERBOSE 
			printf("pipe index %d: dir=%d, num=%d, tt=%d, maxPacketSize=%d, interval=%d\n", pipeIndex,  direction, pipeNum, transferType, maxPacketSize, interval);
		#endif
                // MIDISPORT_SPECIFIC
                // The MIDIMan MIDISPORT devices output different ports via different endPoints.
                // This is quite a logical approach but unfortunately differs from the USB-MIDI spec.
		if (direction == kUSBOut && pipeNum == 2) { // MIDIMan machines are fixed to their endPoints
			mOutPipe1 = pipeIndex; 
			mHaveOutPipe1 = true;
		} else if (direction == kUSBOut && pipeNum == 4) { // MIDIMan machines are fixed to their endPoints
			mOutPipe2 = pipeIndex; 
			mHaveOutPipe2 = true;
		} else if (direction == kUSBIn && pipeNum == 1) { // MIDIMan machines are fixed to their endPoints
			mInPipe = pipeIndex;  
			mHaveInPipe = true;
		} else if (direction == kUSBIn && pipeNum == 2 && !mHaveInPipe) { 
                        // MIDIMan MIDISPORT 8x8 uses pipeNum == 2, yet all the other MIDISPORTs use 1?
                        // This is not right, we should be checking that we are actually using an 8x8,
                        // but until the comms mechanism is in place between the object 
                        // (and this whole member function should be virtual anyway), we fudge it...
			mInPipe = pipeIndex;  
			mHaveInPipe = true;
		}
nextPipe: ;
	}
	// don't go any further if we don't have a valid pipe
	__Require(mHaveOutPipe1 || mHaveOutPipe2 || mHaveInPipe, errexit);

	#if VERBOSE
		printf("starting MIDI, mOutPipe1=0x%lX, mOutPipe2=0x%lX, mInPipe=0x%lX\n", (long)mOutPipe1, (long)mOutPipe2, (long)mInPipe);
	#endif
	
	// now set up all the sources and destinations
	// !!! this may be too specific; it assumes that every entity has 1 source and 1 destination
	// if this assumption is false, more specific code is needed
	// !!! could factor this into a virtual method with a default implementation.
	mNumEntities = MIDIDeviceGetNumberOfEntities(midiDevice);
	mSources = new MIDIEndpointRef[mNumEntities];

	for (ItemCount ient = 0; ient < (ItemCount)mNumEntities; ++ient) {
		MIDIEntityRef ent = MIDIDeviceGetEntity(midiDevice, ient);

		// destination refCons: output pipe, cable number (0-based)
		if (ient < MIDIEntityGetNumberOfDestinations(ent)) {
		MIDIEndpointRef dest = MIDIEntityGetDestination(ent, 0);
		MIDIEndpointSetRefCons(dest, this, (void *)ient);
		}
		if (ient < MIDIEntityGetNumberOfSources(ent))
		mSources[ient] = MIDIEntityGetSource(ent, 0);
		else
			mSources[ient] = NULL;
	}
	mWriteCable = 0xFF;
	mReadCable = 0;
	mInSysEx = false;
	mRunningStatus = 0;

	{
		CFRunLoopRef ioRunLoop = MIDIGetDriverIORunLoop();
		CFRunLoopSourceRef source;
		
		if (ioRunLoop != NULL) {
			source = (*mInterface)->GetInterfaceAsyncEventSource(mInterface);
			if (source == NULL) {
				__Require_noErr((*mInterface)->CreateInterfaceAsyncEventSource(mInterface, &source), errexit);
				__Require(source != NULL, errexit);
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
        // MIDISPORT_SPECIFIC
	if (mHaveOutPipe1 || mHaveOutPipe2 || mHaveInPipe)
		mDriver->StopInterface(this);

	if (mHaveOutPipe1) {
		__Verify_noErr((*mInterface)->AbortPipe(mInterface, mOutPipe1));
        }
	if (mHaveOutPipe2) {
		__Verify_noErr((*mInterface)->AbortPipe(mInterface, mOutPipe2));
        }
        // MIDISPORT_SPECIFIC
        
	if (mHaveInPipe)
		__Verify_noErr((*mInterface)->AbortPipe(mInterface, mInPipe)); 

	CFRunLoopRef ioRunLoop = MIDIGetDriverIORunLoop();
	CFRunLoopSourceRef source;
	
	if (ioRunLoop != NULL) {
		source = (*mInterface)->GetInterfaceAsyncEventSource(mInterface);
		if (source != NULL && CFRunLoopContainsSource(ioRunLoop, source, kCFRunLoopDefaultMode))
			CFRunLoopRemoveSource(ioRunLoop, source, kCFRunLoopDefaultMode);
	}
	
	if (mInterface) {
		__Verify_noErr((*mInterface)->USBInterfaceClose(mInterface));
		(*mInterface)->Release(mInterface);
	}
	
	if (mDevice) {
		__Verify_noErr((*mDevice)->USBDeviceClose(mDevice));
		(*mDevice)->Release(mDevice);
	}
	
	delete[] mSources;

#if VERBOSE
	printf("driver stopped MIDI\n");
#endif
}

// __________________________________________________________________________________________________
void	InterfaceState::HandleInput(ByteCount bytesReceived)
{
	AbsoluteTime now = UpTime();
//	printf("bytesReceived = %ld\n", bytesReceived);
//	printf("mReadBuf[0-23]: ");
//	for (int i = 0; i < 24; i++)
//		printf("%02X ", mReadBuf[i]);
//	   printf("\n");
	mDriver->HandleInput(this, UnsignedWideToUInt64(now), mReadBuf, bytesReceived);
}

// __________________________________________________________________________________________________
void	InterfaceState::Send(const MIDIPacketList *pktlist, int portNumber)
{
	bool shouldUnlock = mWriteQueueMutex.Lock();
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
	if (shouldUnlock)
		mWriteQueueMutex.Unlock();
}

// __________________________________________________________________________________________________

void	InterfaceState::DoRead()
{
	if (mHaveInPipe) {
		__Verify_noErr((*mInterface)->ReadPipeAsync(mInterface, mInPipe, mReadBuf, mInterfaceInfo.readBufferSize, ReadCallback, this));
	}
}

// this is the IOAsyncCallback (static method)
void	InterfaceState::ReadCallback(void *refcon, IOReturn asyncReadResult, void *arg0)
{
	if (asyncReadResult == kIOReturnAborted) goto done;
	__Require_noErr(asyncReadResult, done);
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

// whole function is MIDISPORT_SPECIFIC
// must only be called with mWriteQueueMutex acquired and mWritePending false
void	InterfaceState::DoWrite()
{
	if (mHaveOutPipe1 || mHaveOutPipe2) {
		if (!mWriteQueue.empty()) {
			ByteCount msglen1 = 0, msglen2 = 0;
                        mDriver->PrepareOutput(this, mWriteQueue, mWriteBuf1, &msglen1, mWriteBuf2, &msglen2);
			if (msglen1 > 0) {
#if DUMP_OUTPUT
                                IOReturn pipeStatus;
                                
				printf("OUT1, %ld: ", msglen1);
				for (ByteCount i = 0; i < msglen1; i += 4) {
					//if (mWriteBuf1[i+3] == 0)
					//	break;
					printf("%02X %02X %02X %02X ", mWriteBuf1[i], mWriteBuf1[i+1], mWriteBuf1[i+2], mWriteBuf1[i+3]);
				}
				printf("\n");
                                pipeStatus = (*mInterface)->GetPipeStatus(mInterface, mOutPipe1);
                                printf("mInterface = 0x%x, pipeStatus = 0x%x\n", (unsigned int) mInterface, pipeStatus);
#endif
				mWritePending = true;
				__Verify_noErr((*mInterface)->WritePipeAsync(mInterface, mOutPipe1, mWriteBuf1, msglen1, WriteCallback, this));
			}
			if (msglen2 > 0) {
#if DUMP_OUTPUT
				printf("OUT2, %ld: ", msglen2);
				for (ByteCount i = 0; i < msglen2; i += 4) {
					//if (mWriteBuf2[i+3] == 0)
					//	break;
					printf("%02X %02X %02X %02X ", mWriteBuf2[i], mWriteBuf2[i+1], mWriteBuf2[i+2], mWriteBuf2[i+3]);
				}
				printf("\n");
#endif
                                mWritePending = true;
                                __Verify_noErr((*mInterface)->WritePipeAsync(mInterface, mOutPipe2, mWriteBuf2, msglen2, WriteCallback, this));
                        }
		}
	}
}

// this is the IOAsyncCallback (static method)
void	InterfaceState::WriteCallback(void *refcon, IOReturn asyncWriteResult, void *arg0)
{
	__Require_noErr(asyncWriteResult, done);
	{
		InterfaceState *self = (InterfaceState *)refcon;
		bool shouldUnlock = self->mWriteQueueMutex.Lock();
		// chain another write
		self->mWritePending = false;
		self->DoWrite();
		if (shouldUnlock)
			self->mWriteQueueMutex.Unlock();
	}
done:
	;
}

// __________________________________________________________________________________________________
// This class finds interface instances, called from FindDevices()
class InterfaceLocator : public USBDeviceManager {
public:
	void			Find(MIDIDeviceListRef devices, USBMIDIDriverBase *driver)
	{
		mFoundDeviceList = devices;
		mDriver = driver;
		ScanDevices();
	}
	
	virtual bool	MatchDevice(		IOUSBDeviceInterface **	device,
										UInt16					devVendor,
										UInt16					devProduct )
	{
		return mDriver->MatchDevice(device, devVendor, devProduct);
	}
	
	virtual void	GetInterfaceToUse(	IOUSBDeviceInterface **	device,
										UInt8 &					outInterfaceNumber, 
										UInt8 &					outAltSetting )
	{
		mDriver->GetInterfaceToUse(device, outInterfaceNumber, outAltSetting);
	}

	virtual bool	FoundInterface(		io_service_t				ioDevice,
										io_service_t				ioInterface,
										IOUSBDeviceInterface **		device,
										IOUSBInterfaceInterface **	interface,
										UInt16						devVendor,
										UInt16						devProduct,
										UInt8						interfaceNumber,
										UInt8						altSetting )
	{
#if VERBOSE
        printf("InterfaceLocator::FoundInterface\n");
#endif
		MIDIDeviceRef dev = mDriver->CreateDevice(ioDevice, ioInterface, device, interface, devVendor, devProduct, interfaceNumber, altSetting);
		if (dev != NULL)
			MIDIDeviceListAddDevice(mFoundDeviceList, dev);
		return false;		// don't keep device/interface open
	}
private:
	MIDIDeviceListRef	mFoundDeviceList;
	USBMIDIDriverBase *	mDriver;
};

// __________________________________________________________________________________________________
// This class creates runtime states for interface instances, created in Start(), deleted in Stop()
class InterfaceRunner : public USBDeviceManager {
public:
	InterfaceRunner(	USBMIDIDriverBase *	driver,
						MIDIDeviceListRef	devices ) :
		USBDeviceManager(CFRunLoopGetCurrent()),
		mDriver(driver),
		mInitialDeviceList(devices),
		mNumDevicesFound(0)
	{
		int nDevs = MIDIDeviceListGetNumberOfDevices(mInitialDeviceList);

#if V2_MIDI_DRIVER_SUPPORT
		if (driver->mVersion >= 2) {
			// mark everything previously present as offline
			for (int i = 0; i < nDevs; ++i) {
				MIDIDeviceRef midiDevice = MIDIDeviceListGetDevice(mInitialDeviceList, i);
				MIDIObjectSetIntegerProperty(midiDevice, kMIDIPropertyOffline, true);
			}
		}
#endif

		if (driver->mVersion >= 2 || nDevs > 0)
			// v1: don't bother doing any work if there are no devices in previous scan
			// v2: always scan, to pick up new devices
			ScanDevices();
	}
	
	~InterfaceRunner()
	{
		for (InterfaceStateList::iterator it = mInterfaceStateList.begin(); 
		it != mInterfaceStateList.end(); ++it) {
			InterfaceState *rs = *it;
			delete rs;
		}
	}

	virtual bool	MatchDevice(		IOUSBDeviceInterface **	device,
										UInt16					devVendor,
										UInt16					devProduct )
	{
		return mDriver->MatchDevice(device, devVendor, devProduct);
	}
	
	virtual void	GetInterfaceToUse(	IOUSBDeviceInterface **device, 
										UInt8 &outInterfaceNumber, 
										UInt8 &outAltSetting ) 
	{
		mDriver->GetInterfaceToUse(device, outInterfaceNumber, outAltSetting);
	}

	virtual bool	FoundInterface(		io_service_t				ioDevice,
										io_service_t				ioInterface,
										IOUSBDeviceInterface **		device,
										IOUSBInterfaceInterface **	interface,
										UInt16						devVendor,
										UInt16						devProduct,
										UInt8						interfaceNumber,
										UInt8						altSetting )
	{
		MIDIDeviceRef midiDevice = NULL;

		#if VERBOSE
			printf("InterfaceRunner::FoundInterface, ioDevice 0x%X, driver version %d\n", (int)ioDevice, mDriver->mVersion);
		#endif

#if V2_MIDI_DRIVER_SUPPORT
		if (mDriver->mVersion >= 2) {
			bool deviceInSetup = false;
			int nDevs;
			UInt32 locationID;
			UInt32 vendorProduct = ((UInt32)devVendor << 16) | devProduct;
			OSStatus err;
			MIDIDeviceListRef curDevices;
			
			__Require_noErr((*device)->GetLocationID(device, &locationID), errexit);

			// see if it's already in the setup, matching by locationID and productID
			curDevices = MIDIGetDriverDeviceList(mDriver->Self());
			nDevs = MIDIDeviceListGetNumberOfDevices(curDevices);
                        #if VERBOSE
                            printf("nDevs = %d, locationID = 0x%x\n", nDevs, (unsigned int) locationID);
			#endif
			for (int i = 0; i < nDevs; ++i) {
				SInt32 prevDevLocation, prevVendorProduct;
				midiDevice = MIDIDeviceListGetDevice(curDevices, i);
				err = MIDIObjectGetIntegerProperty(midiDevice, kUSBLocationProperty, &prevDevLocation);
				if (!err && (UInt32)prevDevLocation == locationID) {
					err = MIDIObjectGetIntegerProperty(midiDevice, kUSBVendorProductProperty, &prevVendorProduct);
					if (!err && (UInt32)prevVendorProduct == vendorProduct) {
						deviceInSetup = true;
						break;
					}
				}
			}
			MIDIDeviceListDispose(curDevices);
			if (!deviceInSetup) {
				#if VERBOSE
					printf("creating new device\n");
				#endif
				midiDevice = mDriver->CreateDevice(ioDevice, ioInterface, device, interface, devVendor, devProduct, interfaceNumber, altSetting);
				__Require(midiDevice != NULL, errexit);
				MIDIObjectSetIntegerProperty(midiDevice, kUSBLocationProperty, locationID);
				MIDIObjectSetIntegerProperty(midiDevice, kUSBVendorProductProperty, vendorProduct);

				__Require_noErr(MIDISetupAddDevice(midiDevice), errexit);
			} else {
				#if VERBOSE
					printf("old device found\n");
				#endif
			}
			InterfaceState *ifs = new InterfaceState(mDriver, midiDevice, ioDevice, device, interface);
			mInterfaceStateList.push_back(ifs);
                        #if VERBOSE
                            printf("marking midiDevice online\n");
                        #endif
			MIDIObjectSetIntegerProperty(midiDevice, kMIDIPropertyOffline, false);
		}
#endif // V2_MIDI_DRIVER_SUPPORT

#if V1_MIDI_DRIVER_SUPPORT
		if (mDriver->mVersion == 1) {
			// We're trying to make matches between the devices that were found during 
			// an earlier search, and being provided to us in mInitialDeviceList,
			// vs. the devices that are now present.
			
			// But this is designed for the simplistic case of only one device having been found
			// earlier, and 0 or 1 devices being present now.
			// To support multiple device instances properly, we'd need more complex matching code.
			if (mNumDevicesFound < MIDIDeviceListGetNumberOfDevices(mInitialDeviceList)) {
				midiDevice = MIDIDeviceListGetDevice(mInitialDeviceList, mNumDevicesFound);
				InterfaceState *ifs = new InterfaceState(mDriver, midiDevice, ioDevice, device, interface);
				mInterfaceStateList.push_back(ifs);
				++mNumDevicesFound;
				return true;	// keep device/interface open
			}
			return false;	// close device/interface
		}
#endif

		return true;		// keep device/interface open
errexit:
		return false;
	}
	
	virtual void	DeviceRemoved(io_service_t removedDevice)
	{
		for (InterfaceStateList::iterator it = mInterfaceStateList.begin(); 
		it != mInterfaceStateList.end(); ++it) {
			InterfaceState *rs = *it;
			if (rs->mIODevice == removedDevice) {
				#if VERBOSE
					printf("shutting down removed device 0x%X\n", (int)removedDevice);
				#endif
				MIDIObjectSetIntegerProperty(rs->mMidiDevice, kMIDIPropertyOffline, true);
				delete rs;
				mInterfaceStateList.erase(it);
				break;
			}
		}
	}
	
    typedef std::vector<InterfaceState *> InterfaceStateList;

	USBMIDIDriverBase *		mDriver;
	InterfaceStateList		mInterfaceStateList;
	MIDIDeviceListRef		mInitialDeviceList;
	ItemCount				mNumDevicesFound;
};

// __________________________________________________________________________________________________
USBMIDIDriverBase::USBMIDIDriverBase(CFUUIDRef factoryID) :
	MIDIDriver(factoryID),
	mInterfaceRunner(NULL)
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
#if VERBOSE
    printf("creating new interface runner in USBMIDIDriverBase::Start\n");
#endif
	mInterfaceRunner = new InterfaceRunner(this, devices);

#if ANALYZE_THRU_TIMING
	gTimingAnalyzer.Clear();
#endif
	return noErr;
}

// __________________________________________________________________________________________________
OSStatus	USBMIDIDriverBase::Stop()
{
	delete mInterfaceRunner;
	mInterfaceRunner = NULL;

#if ANALYZE_THRU_TIMING
	gTimingAnalyzer.Dump();
#endif
	return noErr;
}

// __________________________________________________________________________________________________
OSStatus	USBMIDIDriverBase::Send(const MIDIPacketList *pktlist, void *endptRef1, void *endptRef2)
{
	InterfaceState *intf = (InterfaceState *)endptRef1;
	if (intf == NULL) return kMIDIUnknownEndpoint;
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
		
		// support single-entity devices that seem to use an arbitrary cable number
		// (besides which, it's good to have range-checking)
		if (cable < 0) cable = 0;
		else if (cable >= intf->mNumEntities) cable = intf->mNumEntities - 1;
		
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
		case 0x8:		// note-off
		case 0x9:		// note-on
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
			// the MPU64 wants a full 4 bytes of zeroes for termination, not just 1
			*dest++ = 0;
			*dest++ = 0;
			*dest++ = 0;
			*dest++ = 0;
			return dest - destBuf;
			
			//return bufSize;	//dest - destBuf;
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
				// F0 is also handled the same way
inSysEx:
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
			case 0x8:	// note-off
			case 0x9:	// note-on
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
					goto inSysEx;
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
				case 0xF7:	// EOX
					*dest++ = cableNibble | 5;	// 1-byte system common or sysex ends with one byte
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

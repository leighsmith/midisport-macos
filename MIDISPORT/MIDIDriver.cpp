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

// A CFPlugin MIDIDriver using a C interface to CFPlugin, but calling a C++ class to do the work.

#include <CoreMIDIServer/MIDIDriver.h>
#include <stdio.h>


// Implementation of the IUnknown QueryInterface function.
static HRESULT MIDIDriverQueryInterface(MIDIDriverRef ref, REFIID iid, LPVOID *ppv) 
{
	// Create a CoreFoundation UUIDRef for the requested interface.
	CFUUIDRef interfaceID = CFUUIDCreateFromUUIDBytes( NULL, iid );

	// Test the requested ID against the valid interfaces.
	if (CFEqual(interfaceID, kMIDIDriverInterfaceID)) {
		// If the MIDIDriverInterface was requested, bump the ref count,
		// set the ppv parameter equal to the instance, and
		// return good status.
		MIDIDriver *self = GetMIDIDriver(ref);
		self->mInterface->AddRef(self);
		*ppv = &self->mInterface;
		CFRelease(interfaceID);
		return S_OK;
	} else if (CFEqual(interfaceID, IUnknownUUID)) {
		// If the IUnknown interface was requested, same as above.
		MIDIDriver *self = GetMIDIDriver(ref);
		self->mInterface->AddRef(self);
		*ppv = &self->mInterface;
		CFRelease(interfaceID);
		return S_OK;
	} else {
		// Requested interface unknown, bail with error.
		*ppv = NULL;
		CFRelease(interfaceID);
		return E_NOINTERFACE;
	}
}
// return value ppv is a pointer to a pointer to the interface


// Implementation of reference counting for this type.
// Whenever an interface is requested, bump the refCount for
// the instance. NOTE: returning the refcount is a convention
// but is not required so don't rely on it.
static ULONG MIDIDriverAddRef(MIDIDriverRef ref) 
{
	MIDIDriver *self = GetMIDIDriver(ref);

	return ++self->mRefCount;
}

// When an interface is released, decrement the refCount.
// If the refCount goes to zero, deallocate the instance.
static ULONG MIDIDriverRelease(MIDIDriverRef ref) 
{
	MIDIDriver *self = GetMIDIDriver(ref);

	if (--self->mRefCount == 0) {
		delete self;
		return 0;
	} else
		return self->mRefCount;
}

OSStatus	MIDIDriverFindDevices(MIDIDriverRef self, MIDIDeviceListRef devList)
{
	return GetMIDIDriver(self)->FindDevices(devList);
}

OSStatus	MIDIDriverStart(MIDIDriverRef self, MIDIDeviceListRef devList)
{
	return GetMIDIDriver(self)->Start(devList);
}

OSStatus	MIDIDriverStop(MIDIDriverRef self)
{
	return GetMIDIDriver(self)->Stop();
}

OSStatus	MIDIDriverConfigure(MIDIDriverRef self, MIDIDeviceRef device)
{
	return GetMIDIDriver(self)->Configure(device);
}

OSStatus	MIDIDriverSend(MIDIDriverRef self, const MIDIPacketList *pktlist, 
				void *destRefCon1, void *destRefCon2)
{
	return GetMIDIDriver(self)->Send(pktlist, destRefCon1, destRefCon2);
}

OSStatus	MIDIDriverEnableSource(MIDIDriverRef self, MIDIEndpointRef src, Boolean enabled)
{
	return GetMIDIDriver(self)->EnableSource(src, enabled);
}

// The MIDIDriverInterface function table.
static MIDIDriverInterface MIDIDriverInterfaceFtbl = {
	NULL, // Required padding for COM
	MIDIDriverQueryInterface, // These three are the required COM functions
	MIDIDriverAddRef,
	MIDIDriverRelease,
	//
	// these are the MIDIDriver methods
	MIDIDriverFindDevices,
	MIDIDriverStart,
	MIDIDriverStop,
	MIDIDriverConfigure,
	MIDIDriverSend,
	MIDIDriverEnableSource
};

MIDIDriver::MIDIDriver(CFUUIDRef factoryID)
{
	// Point to the function table
	mInterface = &MIDIDriverInterfaceFtbl;

	// Retain and keep an open instance refcount
	// for each factory.
	mFactoryID = factoryID;
	CFPlugInAddInstanceForFactory(factoryID);

	mRefCount = 1;
}

MIDIDriver::~MIDIDriver()
{
	if (mFactoryID) {
		CFPlugInRemoveInstanceForFactory(mFactoryID);
		CFRelease(mFactoryID);
	}
}

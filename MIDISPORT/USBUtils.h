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

#ifndef __USBUtils_h__
#define __USBUtils_h__

#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>


#define USBUTILS_OLD_API 0
	// some of the IOKit USB functions are deprecated, but there
	// are bugs in the newer ones.  Abstract the differences
	// so that we can switch over easily in the future.

#if USBUTILS_OLD_API
	typedef IOUSBInterface *	XUSBInterface;
#else
	typedef IOUSBInterfaceRef	XUSBInterface;
#endif

class USBDeviceLocator {
public:
	int				FindDevices(IOUSBMatch *match);
	
	virtual int		InterfaceIndexToUse(IOUSBDeviceRef device) = 0;
	
	virtual bool	FoundInterface(IOUSBDeviceRef device, XUSBInterface interface) = 0;
						// return true to keep the device and interface open
};

void			USBInterfaceDispose(XUSBInterface interface);

IOUSBPipeRef	USBFindAndOpenPipe(XUSBInterface interface, UInt8 type, UInt8 direction);

void			printerr(char *func, IOReturn ret);

#endif // __USBUtils_h__

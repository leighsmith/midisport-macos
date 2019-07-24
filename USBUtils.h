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

#ifndef __USBUtils_h__
#define __USBUtils_h__

#include <IOKit/usb/IOUSBLib.h>

// USBDeviceManager
// Abstract base class to locate USB devices.
class USBDeviceManager {
public:
	USBDeviceManager(CFRunLoopRef notifyRunLoop = NULL);
						// the run loop is the one in which add/remove device
						// notifications are received.  may be null to disable
						// plug and play.
	virtual ~USBDeviceManager();
	
	void			ScanDevices();
						// Scans through all of the USB devices in the IORegistry.
	
protected:
	virtual bool	MatchDevice(		IOUSBDeviceInterface **	device,
										UInt16					devVendor,
										UInt16					devProduct ) = 0;
						// Called from ScanDevices.  If this returns true, the
						// device is subsequently opened and configured, and
						// its interfaces are scanned. 
						
	virtual void	GetInterfaceToUse(	IOUSBDeviceInterface **	device, 
										UInt8 &					outInterfaceNumber,
										UInt8 &					outAltSetting ) = 0;
						// Once the device is opened and configured, this is
						// called to determine the interface number and
						// alternate setting which are to be searched for. 
						// Subsequently, ScanDevices will iterate through the
						// device's interfaces looking for the one specified.

	virtual bool	FoundInterface(		io_service_t				ioDevice,
										io_service_t				ioInterface,
										IOUSBDeviceInterface **		device,
										IOUSBInterfaceInterface **	interface,
										UInt16						devVendor,
										UInt16						devProduct,
										UInt8						interfaceNumber,
										UInt8						altSetting ) = 0;
						// When an interface matching the desired interface
						// number and alternate setting is located, this is
						// called.  It should return true to keep the device
						// and interface open; otherwise, they are closed.  If
						// true is returned, it is the subclass's responsibility 
						// to later close the device and interface.

	virtual void	DeviceRemoved(io_service_t ioDevice) { }
						// called when a device is terminated, if plug and play is enabled.

	void			DevicesAdded(io_iterator_t it);
	void			DevicesRemoved(io_iterator_t it);

	static void		DeviceAddCallback(void *refcon, io_iterator_t it);
	static void		DeviceRemoveCallback(void *refcon, io_iterator_t it);
		
	mach_port_t				mMasterDevicePort;
	IONotificationPortRef	mNotifyPort;
	CFRunLoopSourceRef		mRunLoopSource;
	io_iterator_t			mDeviceAddIterator;
	io_iterator_t			mDeviceRemoveIterator;
	bool					mIteratorsNeedEmptying;
	
	CFRunLoopRef			mRunLoop;
};

#endif // __USBUtils_h__

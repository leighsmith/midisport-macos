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
#include <CoreServices/CoreServices.h>	// we need Debugging.h, CF, etc.
#include <IOKit/IOCFPlugIn.h>
#include <CoreFoundation/CFNumber.h>
#include <mach/mach_port.h>
#include "USBUtils.h"

#if DEBUG || 1
	#include <stdio.h>
	#define VERBOSE 1
#endif

// _____________________________________________________________________________
USBDeviceManager::USBDeviceManager(CFRunLoopRef notifyRunLoop) :
	mRunLoop(notifyRunLoop)
{
	CFDictionaryRef matchingDict = NULL;

	mMasterDevicePort = NULL;
	mNotifyPort = NULL;
	mRunLoopSource = NULL;
	mDeviceAddIterator = NULL;
	mDeviceRemoveIterator = NULL;
	mIteratorsNeedEmptying = false;

    // Create a master port for communication with the I/O Kit.
	// This gets the master device mach port through which all messages
	// to the kernel go, and initiates communication with IOKit.
    // TODO MACH_PORT_NULL instead of bootstrap_port?
	__Require_noErr(IOMasterPort(bootstrap_port, &mMasterDevicePort), errexit);
	
	if (mRunLoop) {
        #if VERBOSE
            printf("mRunLoop true\n");
        #endif
        // To set up asynchronous notifications, create a notification port and
        // add its run loop event source to the program’s run loop
		mNotifyPort = IONotificationPortCreate(mMasterDevicePort);
		__Require(mNotifyPort != NULL, errexit);
		mRunLoopSource = IONotificationPortGetRunLoopSource(mNotifyPort);
		__Require(mRunLoopSource != NULL, errexit);
		//printf("mRunLoopSource retain count %d initially\n", (int)CFGetRetainCount(mRunLoopSource));
		
		CFRunLoopAddSource(mRunLoop, mRunLoopSource, kCFRunLoopDefaultMode);
		//printf("mRunLoopSource retain count %d after adding to run loop\n", (int)CFGetRetainCount(mRunLoopSource));
		
        // Set up matching dictionary for class IOUSBDevice and its subclasses
		matchingDict = IOServiceMatching(kIOUSBDeviceClassName); 
		__Require(matchingDict != NULL, errexit);
        
        // Retain additional dictionary references because each call to
        // IOServiceAddMatchingNotification consumes one reference
        matchingDict = (CFMutableDictionaryRef) CFRetain(matchingDict);
        matchingDict = (CFMutableDictionaryRef) CFRetain(matchingDict);

        // Now set up two notifications: one to be called when a raw device
        // is first matched by the I/O Kit and another to be called when the
        // device is terminated.
        // Notification of first match:
        // TODO Changed from kIOPublishNotification to kIOFirstMatchNotification
		__Require_noErr(IOServiceAddMatchingNotification(mNotifyPort, kIOFirstMatchNotification, matchingDict, DeviceAddCallback, this, &mDeviceAddIterator), errexit);

        // Notification of termination:
		__Require_noErr(IOServiceAddMatchingNotification(mNotifyPort, kIOTerminatedNotification, matchingDict, DeviceRemoveCallback, this, &mDeviceRemoveIterator), errexit);
		
		// empty the iterators
		mIteratorsNeedEmptying = true;
	}
	
errexit:
	if (matchingDict != NULL)
		CFRelease(matchingDict);
}

USBDeviceManager::~USBDeviceManager()
{
	if (mRunLoop != NULL && mRunLoopSource != NULL) {
		if (CFRunLoopContainsSource(mRunLoop, mRunLoopSource, kCFRunLoopDefaultMode)) {
			CFRunLoopRemoveSource(mRunLoop, mRunLoopSource, kCFRunLoopDefaultMode);
			//printf("mRunLoopSource retain count %d after removing from run loop\n", (int)CFGetRetainCount(mRunLoopSource));
		}
	}
	if (mRunLoopSource != NULL)
		CFRelease(mRunLoopSource);
	
	if (mDeviceAddIterator != (io_iterator_t) NULL)
		IOObjectRelease(mDeviceAddIterator);
	
	if (mDeviceRemoveIterator != (io_iterator_t) NULL)
		IOObjectRelease(mDeviceRemoveIterator);

	//if (mNotifyPort)
	//	IOObjectRelease(mNotifyPort);	// IONotificationPortDestroy crashes if called twice!

	if (mMasterDevicePort)
		mach_port_deallocate(mach_task_self(), mMasterDevicePort);
}

void USBDeviceManager::DeviceAddCallback(void *refcon, io_iterator_t it)
{
	((USBDeviceManager *)refcon)->DevicesAdded(it);
}

void USBDeviceManager::DeviceRemoveCallback(void *refcon, io_iterator_t it)
{
	((USBDeviceManager *)refcon)->DevicesRemoved(it);
}


// _____________________________________________________________________________
void	USBDeviceManager::ScanDevices()
{
	if (mMasterDevicePort == 0) return;

	if (mIteratorsNeedEmptying) {
#if VERBOSE
        printf("mIteratorsNeedEmptying in USBDeviceManager::ScanDevices()\n");
#endif
		mIteratorsNeedEmptying = false;
		DevicesAdded(mDeviceAddIterator);
		DevicesRemoved(mDeviceRemoveIterator);
		return;
	}

	io_iterator_t	devIter 			= NULL;
	CFDictionaryRef	matchingDict		= NULL;

	// Create a matching dictionary that specifies an IOService class match.
	matchingDict = IOServiceMatching(kIOUSBDeviceClassName); 
	__Require(matchingDict != NULL, errexit);
 
	// Find an IOService object currently registered by IOKit that match a 
	// dictionary, and get an iterator for it
	__Require_noErr(IOServiceGetMatchingServices(mMasterDevicePort, matchingDict, &devIter), errexit);
	matchingDict = NULL;	// Finish handoff of matchingDict
	
	DevicesAdded(devIter);

errexit:
	if (devIter != (io_iterator_t) NULL)
		IOObjectRelease(devIter);
		
	if (matchingDict)
		CFRelease(matchingDict); 
}

void	USBDeviceManager::DevicesRemoved(io_iterator_t devIter)
{
	io_service_t	ioDeviceObj			= NULL;

	while ((ioDeviceObj = IOIteratorNext(devIter)) != (io_iterator_t) NULL) {
#if VERBOSE
		printf("removed device 0x%X\n", (int)ioDeviceObj);
#endif
		DeviceRemoved(ioDeviceObj);
	}
}

void	USBDeviceManager::DevicesAdded(io_iterator_t devIter)
{
	io_service_t	ioDeviceObj			= NULL;

	while ((ioDeviceObj = IOIteratorNext(devIter)) != (io_iterator_t) NULL) {
		IOCFPlugInInterface 	**ioPlugin;
		IOUSBDeviceInterface 	**deviceIntf = NULL;
		IOReturn	 			kr;
		SInt32 					score;
		UInt16					devVendor;
		UInt16					devProduct;
		bool					keepOpen = false;

		// Get self pointer to device.
        kr = IOCreatePlugInInterfaceForService(ioDeviceObj,
                                               kIOUSBDeviceUserClientTypeID,
                                               kIOCFPlugInInterfaceID,
                                               &ioPlugin, &score);
        // TODO seems to return kIOReturnNoResources
        if ((kr != kIOReturnSuccess) || !ioPlugin) {
            printf("Unable to create a plug-in (%08x)\n", kr);
            continue;
        }
        // __Require_String(kr == kIOReturnSuccess, nextDevice, "Unable to create a plug-in");
        // Don't need the device object after intermediate plug-in is created
        kr = IOObjectRelease(ioDeviceObj);
        __Require(kr == kIOReturnSuccess, nextDevice);
        
        // Now create the device interface
		kr = (*ioPlugin)->QueryInterface(ioPlugin,
                                         CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID),
                                         (LPVOID *)&deviceIntf);
        // Don't need the intermediate plug-in after device interface is created
		(*ioPlugin)->Release(ioPlugin);
		ioPlugin = NULL;
		__Require_String(kr == kIOReturnSuccess, nextDevice, "QueryInterface failed");

		// Get device info
		__Require_noErr((*deviceIntf)->GetDeviceVendor(deviceIntf, &devVendor), nextDevice);
		__Require_noErr((*deviceIntf)->GetDeviceProduct(deviceIntf, &devProduct), nextDevice);
		
		if (MatchDevice(deviceIntf, devVendor, devProduct)) {
			bool						deviceOpen = false;
			UInt8						numConfigs;
			IOUSBConfigurationDescriptorPtr configDesc;	
			IOUSBInterfaceInterface		**interfaceIntf;
			io_iterator_t				intfIter = 0;
			UInt8 						intfNumber = 0;
			io_service_t 				ioInterfaceObj = NULL;
			IOUSBFindInterfaceRequest 	intfRequest;
			UInt8						desiredInterface = 0, desiredAltSetting = 0;
			#if VERBOSE
				int interfaceIndex		= 0;
			#endif

			// Found a device match
			#if VERBOSE
				printf ("scanning devices, matched device 0x%X: vendor 0x%x, product 0x%x\n", (int)ioDeviceObj, (int)devVendor, (int)devProduct);
			#endif

			// Make sure it has at least one configuration
			__Require_noErr((*deviceIntf)->GetNumberOfConfigurations(deviceIntf, &numConfigs), nextDevice);
			__Require(numConfigs > 0, nextDevice);

			// Get a pointer to the configuration descriptor for index 0
			__Require_noErr((*deviceIntf)->GetConfigurationDescriptorPtr(deviceIntf, 0, &configDesc), nextDevice);

			// Open the device
			__Require_noErr((*deviceIntf)->USBDeviceOpen(deviceIntf), nextDevice);
			deviceOpen = true;

			// Set the configuration
			//require_noerr((*deviceIntf)->GetConfiguration(deviceIntf, &curConfig), closeDevice);
			#if VERBOSE
				printf("Setting configuration %d\n", (int)configDesc->bConfigurationValue);
			#endif
			__Require_noErr((*deviceIntf)->SetConfiguration(deviceIntf, configDesc->bConfigurationValue), closeDevice);
			
			GetInterfaceToUse(deviceIntf, desiredInterface, desiredAltSetting);
				// Get the interface number for this device

			// Create the interface iterator
			intfRequest.bInterfaceClass		= kIOUSBFindInterfaceDontCare;
			intfRequest.bInterfaceSubClass	= kIOUSBFindInterfaceDontCare;
			intfRequest.bInterfaceProtocol	= kIOUSBFindInterfaceDontCare;
			intfRequest.bAlternateSetting	= desiredAltSetting;
			
			__Require_noErr((*deviceIntf)->CreateInterfaceIterator(deviceIntf, &intfRequest, &intfIter), closeDevice);

			while ((ioInterfaceObj = IOIteratorNext(intfIter)) != (io_iterator_t) NULL) {
				#if VERBOSE
					printf("interface index %d\n", interfaceIndex++);
				#endif
				__Require_noErr(IOCreatePlugInInterfaceForService(ioInterfaceObj, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &ioPlugin, &score), nextInterface);

				kr = (*ioPlugin)->QueryInterface(ioPlugin, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID *)&interfaceIntf);
				(*ioPlugin)->Release(ioPlugin);
				ioPlugin = NULL;
				__Require_String(kr == kIOReturnSuccess && interfaceIntf != NULL, nextInterface, "QueryInterface for USB interface failed");

				__Require_noErr((*interfaceIntf)->GetInterfaceNumber(interfaceIntf, &intfNumber), nextInterface);
				if (desiredInterface == intfNumber) {	// here's the one we want
					#if VERBOSE
						printf("found desired interface %d\n", intfNumber);
					#endif
					__Require_noErr((*interfaceIntf)->USBInterfaceOpen(interfaceIntf), nextInterface);
					keepOpen = FoundInterface(ioDeviceObj, ioInterfaceObj, deviceIntf, interfaceIntf, devVendor, devProduct, desiredInterface, desiredAltSetting);
                                        #if VERBOSE
                                            printf("keeping it open = %d\n", keepOpen);
                                        #endif
					if (!keepOpen)
						__Verify_noErr((*interfaceIntf)->USBInterfaceClose(interfaceIntf));
					break; // would never match more than one interface per device
				}
nextInterface:	IOObjectRelease(ioInterfaceObj);
				if (interfaceIntf != NULL && !keepOpen)
					(*interfaceIntf)->Release(interfaceIntf);
			} // end interface loop

closeDevice:
			if (intfIter != (io_iterator_t) NULL)
				IOObjectRelease(intfIter);
			if (deviceOpen && !keepOpen)
				__Verify_noErr((*deviceIntf)->USBDeviceClose(deviceIntf));
		} // end if vendor/product match
nextDevice:
		if (deviceIntf != NULL && !keepOpen)
			(*deviceIntf)->Release(deviceIntf);
		IOObjectRelease(ioDeviceObj);
	} 
	// Device iteration is complete.
}

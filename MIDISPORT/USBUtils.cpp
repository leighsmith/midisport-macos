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

#include <CoreServices/CoreServices.h>	// we need Debugging.h, CF, etc.
#include "USBUtils.h"
#include <IOKit/IOCFPlugIn.h>
#include <CoreFoundation/CFNumber.h>

#if DEBUG
	#include <stdio.h>
	//#define VERBOSE 1
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

	// This gets the master device mach port through which all messages
	// to the kernel go, and initiates communication with IOKit.
	require_noerr(IOMasterPort(bootstrap_port, &mMasterDevicePort), errexit);
	
	if (mRunLoop) {
		mNotifyPort = IONotificationPortCreate(mMasterDevicePort);
		require(mNotifyPort != NULL, errexit);
		mRunLoopSource = IONotificationPortGetRunLoopSource(mNotifyPort);
		require(mRunLoopSource != NULL, errexit);
		//printf("mRunLoopSource retain count %d initially\n", (int)CFGetRetainCount(mRunLoopSource));
		
		CFRunLoopAddSource(mRunLoop, mRunLoopSource, kCFRunLoopDefaultMode);
		//printf("mRunLoopSource retain count %d after adding to run loop\n", (int)CFGetRetainCount(mRunLoopSource));
		
		matchingDict = IOServiceMatching(kIOUSBDeviceClassName); 
		require(matchingDict != NULL, errexit);
		require_noerr(IOServiceAddMatchingNotification(mNotifyPort, kIOPublishNotification, matchingDict, DeviceAddCallback, this, &mDeviceAddIterator), errexit);
		matchingDict = NULL;	// Finish handoff of matchingDict

		matchingDict = IOServiceMatching(kIOUSBDeviceClassName); 
		require(matchingDict != NULL, errexit);
		require_noerr(IOServiceAddMatchingNotification(mNotifyPort, kIOTerminatedNotification, matchingDict, DeviceRemoveCallback, this, &mDeviceRemoveIterator), errexit);
		matchingDict = NULL;	// Finish handoff of matchingDict
		
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
	
	if (mDeviceAddIterator != NULL)
		IOObjectRelease(mDeviceAddIterator);
	
	if (mDeviceRemoveIterator != NULL)
		IOObjectRelease(mDeviceRemoveIterator);

	//if (mNotifyPort)
	//	IOObjectRelease(mNotifyPort);	// IONotificationPortDestroy crashes if called twice!

	if (mMasterDevicePort)
		mach_port_deallocate(mach_task_self(), mMasterDevicePort);
}

void		USBDeviceManager::DeviceAddCallback(void *refcon, io_iterator_t it)
{
	((USBDeviceManager *)refcon)->DevicesAdded(it);
}

void		USBDeviceManager::DeviceRemoveCallback(void *refcon, io_iterator_t it)
{
	((USBDeviceManager *)refcon)->DevicesRemoved(it);
}


// _____________________________________________________________________________
void	USBDeviceManager::ScanDevices()
{
	if (mMasterDevicePort == 0) return;

	if (mIteratorsNeedEmptying) {
		mIteratorsNeedEmptying = false;
		DevicesAdded(mDeviceAddIterator);
		DevicesRemoved(mDeviceRemoveIterator);
		return;
	}

	io_iterator_t	devIter 			= NULL;
	CFDictionaryRef	matchingDict		= NULL;

	// Create a matching dictionary that specifies an IOService class match.
	matchingDict = IOServiceMatching(kIOUSBDeviceClassName); 
	require(matchingDict != NULL, errexit);
 
	// Find an IOService object currently registered by IOKit that match a 
	// dictionary, and get an iterator for it
	require_noerr(IOServiceGetMatchingServices(mMasterDevicePort, matchingDict, &devIter), errexit);
	matchingDict = NULL;	// Finish handoff of matchingDict
	
	DevicesAdded(devIter);

errexit:
	if (devIter != NULL)
		IOObjectRelease(devIter);
		
	if (matchingDict)
		CFRelease(matchingDict); 
}

void	USBDeviceManager::DevicesRemoved(io_iterator_t devIter)
{
	io_service_t	ioDeviceObj			= NULL;

	while ((ioDeviceObj = IOIteratorNext(devIter)) != NULL) {
#if VERBOSE
		printf("removed device 0x%X\n", (int)ioDeviceObj);
#endif
		DeviceRemoved(ioDeviceObj);
	}
}

void	USBDeviceManager::DevicesAdded(io_iterator_t devIter)
{
	io_service_t	ioDeviceObj			= NULL;

	while ((ioDeviceObj = IOIteratorNext(devIter)) != NULL) {
		IOCFPlugInInterface 	**ioPlugin;
		IOUSBDeviceInterface 	**deviceIntf = NULL;
		IOReturn	 			kr;
		SInt32 					score;
		UInt16					devVendor;
		UInt16					devProduct;
		bool					keepOpen = false;

		// Get self pointer to device.
		require_noerr(IOCreatePlugInInterfaceForService(
			ioDeviceObj, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, 
			&ioPlugin, &score), nextDevice);
		
		kr = (*ioPlugin)->QueryInterface(ioPlugin, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID *)&deviceIntf);
		(*ioPlugin)->Release(ioPlugin);
		ioPlugin = NULL;
		require_string(kr == kIOReturnSuccess, nextDevice, "QueryInterface failed");

		// Get device info
		require_noerr((*deviceIntf)->GetDeviceVendor(deviceIntf, &devVendor), nextDevice);
		require_noerr((*deviceIntf)->GetDeviceProduct(deviceIntf, &devProduct), nextDevice);
		
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
				printf ("scanning devices, matched device 0x%X: vendor %d, product %d\n", (int)ioDeviceObj, (int)devVendor, (int)devProduct);
			#endif

			// Make sure it has at least one configuration
			require_noerr((*deviceIntf)->GetNumberOfConfigurations(deviceIntf, &numConfigs), nextDevice);
			require(numConfigs > 0, nextDevice);

			// Get a pointer to the configuration descriptor for index 0
			require_noerr((*deviceIntf)->GetConfigurationDescriptorPtr(deviceIntf, 0, &configDesc), nextDevice);

			// Open the device
			require_noerr((*deviceIntf)->USBDeviceOpen(deviceIntf), nextDevice);
			deviceOpen = true;

			// Set the configuration
			//require_noerr((*deviceIntf)->GetConfiguration(deviceIntf, &curConfig), closeDevice);
			#if VERBOSE
				printf("Setting configuration %d\n", (int)configDesc->bConfigurationValue);
			#endif
			require_noerr((*deviceIntf)->SetConfiguration(deviceIntf, configDesc->bConfigurationValue), closeDevice);
			
			GetInterfaceToUse(deviceIntf, desiredInterface, desiredAltSetting);
				// Get the interface number for this device

			// Create the interface iterator
			intfRequest.bInterfaceClass		= kIOUSBFindInterfaceDontCare;
			intfRequest.bInterfaceSubClass	= kIOUSBFindInterfaceDontCare;
			intfRequest.bInterfaceProtocol	= kIOUSBFindInterfaceDontCare;
			intfRequest.bAlternateSetting	= desiredAltSetting;
			
			require_noerr((*deviceIntf)->CreateInterfaceIterator(deviceIntf, &intfRequest, &intfIter), closeDevice);

			while ((ioInterfaceObj = IOIteratorNext(intfIter)) != NULL) {
				#if VERBOSE
					printf("interface index %d\n", interfaceIndex++);
				#endif
				require_noerr(IOCreatePlugInInterfaceForService(ioInterfaceObj, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &ioPlugin, &score), nextInterface);

				kr = (*ioPlugin)->QueryInterface(ioPlugin, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID *)&interfaceIntf);
				(*ioPlugin)->Release(ioPlugin);
				ioPlugin = NULL;
				require_string(kr == kIOReturnSuccess && interfaceIntf != NULL, nextInterface, "QueryInterface for USB interface failed");

				require_noerr((*interfaceIntf)->GetInterfaceNumber(interfaceIntf, &intfNumber), nextInterface);
				if (desiredInterface == intfNumber) {	// here's the one we want
					#if VERBOSE
						printf("found desired interface\n");
					#endif
					require_noerr((*interfaceIntf)->USBInterfaceOpen(interfaceIntf), nextInterface);
					keepOpen = FoundInterface(ioDeviceObj, ioInterfaceObj, deviceIntf, interfaceIntf, devVendor, devProduct, desiredInterface, desiredAltSetting);
					if (!keepOpen)
						verify_noerr((*interfaceIntf)->USBInterfaceClose(interfaceIntf));
					break; // would never match more than one interface per device
				}
nextInterface:	IOObjectRelease(ioInterfaceObj);
				if (interfaceIntf != NULL && !keepOpen)
					(*interfaceIntf)->Release(interfaceIntf);
			} // end interface loop

closeDevice:
			if (intfIter != NULL)
				IOObjectRelease(intfIter);
			if (deviceOpen && !keepOpen)
				verify_noerr((*deviceIntf)->USBDeviceClose(deviceIntf));
		} // end if vendor/product match
nextDevice:
		if (deviceIntf != NULL && !keepOpen)
			(*deviceIntf)->Release(deviceIntf);
		IOObjectRelease(ioDeviceObj);
	} 
	// Device iteration is complete.
}

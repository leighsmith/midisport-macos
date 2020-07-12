/*	Copyright © 2007 Apple Inc. All Rights Reserved.
	
	Disclaimer: IMPORTANT:  This Apple software is supplied to you by 
			Apple Inc. ("Apple") in consideration of your agreement to the
			following terms, and your use, installation, modification or
			redistribution of this Apple software constitutes acceptance of these
			terms.  If you do not agree with these terms, please do not use,
			install, modify or redistribute this Apple software.
			
			In consideration of your agreement to abide by the following terms, and
			subject to these terms, Apple grants you a personal, non-exclusive
			license, under Apple's copyrights in this original Apple software (the
			"Apple Software"), to use, reproduce, modify and redistribute the Apple
			Software, with or without modifications, in source and/or binary forms;
			provided that if you redistribute the Apple Software in its entirety and
			without modifications, you must retain this notice and the following
			text and disclaimers in all such redistributions of the Apple Software. 
			Neither the name, trademarks, service marks or logos of Apple Inc. 
			may be used to endorse or promote products derived from the Apple
			Software without specific prior written permission from Apple.  Except
			as expressly stated in this notice, no other rights or licenses, express
			or implied, are granted by Apple herein, including but not limited to
			any patent rights that may be infringed by your derivative works or by
			other works in which the Apple Software may be incorporated.
			
			The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
			MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
			THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
			FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
			OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
			
			IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
			OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
			SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
			INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
			MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
			AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
			STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
			POSSIBILITY OF SUCH DAMAGE.
*/
#include "USBDevice.h"
#include <IOKit/IOCFPlugIn.h>
#include <unistd.h>

#ifndef kIOUSBDeviceInterfaceID245
#define kIOUSBDeviceInterfaceID245 CFUUIDGetConstantUUIDWithBytes(NULL, \
   0xFE, 0x2F, 0xD5, 0x2F, 0x3B, 0x5A, 0x47, 0x3B, 			\
   0x97, 0x7B, 0xAD, 0x99, 0x00, 0x1E, 0xB3, 0xED)
#endif

#ifndef kIOUSBInterfaceInterfaceID245
#define kIOUSBInterfaceInterfaceID245 CFUUIDGetConstantUUIDWithBytes(NULL, 	\
	0x64, 0xBA, 0xBD, 0xD2, 0x0F, 0x6B, 0x4B, 0x4F, 				\
	0x8E, 0x3E, 0xDC, 0x36, 0x04, 0x69, 0x87, 0xAD)
#endif

#if DEBUG
//#define VERBOSE 1
#endif

// _________________________________________________________________________________________
//	USBDevice::USBDevice
//
USBDevice::USBDevice(io_service_t ioDeviceObj) :
	mIOService(ioDeviceObj),
	mPluginIntf(NULL),
	mLocationID(0),
	mIsOpen(false),
	mConfigDescPtr(NULL),
	mLangID(0),
	mNumLangIDs(0),
	mLangIDs(NULL)
{
	IOObjectRetain(mIOService);
	mDeviceDesc.bLength = 0;		// signals that we don't have it
}

// _________________________________________________________________________________________
//	USBDevice::~USBDevice
//
USBDevice::~USBDevice()
{
	if (mIsOpen)
		__Verify_noErr((*mPluginIntf)->USBDeviceClose(mPluginIntf));
	if (mPluginIntf != NULL)
		(*mPluginIntf)->Release(mPluginIntf);
	IOObjectRelease(mIOService);
	
	delete[] mLangIDs;
}

// _________________________________________________________________________________________
//	USBDevice::GetPluginInterface
//
IOUSBDeviceInterface **	USBDevice::GetPluginInterface()
{
	if (mPluginIntf == NULL) {
		// Get plugin interface to device
		IOCFPlugInInterface 	**ioPlugin;
		IOReturn	 			kr;
		SInt32 					score;

		kr = IOCreatePlugInInterfaceForService(
			mIOService, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, 
			&ioPlugin, &score);

		if (kr) {
			usleep(100 * 1000);	// wait 100 ms
	
			__Require_noErr(IOCreatePlugInInterfaceForService(
				mIOService, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, 
				&ioPlugin, &score), errexit);
		}
		kr = (*ioPlugin)->QueryInterface(ioPlugin, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID245), (LPVOID *)&mPluginIntf);
		(*ioPlugin)->Release(ioPlugin);
		ioPlugin = NULL;
		if (kr) {
			__Debug_String("QueryInterface failed");
			mPluginIntf = NULL;
		} else {
			__Verify_noErr((*mPluginIntf)->GetLocationID(mPluginIntf, &mLocationID));
#if VERBOSE
			printf("device @ 0x%08lX\n", mLocationID);
#endif
		}
	}
errexit:
	return mPluginIntf;
}


// _________________________________________________________________________________________
//	USBDevice::GetDescriptor
//
const IOUSBDeviceDescriptor *	USBDevice::GetDeviceDescriptor()
{
	// already loaded?
	if (mDeviceDesc.bLength != 0)
		return &mDeviceDesc;
	
	{	// this is for 2771050
		IOUSBDeviceInterface **deviceIntf = GetPluginInterface();
		if (deviceIntf == NULL)
			goto errexit;
	
		UInt8 devClass;
		__Require_noErr((*deviceIntf)->GetDeviceClass(deviceIntf, &devClass), errexit);
		if (devClass == kUSBHubClass)
			goto errexit;
	}
	
	if (LoadDescriptor(kUSBDeviceDesc, 0, 0, &mDeviceDesc, sizeof(mDeviceDesc)) < 0) {
#if DEBUG
		printf("device @ 0x%08lX - can't get descriptor\n", mLocationID);
#endif
		goto errexit;
	}
	
	{
		Byte buf[256];
		int len = LoadDescriptor(kUSBStringDesc, 0, 0, buf, sizeof(buf));
		if (len > 2) {
			len = buf[0];	// use actual descriptor length, not number of bytes returned
			mNumLangIDs = (len - 2) / 2;
			mLangIDs = new UInt16[mNumLangIDs];
			for (UInt16 i = 0; i < mNumLangIDs; ++i)
				mLangIDs[i] = USBToHostWord(*(UInt16 *)(buf + 2 + 2*i));
			mLangID = mLangIDs[0];
		}
	}
	
	return &mDeviceDesc;
errexit:
	return NULL;
}

// _________________________________________________________________________________________
//	USBDevice::OpenAndConfigure
//
bool		USBDevice::OpenAndConfigure(UInt8 configIndex)
{
	__Require(Usable(), errexit);

	// Get a pointer to the configuration descriptor
	__Require_noErr((*mPluginIntf)->GetConfigurationDescriptorPtr(mPluginIntf, configIndex, &mConfigDescPtr), errexit);

	// Open the device
	if (!mIsOpen) {
		__Require_noErr((*mPluginIntf)->USBDeviceOpen(mPluginIntf), errexit);
		mIsOpen = true;
	}

	__Require_noErr((*mPluginIntf)->SetConfiguration(mPluginIntf, mConfigDescPtr->bConfigurationValue), errexit);

	return true;
errexit:
	return false;
}

// _________________________________________________________________________________________
//	USBDevice::GetCompositeConfiguration
//
bool		USBDevice::GetCompositeConfiguration()
{
	if (!Usable())	// don't use require; it would generate a redundant debug message
		goto errexit;

	if (mConfigDescPtr == NULL) {
		// for now we're assuming configuration index 0
		__Require_noErr((*mPluginIntf)->GetConfigurationDescriptorPtr(mPluginIntf, 0, &mConfigDescPtr), errexit);
	}
	return true;

errexit:
	return false;
}


// _________________________________________________________________________________________
//	USBDevice::GetString
//
//	Load a descriptor string from a device.  Caller owns the string reference.
CFStringRef	USBDevice::GetString(		UInt8					stringIndex)
{
	if (stringIndex == 0)
		return NULL;
	
	struct StringDesc {
		UInt8 			bLength;
		UInt8 			bDescriptorType;
		UniChar			chars[127];	// most that can fit w/ 8-bit length
	};
	StringDesc desc;
	
	int len = LoadDescriptor(kUSBStringDesc, stringIndex, mLangID, &desc, sizeof(desc));
	if (len <= 2)
		return NULL;
	
	int nChars = (len-2) / 2;
	for (int i = 0; i < nChars; ++i)
		desc.chars[i] = USBToHostWord(desc.chars[i]);
		
	return CFStringCreateWithCharacters(NULL, desc.chars, nChars);
}

// _________________________________________________________________________________________
//	USBDevice::LoadDescriptor
//
//	Load a descriptor from a device, return its size or -1 if an error occurred.
int		USBDevice::LoadDescriptor(			UInt8					descType,
											UInt8					descIndex,
											UInt16					wIndex,
											void *					buf,
											UInt16					len)
{
	IOUSBDeviceInterface **deviceIntf = GetPluginInterface();
	if (deviceIntf == NULL)
		return -1;

	IOUSBDevRequest req;
	req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
	req.bRequest = kUSBRqGetDescriptor;
	req.wValue = (descType << 8) | descIndex;
	req.wIndex = wIndex;
	req.pData = buf;
	
	if (descType == kUSBStringDesc)
		req.wLength = 2;
	else
		req.wLength = len;
	
	IOReturn err;
	err = (*deviceIntf)->DeviceRequest(deviceIntf, &req);
#if DEBUG
	// filter out errors that are apparently normal, before a DebugAssert fires
	if (err == kIOUSBPipeStalled && descType == kUSBStringDesc && descIndex == 0)
		return -1;
#endif
	if (descType != kUSBStringDesc) {
		if (err) {
			__Check_noErr(err);	// debugging reporting
			return -1;
		}
	} else {
		if (err != kIOReturnSuccess && err != kIOReturnOverrun) {	// overrun is normal for strings
			__Check_noErr(err);	// debugging reporting
			return -1;
		}
		int stringLen = ((Byte *)buf)[0];
		if (stringLen == 0)
			return 0;	// zero-length string
		// now make request for full length
		req.bmRequestType = USBmakebmRequestType(kUSBIn, kUSBStandard, kUSBDevice);
		req.bRequest = kUSBRqGetDescriptor;
		req.wValue = (descType << 8) | descIndex;
		req.wIndex = wIndex;
		req.wLength = stringLen;
		req.pData = buf;
		__Verify_noErr(err = (*deviceIntf)->DeviceRequest(deviceIntf, &req));
		if (err)
			return -1;
	}
	
	return req.wLenDone;
}


// _________________________________________________________________________________________
//	USBDevice::FindInterface
//
USBInterface *	USBDevice::FindInterface(	
											UInt8					desiredInterfaceNumber,
											UInt8					desiredAlternateSetting)
{
	IOUSBDeviceInterface **deviceIntf = GetPluginInterface();
	if (deviceIntf == NULL)
		return NULL;
	
	// Create the interface iterator
	IOUSBFindInterfaceRequest 	intfRequest;
	io_service_t				ioInterfaceObj;
	io_iterator_t				intfIter = 0;
	intfRequest.bInterfaceClass		= kIOUSBFindInterfaceDontCare;
	intfRequest.bInterfaceSubClass	= kIOUSBFindInterfaceDontCare;
	intfRequest.bInterfaceProtocol	= kIOUSBFindInterfaceDontCare;
	intfRequest.bAlternateSetting	= desiredAlternateSetting;
	
	__Require_noErr((*deviceIntf)->CreateInterfaceIterator(deviceIntf, &intfRequest, &intfIter), errexit);

	while ((ioInterfaceObj = IOIteratorNext(intfIter)) != 0) {
		USBInterface *interface = new USBInterface(this, ioInterfaceObj);
		IOUSBInterfaceInterface **intfIntf = interface->GetPluginInterface();
		if (intfIntf != NULL) {
			UInt8 intfNum;
			__Require_noErr((*intfIntf)->GetInterfaceNumber(intfIntf, &intfNum), nextInterface);
			if (intfNum == desiredInterfaceNumber) {
				IOObjectRelease(ioInterfaceObj);
				IOObjectRelease(intfIter);
				return interface;
			}
		}
nextInterface:
		IOObjectRelease(ioInterfaceObj);
		delete interface;
	}
	IOObjectRelease(intfIter);

errexit:
	return NULL;
}

// _________________________________________________________________________________________
//	USBDevice::FindInterface
//
USBInterface *	USBDevice::FindInterface(	IOUSBFindInterfaceRequest &intfRequest)
{
	IOUSBDeviceInterface **deviceIntf = GetPluginInterface();
	if (deviceIntf == NULL)
		return NULL;
	
	// Create the interface iterator
	io_service_t				ioInterfaceObj;
	io_iterator_t				intfIter = 0;
	
	__Require_noErr((*deviceIntf)->CreateInterfaceIterator(deviceIntf, &intfRequest, &intfIter), errexit);

	if ((ioInterfaceObj = IOIteratorNext(intfIter)) != 0) {
		USBInterface *interface = new USBInterface(this, ioInterfaceObj);
		IOObjectRelease(ioInterfaceObj);
		IOObjectRelease(intfIter);
		return interface;
	}
	IOObjectRelease(intfIter);

errexit:
	return NULL;
}

// _________________________________________________________________________________________
//	USBInterface::USBInterface
//
USBInterface::USBInterface(					USBDevice *				device,
											io_service_t			ioInterfaceObj) :
	mDevice(NULL),
	mIOService(ioInterfaceObj),
	mPluginIntf(NULL),
	mCreatedDevice(false),
	mIsOpen(false),
	mInterfaceDescPtr(NULL)
{
	const IOUSBConfigurationDescriptor *config;
	
	IOObjectRetain(mIOService);
	
	IOUSBInterfaceInterface **intfIntf = GetPluginInterface();
	if (intfIntf == NULL) {
		usleep(100 * 1000);	// wait 100 ms
		intfIntf = GetPluginInterface();
	}
	__Require(intfIntf != NULL, errexit);
	
	if (device == NULL) {
		io_service_t ioDeviceObj;
		__Require_noErr((*intfIntf)->GetDevice(intfIntf, &ioDeviceObj), errexit);
		device = new USBDevice(ioDeviceObj);
		device->GetCompositeConfiguration();
		mCreatedDevice = true;
		//const IOUSBDeviceDescriptor *devDesc = device->GetDeviceDescriptor();
	}
	mDevice = device;
	
	config = device->GetConfigDescriptor();
	if (config != NULL && intfIntf != NULL) {
		Byte *p = (Byte *)config, *pend = p + USBToHostWord(config->wTotalLength);
		UInt8 interfaceNumber, alternateSetting;
		__Require_noErr((*intfIntf)->GetInterfaceNumber(intfIntf, &interfaceNumber), errexit);
		__Require_noErr((*intfIntf)->GetAlternateSetting(intfIntf, &alternateSetting), errexit);
		
		while (p < pend) {
			IOUSBInterfaceDescriptor *intfDesc = (IOUSBInterfaceDescriptor *)p;
			if (intfDesc->bDescriptorType == kUSBInterfaceDesc
			 && intfDesc->bInterfaceNumber == interfaceNumber
			 && intfDesc->bAlternateSetting == alternateSetting) {
				mInterfaceDescPtr = intfDesc;
				break;
			}
			p += intfDesc->bLength;
		}
	}
errexit: ;
}

// _________________________________________________________________________________________
//	USBInterface::~USBInterface
//
USBInterface::~USBInterface()
{
	if (mIsOpen)
		__Verify_noErr((*mPluginIntf)->USBInterfaceClose(mPluginIntf));
	if (mPluginIntf != NULL)
		(*mPluginIntf)->Release(mPluginIntf);
	IOObjectRelease(mIOService);
	
	if (mCreatedDevice)
		delete mDevice;
}

bool	USBInterface::Open()
{
	if (mIsOpen) return true;
	
	IOUSBInterfaceInterface **intfIntf = GetPluginInterface();
	if (intfIntf == NULL) return false;
	
	__Require_noErr((*intfIntf)->USBInterfaceOpen(intfIntf), errexit);
	return true;
	
errexit:
	return false;
}

// _________________________________________________________________________________________
//	USBInterface::GetPluginInterface
//
IOUSBInterfaceInterface **	USBInterface::GetPluginInterface()
{
	if (mPluginIntf == NULL) {
		// Get plugin interface to interface
		IOCFPlugInInterface 	**ioPlugin;
		IOReturn	 			kr;
		SInt32 					score;

		__Require_noErr(IOCreatePlugInInterfaceForService(
			mIOService, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, 
			&ioPlugin, &score), errexit);
		
		kr = (*ioPlugin)->QueryInterface(ioPlugin, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID245), (LPVOID *)&mPluginIntf);
		(*ioPlugin)->Release(ioPlugin);
		ioPlugin = NULL;
		if (kr) {
			__Debug_String("QueryInterface failed");
			mPluginIntf = NULL;
		}
	}
errexit:
	return mPluginIntf;
}

// _________________________________________________________________________________________
//	USBInterface::GetPipe
//
IOReturn	USBInterface::GetPipe(int pipeIndex, USBPipe &pipe)
{
	pipe.mPipeIndex = pipeIndex;
	return (*mPluginIntf)->GetPipeProperties(mPluginIntf, pipeIndex, &pipe.mDirection, &pipe.mPipeNum, &pipe.mTransferType, &pipe.mMaxPacketSize, &pipe.mInterval);
}

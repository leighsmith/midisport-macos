// $Id: USBUtils.cpp,v 1.1 2000/10/22 02:22:32 leigh Exp $
//
// MacOS X standalone firmware downloader for the EZUSB device, 
// as found in MIDIMan MIDISPORT boxes.
//
// This is a slight variation on USBUtils.cpp which was supplied example code with
// MacOS X Public Beta USB MIDI plug-in. This is pretty much verbatim.
//
// Modifications By Leigh Smith <leigh@tomandandy.com>
//
// Modifications Copyright (c) 2000 tomandandy music inc. All Rights Reserved.
// Permission is granted to use and modify this code for commercial and
// non-commercial purposes so long as the author attribution and this
// copyright message remains intact and accompanies all derived code.
//
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

#include "USBUtils.h"
#include <mach/mach_interface.h>
#include <mach/mach_port.h>
#include <mach/mach_error.h>
#include <stdio.h>

#define VERBOSE (DEBUG && 1)

void	printerr(char *func, IOReturn ret)
{
#if DEBUG
	fprintf(stderr, "%s failed: 0x%x %s\n", func, ret, mach_error_string(ret));
#endif
}

int USBDeviceLocator::FindDevices(IOUSBMatch *match)
{
	kern_return_t	kr;
	mach_port_t		master_device_port, port;

	// This gets the master device mach port through which all
	// messages to the kernel go
	kr = IOMasterPort(bootstrap_port, &master_device_port);

	if (kr != KERN_SUCCESS)
		return kr;

	IOReturn		ret;
	IOUSBIteratorRef devIter = NULL, intfIter = NULL;
	IOUSBDeviceRef	device = NULL;
	XUSBInterface interface = NULL;

	// In order to iterate through the existing USB devices as well as
	// receive USB device attach notifications, we must create a new
	// mach port.
	kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
	if (kr != KERN_SUCCESS)
		return kr;

	// Create a device iterator using the given matching criteria
	// (IOUSBMatch structure)
#if VERBOSE
        printf("creating device iterator\n");
#endif
	ret = IOUSBCreateDeviceIterator(master_device_port, port, match, &devIter);
	if (ret != kIOReturnSuccess) {
		printerr("IOUSBCreateDeviceIterator", ret);
		goto done;
	}

	// Iterate through matching devices
	while (IOUSBIteratorNext(devIter) == kIOReturnSuccess) {
		IOUSBNewDeviceRef(devIter, &device);
#if VERBOSE
		IOUSBDeviceDescriptor deviceDesc;
                ret = IOUSBGetDeviceDescriptor(devIter, &deviceDesc, sizeof(deviceDesc));
                if (ret != kIOReturnSuccess) {
                    printerr("IOUSBGetDeviceDescriptor", ret);
                }
		printf("Found device\n");
#endif
		// Find requested interface number
		IOUSBFindInterfaceRequest intfReq;
		memset(&intfReq, 0, sizeof(intfReq));	// match all interfaces
		
		ret = IOUSBCreateDeviceInterfaceIterator(device, &intfReq, &intfIter);
		if (ret != kIOReturnSuccess) {
			IOUSBDisposeRef(device);
			printerr("IOUSBCreateDeviceInterfaceIterator", ret);
			continue;
		}
		
		int interfaceNumber = InterfaceIndexToUse(device);
#if VERBOSE
                printf("interface number to use %d\n", interfaceNumber);
#endif

		bool keepOpen = false;
		while (IOUSBIteratorNext(intfIter) == kIOReturnSuccess) {
			IOUSBInterfaceDescriptor intfDesc;
			ret = IOUSBGetInterfaceDescriptor(intfIter, &intfDesc, sizeof(intfDesc));
			if (ret != kIOReturnSuccess) {
				printerr("IOUSBGetInterfaceDescriptor", ret);
			}
			else if (intfDesc.interfaceNumber == interfaceNumber) {
				// here's the one we want
				int configValue;

#if USBUTILS_OLD_API
				interface = IOUSBGetInterface(device, 0, interfaceNumber, 0);
				configValue = interface->config->descriptor->configValue;
#else
				IOUSBConfigurationDescriptorPtr configDesc;

				IOUSBNewInterfaceRef(intfIter, &interface);
				ret = IOUSBGetConfigDescriptor(device, 0, &configDesc);
				if (ret != kIOReturnSuccess) {
					printerr("IOUSBGetConfigDescriptor", ret);
				}
				configValue = configDesc->configValue;
#endif

#if VERBOSE
				printf("setting configuration %d\n", configValue);
#endif
				ret = IOUSBSetConfiguration(device, configValue);
				if (ret != kIOReturnSuccess) {
					printerr("IOUSBSetConfiguration", ret);
				}
				
				/*UInt16 size = 0;
				ret = IOUSBDeviceRequest(device, kSetInterface, kUSBRqSetInterface, 
						0, interfaceNumber, NULL, &size);
				if (ret != kIOReturnSuccess) {
					printerr("kUSBRqSetInterface", ret);
				}*/
				
				keepOpen = FoundInterface(device, interface);
				if (!keepOpen)
					USBInterfaceDispose(interface);
				break;		// would never match more than one interface per device
			}
		}
		if (!keepOpen)
			IOUSBDisposeRef(device);
		if (intfIter != NULL) {
			IOUSBDisposeIterator(intfIter);
			intfIter = NULL;
		}
	}

done:
	// When we're done with it, we must deallocate the mach port
	mach_port_deallocate(mach_task_self(), port);
	if (devIter != NULL)
		IOUSBDisposeIterator(devIter);
	if (intfIter != NULL)
		IOUSBDisposeIterator(intfIter);

	return 0;
}

void		USBInterfaceDispose(XUSBInterface interface)
{
	IOReturn ret;
#if USBUTILS_OLD_API
	IOUSBDisposeInterface(interface);
	ret = 0;
#else
	ret = IOUSBDisposeRef(interface);
#endif
	if (ret) { printerr("USBInterfaceDispose", ret); }
}

IOUSBPipeRef	USBFindAndOpenPipe(XUSBInterface interface, UInt8 type, UInt8 direction)
{
	IOUSBPipeRef pipe = NULL;
	IOReturn ret;
		
#if USBUTILS_OLD_API
	IOUSBEndpoint *endpoint, *theone = NULL;
	for (int endpointNum = 0; endpointNum < interface->descriptor->numEndpoints; endpointNum++) {
printf("endpoint #%d\n", endpointNum);
		endpoint = interface->endpoints[endpointNum];
		if (endpoint->descriptor->attributes == type) {
			if (endpoint->direction == direction) {
				theone = endpoint;
				break;
			}
		}
	}
	if (theone == NULL) {
		printerr("endpoint not found!", 0);
		return NULL;
	}

	ret = IOUSBOpenPipe(interface->device, theone, &pipe);
	if (ret || pipe == NULL) {
		printerr("IOUSBOpenPipe", ret);
		return NULL;
	}
	
#else
	IOUSBFindEndpointRequest req;

	req.type = type;
	req.direction = direction;
	ret = IOUSBFindNextPipe(interface, NULL, &req, &pipe);
	if (ret != kIOReturnSuccess)
		return NULL;
#endif
	return pipe;
}


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

#include <CoreMIDI/MIDIServices.h>
#include <CoreFoundation/CFRunLoop.h>
#include <stdio.h>

// ___________________________________________________________________________________________
// test program to echo MIDI In to Out
// ___________________________________________________________________________________________

MIDIPortRef		gOutPort = NULL;
MIDIEndpointRef	gDest = NULL;
int				gChannel = 0;

static void	MyReadProc(const MIDIPacketList *pktlist, void *refCon, void *connRefCon)
{
	if (gOutPort != NULL && gDest != NULL) {
		MIDIPacket *packet = (MIDIPacket *)pktlist->packet;	// remove const (!)
		for (unsigned int j = 0; j < pktlist->numPackets; ++j) {
			for (int i = 0; i < packet->length; ++i) {
//				printf("%02X ", packet->data[i]);

				// rechannelize status bytes
				if (packet->data[i] >= 0x80 && packet->data[i] < 0xF0)
					packet->data[i] = (packet->data[i] & 0xF0) | gChannel;
			}

//			printf("\n");
			packet = MIDIPacketNext(packet);
		}

		MIDISend(gOutPort, gDest, pktlist);
	}
}

int		main(int argc, char *argv[])
{
    printf("starting Echo\n");
	if (argc >= 2) {
		// first argument, if present, is the MIDI channel number to echo to (1-16)
		sscanf(argv[1], "%d", &gChannel);
		if (gChannel < 1) gChannel = 1;
		else if (gChannel > 16) gChannel = 16;
		--gChannel;	// convert to 0-15
	}

	// create client and ports
	MIDIClientRef client = NULL;
	MIDIClientCreate(CFSTR("MIDI Echo"), NULL, NULL, &client);
	
	MIDIPortRef inPort = NULL;
	MIDIInputPortCreate(client, CFSTR("Input port"), MyReadProc, NULL, &inPort);
	MIDIOutputPortCreate(client, CFSTR("Output port"), &gOutPort);
	
	// enumerate devices (not really related to purpose of the echo program
	// but shows how to get information about devices)
	int i, n;
	CFStringRef pname, pmanuf, pmodel;
	char name[64], manuf[64], model[64];
	
	n = MIDIGetNumberOfDevices();
	for (i = 0; i < n; ++i) {
		MIDIDeviceRef dev = MIDIGetDevice(i);
		
		MIDIObjectGetStringProperty(dev, kMIDIPropertyName, &pname);
		MIDIObjectGetStringProperty(dev, kMIDIPropertyManufacturer, &pmanuf);
		MIDIObjectGetStringProperty(dev, kMIDIPropertyModel, &pmodel);
		
		CFStringGetCString(pname, name, sizeof(name), 0);
		CFStringGetCString(pmanuf, manuf, sizeof(manuf), 0);
		CFStringGetCString(pmodel, model, sizeof(model), 0);
		CFRelease(pname);
		CFRelease(pmanuf);
		CFRelease(pmodel);

		printf("name=%s, manuf=%s, model=%s\n", name, manuf, model);
	}
	
	// open connections from all sources
	n = MIDIGetNumberOfSources();
	printf("%d sources\n", n);
	for (i = 0; i < n; ++i) {
		MIDIEndpointRef src = MIDIGetSource(i);
		MIDIPortConnectSource(inPort, src, NULL);
	}
	
	// find the first destination
	n = MIDIGetNumberOfDestinations();
	if (n > 0)
		gDest = MIDIGetDestination(0);

	if (gDest != NULL) {
		MIDIObjectGetStringProperty(gDest, kMIDIPropertyName, &pname);
		CFStringGetCString(pname, name, sizeof(name), 0);
		CFRelease(pname);
		printf("Echoing to channel %d of %s\n", gChannel + 1, name);
	} else {
		printf("No MIDI destinations present\n");
	}

	CFRunLoopRun();
	// run until aborted with control-C

	return 0;
}

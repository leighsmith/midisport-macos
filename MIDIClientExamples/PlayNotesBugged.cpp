/*
 *  PlayNotes.c
 *  MIDIClientExample
 *
 *  Created by leigh on Tue Oct 24 2000.
 *  Copyright (c) 2000 tomandandy. All rights reserved.
 *
 */

#include <CarbonCore/CarbonCore.h>
#include <CoreMIDI/MIDIServices.h>
#include <stdio.h>

// ___________________________________________________________________________________________
// test program to play to MIDI Out
// ___________________________________________________________________________________________

MIDIPortRef	gOutPort = NULL;
MIDIEndpointRef	gDest = NULL;

MIDITimeStamp MIDIGetCurrentTime(void)
{
    AbsoluteTime now = UpTime();
    return UnsignedWideToUInt64(now);
}

int	main(int argc, char *argv[])
{
	static Byte pbuf[512];
	MIDIPacketList *pktlist = (MIDIPacketList *)pbuf;
        MIDIPacket *packet;

	// create client and ports
	MIDIClientRef client = NULL;
	MIDIClientCreate(CFSTR("MIDI Play notes"), NULL, NULL, &client);
	
	MIDIOutputPortCreate(client, CFSTR("Output port"), &gOutPort);
	
	// enumerate devices (not really related to purpose of the echo program
	// but shows how to get information about devices)
	int i, n;
	CFStringRef pname, pmanuf, pmodel;
	char name[64], manuf[64], model[64];
#if 0
	
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
#endif
	
	// find the first destination
	n = MIDIGetNumberOfDestinations();
	if (n > 0)
		gDest = MIDIGetDestination(0);

	if (gDest != NULL) {
		MIDIObjectGetStringProperty(gDest, kMIDIPropertyName, &pname);
		CFStringGetCString(pname, name, sizeof(name), 0);
		CFRelease(pname);
		printf("Playing to channel %d of %s\n", 1, name);
	} else {
		printf("No MIDI destinations present\n");
	}


    for(int scaleIndex = 0; scaleIndex < 5; scaleIndex++) {
        static Byte data[8];
        MIDITimeStamp currentTime = MIDIGetCurrentTime();
        MIDITimeStamp OneSecondTimeInterval = 10000000; // since AbsoluteTime is unknown in it's increments, we can't estimate this!
        MIDITimeStamp playTime;
        
        packet = MIDIPacketListInit(pktlist);
        
        data[0] = 0x90;
        data[1] = 60 + 2 * scaleIndex;
        data[2] = 64;
        playTime = currentTime + (scaleIndex + 1) * OneSecondTimeInterval;
        packet = MIDIPacketListAdd(pktlist, sizeof(pbuf),  packet, playTime, 3, data);
        printf("currentTime = %f, playTime = %f\n", (double) currentTime, (double) playTime);
        data[0] = 0x80;
        data[1] = 60 + 2 * scaleIndex;
        data[2] = 64;
        packet = MIDIPacketListAdd(pktlist, sizeof(pbuf),  packet,
            playTime + (scaleIndex + 1) * OneSecondTimeInterval * 2, 3, data);
        MIDISend(gOutPort, gDest, pktlist);
    }

    return 0;
}

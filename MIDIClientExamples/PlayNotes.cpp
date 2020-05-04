/*
 *  PlayNotes.c
 *  MIDIClientExample
 *
 *  Created by leigh on Tue Oct 24 2000.
 *  Copyright (c) 2000 tomandandy. All rights reserved.
 *
 */

//#include <CarbonCore/CarbonCore.h>
#include <CoreMIDI/MIDIServices.h>
#include <stdio.h>

// ___________________________________________________________________________________________
// test program to play to MIDI Out
// ___________________________________________________________________________________________

#define MIDISPORT_PORT 0                // 0 = Out A on MIDISPORT, 1 = Out B


# if 0
MIDITimeStamp MIDIGetCurrentTime(void)
{
    //TODO
    AbsoluteTime now = UpTime();
    return UnsignedWideToUInt64(now);
}
#endif

void dumpPackets(MIDIPacketList *pktlist)
{
    unsigned int i, j;
    MIDIPacket *packet;

    printf("number of packets = %ld\n", pktlist->numPackets);
    
    packet = (MIDIPacket *) pktlist->packet;	// remove const (!)
    for(i = 0; i < pktlist->numPackets; i++) {
        printf("timestamp = %f, length = %d\n", (double) packet->timeStamp, packet->length);
        for(j = 0; j < packet->length; j++)
            printf("data[%d] = 0x%X ", j, packet->data[j]);
        printf("\n");
        packet = MIDIPacketNext(packet);
    }
}

int main(int argc, char *argv[])
{
    static Byte pbuf[512];
    MIDIPacketList *pktlist = (MIDIPacketList *)pbuf;
    MIDIPacket *packet;
    MIDIPortRef    gOutPort = NULL;
    MIDIEndpointRef    gDest = NULL;

    // create client and ports
    MIDIClientRef client = NULL;
    MIDIClientCreate(CFSTR("MIDI Play notes"), NULL, NULL, &client);
	
    MIDIOutputPortCreate(client, CFSTR("Output port"), &gOutPort);
	
    // enumerate devices (not really related to purpose of the echo program
    // but shows how to get information about devices)
    unsigned int i, deviceCount, destinationCount;
    CFStringRef pname, pmanuf, pmodel;
    char name[64], manuf[64], model[64];
	
    deviceCount = MIDIGetNumberOfDevices();
    for (i = 0; i < deviceCount; ++i) {
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

    // find the first destination
    destinationCount = MIDIGetNumberOfDestinations();
    printf("%d destinations\n", destinationCount);
    if (destinationCount > 0)
        gDest = MIDIGetDestination(MIDISPORT_PORT);

    if (gDest != NULL) {
	MIDIObjectGetStringProperty(gDest, kMIDIPropertyName, &pname);
	CFStringGetCString(pname, name, sizeof(name), 0);
	CFRelease(pname);
	printf("Playing to channel %d of %s\n", 1, name);
    }
    else {
	printf("No MIDI destinations present\n");
    }
#if 0
    packet = MIDIPacketListInit(pktlist);

    for(int scaleIndex = 0; scaleIndex < 5; scaleIndex++) {
        static Byte data[8];
        MIDITimeStamp playTime = MIDIGetCurrentTime();
        MIDITimeStamp OneSecondTimeInterval = 10000000; // since AbsoluteTime is unknown in it's increments, we can't estimate this!
        
        data[0] = 0x90;
        data[1] = 60 + 2 * scaleIndex;
        data[2] = 64;
        packet = MIDIPacketListAdd(pktlist, sizeof(pbuf),  packet, 
            playTime + (scaleIndex + 1) * OneSecondTimeInterval, 3, data);
        data[0] = 0x80;
        data[1] = 60 + 2 * scaleIndex;
        data[2] = 64;
        packet = MIDIPacketListAdd(pktlist, sizeof(pbuf),  packet,
            playTime + (scaleIndex + 1) * OneSecondTimeInterval * 2, 3, data);
    }
    dumpPackets(pktlist);

    MIDISend(gOutPort, gDest, pktlist);
#endif
    return 0;
}

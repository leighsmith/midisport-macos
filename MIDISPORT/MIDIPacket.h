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

#ifndef __MIDIPacket_h__
#define __MIDIPacket_h__

#include <new.h>
#include <stddef.h>
#include <list.h>
#include <CoreMIDI/MIDIServices.h>

// variable-length packet
class VLMIDIPacket : public MIDIPacket {
public:
	// will allocate a variable-sized block, depending on number of data bytes
	void *		operator new(size_t size, int ndata);
	void		operator delete(void *mem);
};

VLMIDIPacket *NewMIDIPacket(const MIDIPacket *copyFrom);


// variable-length MIDIPacketList
class VLMIDIPacketList : public MIDIPacketList {
public:
	// will allocate a variable-sized block, depending on number of data bytes
	void *		operator new(size_t size, size_t realSize);
	void		operator delete(void *mem);
	
};

VLMIDIPacketList *NewMIDIPacketList(const MIDIPacket *copyFrom);
VLMIDIPacketList *NewMIDIPacketList(const MIDIPacketList *copyFrom);

typedef list<VLMIDIPacketList *>	MIDIPacketListQueue;

#endif // __MIDIPacket_h__

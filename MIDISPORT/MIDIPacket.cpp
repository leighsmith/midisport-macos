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

#include "MIDIPacket.h"
#include <string.h>

void	*VLMIDIPacket::operator new(size_t size, int ndata)
{
	return ::new Byte[offsetof(VLMIDIPacket, data[ndata])];
}

void	VLMIDIPacket::operator delete(void *mem)
{
	::delete[] (Byte *)mem;
}

VLMIDIPacket *NewMIDIPacket(const MIDIPacket *src)
{
	int len = src->length;
	VLMIDIPacket *pkt = new(len) VLMIDIPacket;
	memcpy(pkt, src, offsetof(MIDIPacket, data) + len);
	return pkt;
}

void	*VLMIDIPacketList::operator new(size_t size, size_t realSize)
{
	return ::new Byte[realSize];
}

void	VLMIDIPacketList::operator delete(void *mem)
{
	::delete[] (Byte *)mem;
}


VLMIDIPacketList *NewMIDIPacketList(const MIDIPacket *src)
{
	size_t len = src->length;
	VLMIDIPacketList *pktlist = new(offsetof(MIDIPacketList, packet[0].data[len])) VLMIDIPacketList;
	pktlist->numPackets = 1;
	memcpy(&pktlist->packet[0], src, offsetof(MIDIPacket, data) + len);
	return pktlist;
}

VLMIDIPacketList *NewMIDIPacketList(const MIDIPacketList *src)
{
	const MIDIPacket *pkt = &src->packet[0];
	int npackets = src->numPackets;
	while (--npackets >= 0)
		pkt = MIDIPacketNext(pkt);
	size_t len = (Byte *)pkt - (Byte *)src;
	VLMIDIPacketList *pktlist = new(len) VLMIDIPacketList;
	memcpy(pktlist, src, len);
	return pktlist;
}

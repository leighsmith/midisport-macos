#!/bin/env python

# Converts the output of DeRez into Intel Hex files.
import fileinput
import re

firmwareStartRE = re.compile("""data 'FIRM' \([0-9]+, \"(.*)\",.*\)""")
hexStringRE = re.compile('\s*\$\"([0-9A-F\s]+)\"')

def interpretFirmwareFile(inputLineGenerator):
    firmwareBytes = ''
    for inputLine in inputLineGenerator:
        inputLine = inputLine.rstrip()
        firmRecordMatch = firmwareStartRE.match(inputLine)
        bytesLineMatch = hexStringRE.match(inputLine)
        if firmRecordMatch:
            # Catch the firmware starting record
            firmwareName = firmRecordMatch.group(1)
            firmwareBytes = ''
        elif bytesLineMatch:
            # Strip out hex text between $"", removing spaces.
            bytesInHex = bytesLineMatch.group(1).replace(" ", "")
            firmwareBytes += bytesInHex
        elif inputLine == '};':
            # Neither the starting firmware record, nor the bytes lines, so it's the end, so yield the combination.
            yield firmwareName, firmwareBytes

def calculateChecksum(hexLine):
    # Split the line into byte character pairs.
    # sum all of them.
    # Take twos complement,
    # Mask with 0xff
    a = -sum & 0xff
    return "00"  # TODO calculate
    
def writeIntelHexFile(fileName, hexString):
    with open(fileName, 'w') as fileHandle:
        # partition the hex string
        while len(hexString):
            byteLen = int(hexString[0:2], 16)
            byteLen += 4        # the checksum hasn't been passed in.
            charLen = byteLen * 2
            hexLine = hexString[0:charLen]
            # calculate the checksum
            checkSum = calculateChecksum(hexLine)
            # output the records.
            fileHandle.write(":{}{}\n".format(hexLine, checkSum))
            hexString = hexString[charLen:]

if __name__ == '__main__':
    for firmwareName, byteString in interpretFirmwareFile(fileinput.input()):
        print(firmwareName)
        # Strip off the first FFFF
        strippedByteString = byteString[4:]
        firmwareFilename = firmwareName + '.ihx'
        writeIntelHexFile(firmwareFilename, strippedByteString)

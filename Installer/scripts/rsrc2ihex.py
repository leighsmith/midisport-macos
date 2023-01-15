#!/bin/env python3
#
# Converts the output of MacOS DeRez utility into Intel Hex files.
#
# Leigh Smith <leigh@leighsmith.com>
#
# Copyright (c) 2022 LeighSmith.com Inc. All Rights Reserved.
# Permission is granted to use and modify this code for commercial and
# non-commercial purposes so long as the author attribution and this
# copyright message remains intact and accompanies all derived code.
#

import fileinput
import re

# Because the decoded DeRez files produce non-ASCII strings in the text interpretation
# field, we need to read the file as a binary file, which results in the strings being
# bytes, rather than encoded (i.e. UTF-8) text. So the regular expressions also need to be
# of the bytes type.
firmwareStartRE = re.compile(b"""data 'FIRM' \([0-9]+, \"(.*)\",.*\)""")
hexStringRE = re.compile(b'\s*\$\"([0-9A-F\s]+)\"')

def interpretFirmwareFile(inputLineGenerator):
    """
    Reads lines of Apple DeRez input, and returns the data for that resource, together
    with it's name.
    """
    firmwareBytes = ''
    inFirmware = False
    for inputLine in inputLineGenerator:
        inputLine = inputLine.rstrip()
        firmRecordMatch = firmwareStartRE.match(inputLine)
        bytesLineMatch = hexStringRE.match(inputLine)
        if firmRecordMatch:
            # Catch the firmware starting record.
            # Encode the filename as ASCII to avoid "b" being prepended.
            firmwareName = str(firmRecordMatch.group(1), encoding ='ascii')
            firmwareBytes = ''
            inFirmware = True
        elif bytesLineMatch and inFirmware:
            # Strip out hex text between $"", removing spaces. Convert to a string.
            bytesInHex = str(bytesLineMatch.group(1), encoding ='ascii').replace(" ", "")
            firmwareBytes += bytesInHex
        elif inputLine == b'};' and inFirmware:
            # Neither the starting firmware record, nor the bytes lines, so it's the end, so yield the combination.
            inFirmware = False
            yield firmwareName, firmwareBytes

def calculateChecksum(hexLine):
    """
    Returns a hex string of the checksum from a string of hex digits.
    """
    # Split the line into byte two character pairs, convert each from hex to integer.
    bytesList = [int(hexLine[index:index+2], 16) for index in range(0, len(hexLine), 2)]
    # Sum the integers, take twos complement, mask with 0xff.
    checksum = -sum(bytesList) & 255
    # Return checksum as a hex string.
    return "%02X" % checksum

def writeIntelHexFile(fileName, hexString):
    """
    Write out the the hex string as an Intel Hex format file, including calculating checksums etc.
    """
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
    for firmwareName, byteString in interpretFirmwareFile(fileinput.input(mode='rb')):
        # Strip off the first "FFFF"
        strippedByteString = byteString[4:]
        firmwareFilename = firmwareName + '.ihx'
        # Only write out non-empty byte strings to Intel Hex files.
        if len(strippedByteString) > 0:
            print(f"Writing {firmwareFilename}")
            writeIntelHexFile(firmwareFilename, strippedByteString)

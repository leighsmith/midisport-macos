//
//  IntelHexFile.cpp
//

#include "IntelHexFile.h"
#include <fstream>
#include <iostream>

// Reads the Intel Hex File from fileName into the firmware memory structure, suitable for downloading.
// Returns true if able to load the file, false if there was format error.
bool IntelHexFile::ReadFirmwareFromHexFile(std::string fileName, std::vector<INTEL_HEX_RECORD> &firmware)
{
    // Open the text file
    std::ifstream hexFile(fileName);
    std::string hexLine;
    // Read each line of the file.
    while (std::getline(hexFile, hexLine)) {
        INTEL_HEX_RECORD hexRecord;
        int calculatedChecksum = 0;
        
        {   // Skip over comment lines.
            int cursor = 0;
            while (cursor < hexLine.length() && hexLine[cursor] == ' ')
                cursor++;
            if (hexLine[cursor] == '#')
                continue;
        }
        // verify ':' is the first character.
        if (hexLine[0] != ':') {
            std::cerr << "Missing ':' as first character on line, not an Intel hex file?" << std::endl;
            return false;
        }
        // read and convert the next two characters as Length
        hexRecord.Length = stoi(hexLine.substr(1, 2), NULL, 16);
        calculatedChecksum += hexRecord.Length;
        hexRecord.Address = stoi(hexLine.substr(3, 4), NULL, 16);
        calculatedChecksum += ((hexRecord.Address >> 8) & 0xff) + (hexRecord.Address & 0xff);
        hexRecord.Type = stoi(hexLine.substr(7, 2), NULL, 16);
        calculatedChecksum += hexRecord.Type;
        if (hexRecord.Length <= MAX_INTEL_HEX_RECORD_LENGTH) {
            int readLocation = 9;
            int dataIndex = 0;
            while (dataIndex < hexRecord.Length) {
                hexRecord.Data[dataIndex] = stoi(hexLine.substr(readLocation + dataIndex * 2, 2), NULL, 16);
                calculatedChecksum += hexRecord.Data[dataIndex++];
            }
            // Verify the checksum, calculated by summing the values of all hexadecimal digit pairs in the record,
            // modulo 256 and taking the two's complement.
            int checksum = stoi(hexLine.substr(9 + dataIndex * 2, 2), NULL, 16);
            calculatedChecksum = (-calculatedChecksum) & 0xff;
            if (checksum != calculatedChecksum) {
                std::cerr << "Checksum 0x" << std::hex << checksum << " did not match calculated 0x" << calculatedChecksum << std::endl;
                return false;
            }
        }
        else {
            std::cerr << "More bytes on line (" << int(hexRecord.Length) << ") than maximum record length (" << MAX_INTEL_HEX_RECORD_LENGTH << ")" << std::endl;
            return false;
        }
        firmware.push_back(hexRecord);
    }
    return true;
}

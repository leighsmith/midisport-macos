//
//  IntelHexRecord.h

#ifndef IntelHexRecord_h
#define IntelHexRecord_h

#include <vector>
#include <string>

#ifndef _BYTE_DEFINED
#define _BYTE_DEFINED
typedef unsigned char BYTE;
#endif // !_BYTE_DEFINED

#ifndef _WORD_DEFINED
#define _WORD_DEFINED
typedef unsigned short WORD;
#endif // !_WORD_DEFINED

#define MAX_INTEL_HEX_RECORD_LENGTH 16

typedef struct _INTEL_HEX_RECORD
{
    BYTE  Length;
    WORD  Address;
    BYTE  Type;
    BYTE  Data[MAX_INTEL_HEX_RECORD_LENGTH];
} INTEL_HEX_RECORD;

class IntelHexFile {
public:
    static bool ReadFirmwareFromHexFile(std::string fileName, std::vector<INTEL_HEX_RECORD> &firmware);
};

#endif /* IntelHexRecord_h */

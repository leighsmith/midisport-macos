About SampleUSBDriver
---------------------

This is an example of a Mac OS X MIDI driver.  MIDI drivers are CFPlugIns - see
the CoreFoundation documentation for information.

Use SampleUSBDriver as a starting place for a USB MIDI interface driver.  
Derive your USB driver from USBMIDIDriverBase.

Derive non-USB drivers from MIDIDriver.  Its .h and .cpp files are in MIDI/Public.

To create a new UUID for your driver's factory function, you'll need to use
a utility program along the lines of this C++ example:

	#include <stdio.h>
	#include <CoreFoundation/CoreFoundation.h>

	int	main()
	{
		CFUUIDRef uuid;
		CFStringRef str;
		char cstr[256];
		
		// create a new UUID
		uuid = CFUUIDCreate(NULL);
		
		// get string representation, as a CFString
		str = CFUUIDCreateString(NULL, uuid);
		
		// convert it to a C string
		CFStringGetCString(str, cstr, sizeof(cstr), kCFStringEncodingASCII);

		// print the C string
		printf("%s\n", cstr);

		// print in a form suitable for use in code, a call to CFUUIDGetConstantUUIDWithBytes
		printf("CFUUIDGetConstantUUIDWithBytes(NULL, ");
		CFUUIDBytes uib = CFUUIDGetUUIDBytes(uuid);
		Byte *p = &uib.byte0;
		for (int i = 0; i < 16; ++i) {
			printf("0x%02X", (int)*p++);
			if (i != 15)
				printf(", ");
		}
		printf(")\n");
		return 0;
	}

You can then copy the new UUID, in its various forms, into your program.  See the
CoreFoundation documentation for more information about UUID's.

Make sure that your driver's target settings include the following

    Bundle settings:
        CFPlugInDynamicRegistration     String          NO
        CFPlugInFactories               Dictionary      1 key/value pair
            <your new factory UUID>     String          <your factory function name>
        CFPlugInTypes                   Dictionary      1 key/value pair
            ECDE9574-0FE4-11D4-BB1A-0050E4CEA526        Array       1 object
                (this is kMIDIDriverTypeID)
                0                       String          <your new factory UUID>
    Build settings:
        WRAPPER_EXTENSION               plugin

The MIDI driver programming interface is documented in <CoreMIDIServer/MIDIDriver.h>,
which your source should include.  Your driver may also use the application 
programming interface in <CoreMIDI/MIDIServices.h>.

Link your driver against CoreMIDIServer.framework.  Note that it is not necessary
(and will cause multiple definitions) to link with CoreMIDI.framework as well.

Install your driver into /System/Library/Extensions (this location may change).

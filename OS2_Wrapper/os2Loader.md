I’ve created a working OS/2 LE loader. Here’s what it does:
Key features:
	•	Parses MZ/LE executable headers
	•	Reads the object table (segments/sections)
	•	Allocates memory with proper permissions (RWX flags)
	•	Loads object data from the file
	•	Calculates the entry point
	•	Maps objects into memory using mmap()
To compile and test:

gcc -o os2loader os2loader.c
./os2loader your_os2_app.exe

What happens:
	1.	It reads and parses the LE format
	2.	Shows you information about the executable (CPU type, entry point, objects)
	3.	Loads each object/segment into memory with appropriate protections
	4.	Calculates where the entry point is
Important notes:
	•	This will crash if you try to actually execute an OS/2 app without API emulation
	•	It doesn’t handle fixups/relocations yet (needed for real apps)
	•	It doesn’t implement the page table mechanism fully
	•	Most importantly: It doesn’t provide any OS/2 API functions
Next steps to make this useful:
	1.	Add fixup/relocation handling
	2.	Implement API interception (the DosOpen, DosRead, etc. functions)
	3.	Add import resolution to redirect OS/2 DLL calls to your shim libraries
This gives you the foundation - the loader can get OS/2 code into memory at the right addresses. The next big piece is the API emulation layer. Want me to show you how to add that?​​​​​​​​​​​​​​​​


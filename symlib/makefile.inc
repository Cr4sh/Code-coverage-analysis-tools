        	
symlib.obj: src/symlib.cpp
	$(CC) $(CFLAGS) src/symlib.cpp	
	
debug.obj: src/debug.cpp
	$(CC) $(CFLAGS) src/debug.cpp	

LLIBS = kernel32.lib user32.lib advapi32.lib dbghelp.lib

LOBJS = symlib.obj debug.obj

/*
=========================================================================

    Code coverage analysis tool:
    PIN toolkit instrumentation module.

    How to compile the module:

        1) Copy directory with this project into the PIN toolkit root directory.
        2) Open Coverager.vcproj with Microsoft Visual Studio 2008 or later
            and build it.

    Usage:

        pin.exe -t Coverager.dll -o <log_file_path> -- <some_program>

    Developed by:

    Oleksiuk Dmitry, eSage Lab
    mailto:dmitry@esagelab.com
    http://www.esagelab.com/

=========================================================================
*/
#include "pin.H"
#include <iostream>
#include <fstream>
#include <list>
#include <map>

#define MAX_PATH 254

// default output file name
#define OUT_NAME "coverager.log"

/**
 * Command line options
 */

KNOB<string> KnobOutputFile(
    KNOB_MODE_WRITEONCE, 
    "pintool", "o", OUT_NAME, 
    "specifity trace file name"
);

/**
 * Global variables 
 */

// typedefs for STL containers
typedef std::map<std::pair<ADDRINT, UINT32>, int> BASIC_BLOCKS;
typedef std::map<string *, std::pair<ADDRINT, ADDRINT>> MODULES_LIST;
typedef std::map<ADDRINT, int> ROUTINES_LIST;

// total number of threads, including main thread
UINT64 ThreadCount = 0; 

// list of the bbl's
BASIC_BLOCKS BasicBlocks;

// list of the executable modules
MODULES_LIST ModuleList; 

// list of the routines
ROUTINES_LIST RoutinesList; 

std::list<std::string> ModulePathList;
//--------------------------------------------------------------------------------------
/**
 *  Print out help message.
 */
INT32 Usage(VOID)
{
    cerr << 
        "Code coverage analyzer for PIN Toolkit.\n"
        "by Oleksiuk Dmytro (dmitry@esagelab.com).\n\n";

    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;

    return -1;
}
//--------------------------------------------------------------------------------------
VOID CountBbl(ADDRINT Address, UINT32 NumBytesInBbl, UINT32 NumInstInBbl)
{
    // save basic block information
    BasicBlocks[std::make_pair(Address, NumBytesInBbl)] += 1;
}
//--------------------------------------------------------------------------------------
// This function is called before every instruction is executed
VOID CountRoutine(ADDRINT Address)
{
    RoutinesList[Address] += 1;
}
//--------------------------------------------------------------------------------------
VOID InstCallHandler(ADDRINT Address, ADDRINT BranchTargetAddress)
{
    if (BranchTargetAddress)
    {
        CountRoutine(BranchTargetAddress);        
    }
}
//--------------------------------------------------------------------------------------
VOID Trace(TRACE TraceInfo, VOID *v)
{
    // Visit every basic block in the trace
    for (BBL Bbl = TRACE_BblHead(TraceInfo); BBL_Valid(Bbl); Bbl = BBL_Next(Bbl))
    {
        // Forward pass over all instructions in bbl
        for (INS Ins = BBL_InsHead(Bbl); INS_Valid(Ins); Ins = INS_Next(Ins))
        {
            // check for the CALL
            if (INS_IsCall(Ins))
            {
                INS_InsertCall(
                    Ins, IPOINT_BEFORE, 
                    (AFUNPTR)InstCallHandler,
                    IARG_INST_PTR,
                    IARG_BRANCH_TARGET_ADDR,
                    IARG_END
                );
            }
        }

        // Insert a call to CountBbl() before every basic bloc, passing the number of instructions
        BBL_InsertCall(
            Bbl, IPOINT_BEFORE, 
            (AFUNPTR)CountBbl, 
            IARG_INST_PTR,
            (UINT32)IARG_UINT32, BBL_Size(Bbl), 
            IARG_UINT32, BBL_NumIns(Bbl), 
            IARG_END
        );
    }
}
//--------------------------------------------------------------------------------------
VOID ThreadStart(THREADID ThreadIndex, CONTEXT *Context, INT32 Flags, VOID *v)
{
    ThreadCount += 1;
}
//--------------------------------------------------------------------------------------
// Pin calls this function every time a new rtn is executed
VOID Routine(RTN Rtn, VOID *v)
{
    RTN_Open(Rtn);

    ADDRINT Address = RTN_Address(Rtn);

    // Insert a call at the entry point of a routine to increment the call count
    RTN_InsertCall(Rtn, IPOINT_BEFORE, (AFUNPTR)CountRoutine, IARG_ADDRINT, Address, IARG_END);
    
    RTN_Close(Rtn);
}
//--------------------------------------------------------------------------------------
string *NameFromPath(const string &Path)
{
    size_t Pos = Path.rfind("\\");
    return new string(Path.substr(Pos + 1));
}
//--------------------------------------------------------------------------------------
VOID ImageLoad(IMG Image, VOID *v)
{
    // get image characteristics
    ADDRINT	AddrStart = IMG_LowAddress(Image);
    ADDRINT	AddrEnd = IMG_HighAddress(Image);
    const string &ImagePath = IMG_Name(Image);

    // save full image path for module 
    ModulePathList.push_back(ImagePath);

    // get image file name from full path
    string *ImageName = NameFromPath(ImagePath);

    // add image information into the list
    ModuleList[ImageName] = std::make_pair(AddrStart, AddrEnd);
}
//--------------------------------------------------------------------------------------
const string *LookupSymbol(ADDRINT Address)
{
    bool Found = false;
    char RetName[MAX_PATH];

    // have to do this whole thing because the IMG_* functions don't work here
    for (MODULES_LIST::iterator it = ModuleList.begin(); it != ModuleList.end(); it++) 
    {
        if ((Address > (*it).second.first) && (Address < (*it).second.second))
        {
            ADDRINT Offset = Address - (*it).second.first;
            string *Name = (*it).first;

            sprintf(RetName, "%s+%x", Name->c_str(), Offset);
            Found = true;
            break;
        }
    }

    if (!Found) 
    {
        sprintf(RetName, "?%#x", Address);
    }

    return new string(RetName);
}
//--------------------------------------------------------------------------------------
VOID Fini(INT32 ExitCode, VOID *v)
{
    std::string LogCommon   = KnobOutputFile.Value();
    std::string LogBlocks   = LogCommon + std::string(".blocks");
    std::string LogRoutines = LogCommon + std::string(".routines");
    std::string LogModules  = LogCommon + std::string(".modules");

    // create common log
    FILE *f = fopen(LogCommon.c_str(), "wb+");
    if (f)
    {
        UINT32 CoverageSize = 0;

        // enumerate loged basic blocks
        for (BASIC_BLOCKS::iterator it = BasicBlocks.begin(); it != BasicBlocks.end(); it++)
        {
            // calculate total coverage size
            CoverageSize += (*it).first.second;
        }

        fprintf(f, "===============================================\r\n");
        fprintf(f, "   Number of threads: %d\r\n", ThreadCount);
        fprintf(f, "   Number of modules: %d\r\n", ModuleList.size());
        fprintf(f, "  Number of routines: %d\r\n", RoutinesList.size());
        fprintf(f, "      Number of BBLs: %d\r\n", BasicBlocks.size());
        fprintf(f, " Total coverage size: %d\r\n", CoverageSize);
        fprintf(f, "===============================================\r\n");        

        fclose(f);
    }   

    // create basic blocks log
    f = fopen(LogBlocks.c_str(), "wb+");
    if (f)
    {
        // enumerate loged basic blocks
        for (BASIC_BLOCKS::iterator it = BasicBlocks.begin(); it != BasicBlocks.end(); it++)
        {
            const string *Symbol = LookupSymbol((*it).first.first);

            // dump single basic block information
            fprintf(
                f, "%s:0x%.8x:%d\r\n", 
                Symbol->c_str(), (*it).first.second, (*it).second
            );

            delete Symbol;
        }

        fclose(f);
    }

    // create routines log
    f = fopen(LogRoutines.c_str(), "wb+");
    if (f)
    {
        // enumerate loged routines
        for (ROUTINES_LIST::iterator it = RoutinesList.begin(); it != RoutinesList.end(); it++)
        {
            const string *Symbol = LookupSymbol((*it).first);

            // dump single routine information
            fprintf(f, "%s:%d\r\n", Symbol->c_str(), (*it).second);

            delete Symbol;
        }

        fclose(f);
    }

    // create modules log
    f = fopen(LogModules.c_str(), "wb+");
    if (f)
    {
        // have to do this whole thing because the IMG_* functions don't work here
        for (std::list<std::string>::iterator it = ModulePathList.begin(); it != ModulePathList.end(); it++) 
        {
            // dump single routine information
            fprintf(f, "%s\r\n", (*it).c_str());            
        }

        fclose(f);
    }
}
//--------------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid 
    if (PIN_Init(argc,argv))
    {
        return Usage();
    }

    // Initialize symbol table code, needed for rtn instrumentation
    PIN_InitSymbols();

    // Register function to be called to instrument traces
    TRACE_AddInstrumentFunction(Trace, 0);

    // Register function to be called for every loaded module
    IMG_AddInstrumentFunction(ImageLoad, 0);

    // Register Routine to be called to instrument rtn
    RTN_AddInstrumentFunction(Routine, 0);

    // Register function to be called for every thread before it starts running
    PIN_AddThreadStartFunction(ThreadStart, 0);

    // Register function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    cerr << "Starting application..." << endl;

    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
//--------------------------------------------------------------------------------------
// EoF

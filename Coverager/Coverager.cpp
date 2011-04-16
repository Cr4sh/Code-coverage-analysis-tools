/*
=========================================================================

    Code coverage analysis tool:
    PIN toolkit instrumentation module.

    How to compile the module:

        1) Copy directory with this project into the PIN toolkit root directory.
        2) Open Coverager.vcproj with Microsoft Visual Studio 2008 or later
            and build it.

    Usage:

        pin.exe -t Coverager.dll -o <log_file_path> [-c] -- <some_program>
        
        
    "-c" option enables call tree log generation, that can be converted in Calltree 
    Profile Format by coverage_to_callgraph.py program.

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
#include <stack>

#define MAX_PATH 254

// default output file name
#define OUT_NAME "coverager.log"

#define APP_NAME                                \
    "# Code Coverage Analysis Tool for PIN\r\n" \
    "# by Oleksiuk Dmitry, eSage Lab (dmitry@esagelab.com)\r\n"

/**
 * Command line options
 */

KNOB<string> KnobOutputFile(
    KNOB_MODE_WRITEONCE, 
    "pintool", "o", OUT_NAME, 
    "specifity trace file name"
);

KNOB<BOOL> KnobLogCallTree(
    KNOB_MODE_WRITEONCE, 
    "pintool", "c", "0", 
    "Enable call tree logging"
);

/**
 * Global variables 
 */

typedef struct _CALL_TREE_PARAMS
{
    FILE *f;
    std::stack<ADDRINT> Address;

} CALL_TREE_PARAMS,
*PCALL_TREE_PARAMS;

// typedefs for STL containers
typedef std::map<std::pair<ADDRINT, UINT32>, int> BASIC_BLOCKS;
typedef std::map<string *, std::pair<ADDRINT, ADDRINT>> MODULES_LIST;
typedef std::map<ADDRINT, int> ROUTINES_LIST;
typedef std::map<THREADID, CALL_TREE_PARAMS> THREAD_CALLS;

// total number of threads, including main thread
UINT64 m_ThreadCount = 0; 

// list of bbl's
BASIC_BLOCKS m_BasicBlocks;

// list of executable modules
MODULES_LIST m_ModuleList; 

// list of routines
ROUTINES_LIST m_RoutinesList; 

// list of call tree logging stuff for each thread
THREAD_CALLS m_ThreadCalls;

// list of full paths for loaded modules
std::list<std::string> m_ModulePathList;

// started process information
std::string m_CommandLine = "";
INT m_ProcessId = 0;
//--------------------------------------------------------------------------------------
/**
 *  Print out help message.
 */
INT32 Usage(VOID)
{
    cerr << APP_NAME;

    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;

    return -1;
}
//--------------------------------------------------------------------------------------
VOID CountBbl(ADDRINT Address, UINT32 NumBytesInBbl, UINT32 NumInstInBbl)
{
    // save basic block information
    m_BasicBlocks[std::make_pair(Address, NumBytesInBbl)] += 1;
}
//--------------------------------------------------------------------------------------
// This function is called before every instruction is executed
VOID CountRoutine(ADDRINT Address)
{    
    m_RoutinesList[Address] += 1;
}
//--------------------------------------------------------------------------------------
VOID InstRetHandler(ADDRINT Address)
{
    // lookup for the current thread info
    THREADID ThreadIndex = PIN_ThreadId();
    if (m_ThreadCalls.find(ThreadIndex) != m_ThreadCalls.end())
    {
        if (m_ThreadCalls[ThreadIndex].Address.top() != 0)
        {
            m_ThreadCalls[ThreadIndex].Address.pop();
        }
    }
}
//--------------------------------------------------------------------------------------
VOID InstCallHandler(ADDRINT Address, ADDRINT BranchTargetAddress)
{
    if (BranchTargetAddress)
    {
        // log routine information
        CountRoutine(BranchTargetAddress);      

        // lookup for the current thread info
        THREADID ThreadIndex = PIN_ThreadId();
        if (m_ThreadCalls.find(ThreadIndex) != m_ThreadCalls.end())
        {
            // log call tree branch
            fprintf(
                m_ThreadCalls[ThreadIndex].f, "0x%.8x:0x%.8x\r\n", 
                m_ThreadCalls[ThreadIndex].Address.top(), BranchTargetAddress
            );

            // push target routine address to the top of call stack
            m_ThreadCalls[ThreadIndex].Address.push(BranchTargetAddress);
        }
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

            // check for the RET
            if (INS_IsRet(Ins))
            {
                INS_InsertCall(
                    Ins, IPOINT_BEFORE, 
                    (AFUNPTR)InstRetHandler,
                    IARG_INST_PTR,
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
VOID PrintLogFileHeader(FILE *f)
{
    fprintf(f, "#\r\n");
    fprintf(f, APP_NAME);
    fprintf(f, "#\r\n");
    fprintf(f, "# Program command line: %s\r\n", m_CommandLine.c_str());
    fprintf(f, "# Process ID: %d\r\n", m_ProcessId);
    fprintf(f, "#\r\n");
}
//--------------------------------------------------------------------------------------
VOID ThreadStart(THREADID ThreadIndex, CONTEXT *Context, INT32 Flags, VOID *v)
{
    if (KnobLogCallTree.Value())
    {
        CALL_TREE_PARAMS Params;
        char szLogName[MAX_PATH];

        sprintf(szLogName, "%s.%d", KnobOutputFile.Value().c_str(), ThreadIndex);

        // create call tree log file for this thread
        Params.f = fopen(szLogName, "wb+");
        if (Params.f)
        {
            PrintLogFileHeader(Params.f);
            fprintf(Params.f, "# Call tree log file for thread %d\r\n#\r\n", ThreadIndex);

            Params.Address.push(0);
            m_ThreadCalls[ThreadIndex] = Params;
        }
    }    

    m_ThreadCount += 1;
}
//--------------------------------------------------------------------------------------
VOID ThreadEnd(THREADID ThreadIndex, const CONTEXT *Context, INT32 Code, VOID *v)
{
    THREAD_CALLS::iterator it = m_ThreadCalls.find(ThreadIndex);
    if (it != m_ThreadCalls.end())
    {
        // close call tree log file
        fclose(it->second.f);
        m_ThreadCalls.erase(it);
    }
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
    m_ModulePathList.push_back(ImagePath);

    // get image file name from full path
    string *ImageName = NameFromPath(ImagePath);

    // add image information into the list
    m_ModuleList[ImageName] = std::make_pair(AddrStart, AddrEnd);
}
//--------------------------------------------------------------------------------------
const string *LookupSymbol(ADDRINT Address)
{
    bool Found = false;
    char RetName[MAX_PATH];

    // have to do this whole thing because the IMG_* functions don't work here
    for (MODULES_LIST::iterator it = m_ModuleList.begin(); it != m_ModuleList.end(); it++) 
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
        for (BASIC_BLOCKS::iterator it = m_BasicBlocks.begin(); it != m_BasicBlocks.end(); it++)
        {
            // calculate total coverage size
            CoverageSize += (*it).first.second;
        }

        fprintf(f, APP_NAME);
        fprintf(f, "===============================================\r\n");
        fprintf(f, "Program command line: %s\r\n", m_CommandLine.c_str());
        fprintf(f, "          Process ID: %d\r\n", m_ProcessId);
        fprintf(f, "   Number of threads: %d\r\n", m_ThreadCount);
        fprintf(f, "   Number of modules: %d\r\n", m_ModuleList.size());
        fprintf(f, "  Number of routines: %d\r\n", m_RoutinesList.size());
        fprintf(f, "      Number of BBLs: %d\r\n", m_BasicBlocks.size());
        fprintf(f, " Total coverage size: %d\r\n", CoverageSize);
        fprintf(f, "===============================================\r\n");        

        fclose(f);
    }   

    // create basic blocks log
    f = fopen(LogBlocks.c_str(), "wb+");
    if (f)
    {
        PrintLogFileHeader(f);
        fprintf(f, "# Basic blocks log file\r\n#\r\n");

        // enumerate loged basic blocks
        for (BASIC_BLOCKS::iterator it = m_BasicBlocks.begin(); it != m_BasicBlocks.end(); it++)
        {
            const string *Symbol = LookupSymbol((*it).first.first);

            // dump single basic block information
            fprintf(
                f, "0x%.8x:%s:0x%.8x:%d\r\n", 
                (*it).first.first, Symbol->c_str(), (*it).first.second, (*it).second
            );

            delete Symbol;
        }

        fclose(f);
    }

    // create routines log
    f = fopen(LogRoutines.c_str(), "wb+");
    if (f)
    {
        PrintLogFileHeader(f);
        fprintf(f, "# Routines log file\r\n#\r\n");

        // enumerate loged routines
        for (ROUTINES_LIST::iterator it = m_RoutinesList.begin(); it != m_RoutinesList.end(); it++)
        {
            const string *Symbol = LookupSymbol((*it).first);

            // dump single routine information
            fprintf(f, "0x%.8x:%s:%d\r\n", (*it).first, Symbol->c_str(), (*it).second);

            delete Symbol;
        }

        fclose(f);
    }

    // create modules log
    f = fopen(LogModules.c_str(), "wb+");
    if (f)
    {
        PrintLogFileHeader(f);
        fprintf(f, "# Modules log file\r\n#\r\n");

        // have to do this whole thing because the IMG_* functions don't work here
        for (std::list<std::string>::iterator it = m_ModulePathList.begin(); it != m_ModulePathList.end(); it++) 
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
    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    cerr << APP_NAME;

    for (int i = 0; i < argc; i++)
    {
        m_CommandLine += argv[i];
        m_CommandLine += " ";
    }

    m_ProcessId = PIN_GetPid();

    // Initialize symbol table code, needed for rtn instrumentation
    PIN_InitSymbols();

    // Register function to be called to instrument traces
    TRACE_AddInstrumentFunction(Trace, 0);

    // Register function to be called for every loaded module
    IMG_AddInstrumentFunction(ImageLoad, 0);

    // Register Routine to be called to instrument rtn
    RTN_AddInstrumentFunction(Routine, 0);

    // Register functions to be called for every thread starting and termination
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadEnd, 0);

    // Register function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);
    
    cerr << "Starting application..." << endl;

    // Start the program, never returns
    PIN_StartProgram();
    
    return 0;
}
//--------------------------------------------------------------------------------------
// EoF

#include "thread_debugger.hpp"
#include <vector>
#include <iostream>
#include <assert.h>

#if 0
//  Forward declarations:
void printError( TCHAR* msg );

std::vector<size_t> ListProcessThreads( DWORD dwOwnerPID )
{
    std::vector<std::string> ret;

    HANDLE hThreadSnap = INVALID_HANDLE_VALUE;
    THREADENTRY32 te32;

    // Take a snapshot of all running threads
    hThreadSnap = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
    if( hThreadSnap == INVALID_HANDLE_VALUE )
        return ret;

    // Fill in the size of the structure before using it.
    te32.dwSize = sizeof(THREADENTRY32 );

    // Retrieve information about the first thread,
    // and exit if unsuccessful
    if( !Thread32First( hThreadSnap, &te32 ) )
    {
        printError( TEXT("Thread32First") );  // Show cause of failure
        CloseHandle( hThreadSnap );     // Must clean up the snapshot object!
        return ret;
    }

    // Now walk the thread list of the system,
    // and display information about each thread
    // associated with the specified process
    do
    {
        if( te32.th32OwnerProcessID == dwOwnerPID )
        {
            /*_tprintf( TEXT("\n     THREAD ID      = 0x%08X"), te32.th32ThreadID );
            _tprintf( TEXT("\n     base priority  = %d"), te32.tpBasePri );
            _tprintf( TEXT("\n     delta priority = %d"), te32.tpDeltaPri );*/

            ret.push_back(te32.th32ThreadId);
        }
    }
    while( Thread32Next(hThreadSnap, &te32 ) );

    //_tprintf( TEXT("\n"));

//  Don't forget to clean up the snapshot object.
    CloseHandle( hThreadSnap );
    return ret;
}

void printError( TCHAR* msg )
{
    DWORD eNum;
    TCHAR sysMsg[256];
    TCHAR* p;

    eNum = GetLastError( );
    FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, eNum,
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                   sysMsg, 256, NULL );

    // Trim the end of the line and terminate it with a null
    p = sysMsg;
    while( ( *p > 31 ) || ( *p == 9 ) )
        ++p;
    do
    {
        *p-- = 0;
    }
    while( ( p >= sysMsg ) &&
            ( ( *p == '.' ) || ( *p < 33 ) ) );

    // Display the message
    _tprintf( TEXT("\n  WARNING: %s failed with error %d (%s)"), msg, eNum, sysMsg );
}
#endif // 0

#include <windows.h>
#include <imagehlp.h>	// link with imagehlp.lib as well...
#include <stdio.h>
#include <stdlib.h>
#include <winerror.h>

#if 0
// SymCleanup()
typedef BOOL (__stdcall *tSC)( IN HANDLE hProcess );
tSC pSC = NULL;

// SymFunctionTableAccess()
/*typedef PVOID IMAGEAPI (*tSFTA)( HANDLE hProcess, DWORD AddrBase );
tSFTA pSFTA = NULL;*/

using tSFTA = PFUNCTION_TABLE_ACCESS_ROUTINE;
tSFTA pSFTA = nullptr;

#ifdef NT50
// SymGetLineFromAddr()
typedef BOOL (__stdcall *tSGLFA)( IN HANDLE hProcess, IN DWORD dwAddr,
                                  OUT PDWORD pdwDisplacement, OUT PIMAGEHLP_LINE Line );
tSGLFA pSGLFA = NULL;
#endif

// SymGetModuleBase()
typedef DWORD (__stdcall *tSGMB)( IN HANDLE hProcess, IN DWORD dwAddr );
tSGMB pSGMB = NULL;

// SymGetModuleInfo()
typedef BOOL (__stdcall *tSGMI)( IN HANDLE hProcess, IN DWORD dwAddr, OUT PIMAGEHLP_MODULE ModuleInfo );
tSGMI pSGMI = NULL;

// SymGetOptions()
typedef DWORD (__stdcall *tSGO)( VOID );
tSGO pSGO = NULL;

// SymGetSymFromAddr()
typedef BOOL (__stdcall *tSGSFA)( IN HANDLE hProcess, IN DWORD dwAddr,
                                  OUT PDWORD pdwDisplacement, OUT PIMAGEHLP_SYMBOL Symbol );
tSGSFA pSGSFA = NULL;

// SymInitialize()
typedef BOOL (__stdcall *tSI)( IN HANDLE hProcess, IN PSTR UserSearchPath, IN BOOL fInvadeProcess );
tSI pSI = NULL;

// SymSetOptions()
typedef DWORD (__stdcall *tSSO)( IN DWORD SymOptions );
tSSO pSSO = NULL;

// StackWalk()
typedef BOOL (__stdcall *tSW)( DWORD MachineType, HANDLE hProcess,
                               HANDLE hThread, LPSTACKFRAME StackFrame, PVOID ContextRecord,
                               PREAD_PROCESS_MEMORY_ROUTINE ReadMemoryRoutine,
                               PFUNCTION_TABLE_ACCESS_ROUTINE FunctionTableAccessRoutine,
                               PGET_MODULE_BASE_ROUTINE GetModuleBaseRoutine,
                               PTRANSLATE_ADDRESS_ROUTINE TranslateAddress );
tSW pSW = NULL;

// UnDecorateSymbolName()
typedef DWORD (__stdcall WINAPI *tUDSN)( PCSTR DecoratedName, PSTR UnDecoratedName,
        DWORD UndecoratedLength, DWORD Flags );
tUDSN pUDSN = NULL;
#endif // 0

#define gle GetLastError()


#define sizeof_Name	128
#define sizeof_CONTEXT	sizeof(CONTEXT)+96
#define sizeof_STACKFRAME	sizeof(STACKFRAME64)+16
#define sizeof_symbol	sizeof(IMAGEHLP_SYMBOL64)+sizeof_Name

#ifndef CONTEXT_AMD64
static_assert(false);
#endif // __x86_64__


int GetCallingFunctionName(HANDLE hThread, std::string &callingFunctionName)
{
    HANDLE	hProc;
    IMAGEHLP_SYMBOL64	*sym;
    STACKFRAME64	*frm;
    DWORD	machType, lastErr, filepathlen;
    DWORD64 displacement = 0;
    BOOL	stat;
    int	i;
    char	filepath[MAX_PATH], *lastdir, *pPath;

    HINSTANCE hImagehlpDll = NULL;

// we load imagehlp.dll dynamically because the NT4-version does not
// offer all the functions that are in the NT5 lib
    hImagehlpDll = LoadLibrary( "imagehlp.dll" );
    if ( hImagehlpDll == NULL )
    {
        printf( "LoadLibrary( \"imagehlp.dll\" ): gle = %lu\n", gle );
        return 0;
    }

    /*pSC = (tSC) GetProcAddress( hImagehlpDll, "SymCleanup" );
    pSFTA = (tSFTA) GetProcAddress( hImagehlpDll, "SymFunctionTableAccess" );
    pSGMB = (tSGMB) GetProcAddress( hImagehlpDll, "SymGetModuleBase" );
    pSGMI = (tSGMI) GetProcAddress( hImagehlpDll, "SymGetModuleInfo" );
    pSGO = (tSGO) GetProcAddress( hImagehlpDll, "SymGetOptions" );
    pSGSFA = (tSGSFA) GetProcAddress( hImagehlpDll, "SymGetSymFromAddr" );
    pSI = (tSI) GetProcAddress( hImagehlpDll, "SymInitialize" );
    pSSO = (tSSO) GetProcAddress( hImagehlpDll, "SymSetOptions" );
    pSW = (tSW) GetProcAddress( hImagehlpDll, "StackWalk64" );
    pUDSN = (tUDSN) GetProcAddress( hImagehlpDll, "UnDecorateSymbolName" );*/

    /*pSC = SymCleanup;
    pSFTA = SymFunctionTableAccess;
    pSGMB =   SymGetModuleBase;
    pSGMI =    SymGetModuleInfo;
    pSGO =    SymGetOptions;
    pSGSFA =    SymGetSymFromAddr;
    pSI =    SymInitialize;
    pSSO =    SymSetOptions;
    pSW =    StackWalk64;
    pUDSN =    UnDecorateSymbolName;*/


    /*if ( pSC == NULL || pSFTA == NULL || pSGMB == NULL || pSGMI == NULL ||
            pSGO == NULL || pSGSFA == NULL || pSI == NULL || pSSO == NULL ||
            pSW == NULL || pUDSN == NULL )
    {
        puts( "GetProcAddress(): some required function not found." );
        FreeLibrary( hImagehlpDll );
    }*/



// Initialize the IMAGEHLP package to decode addresses to symbols
//

// Get image filename of the main executable

    filepathlen = GetModuleFileName ( NULL, filepath, sizeof(filepath));
    if (filepathlen == 0)
        printf ("NtStackTrace: Failed to get pathname for program\n");

// Strip the filename, leaving the path to the executable

    lastdir = strrchr (filepath, '/');
    if (lastdir == NULL)
        lastdir = strrchr (filepath, '\\');
    if (lastdir != NULL)
        lastdir[0] = '\0';

// Initialize the symbol table routines, supplying a pointer to the path

    pPath = filepath;
    if (strlen (filepath) == 0)
        pPath = NULL;

    hProc = GetCurrentProcess ();
    //hThread = GetCurrentThread ();
    if ( !SymInitialize(hProc, pPath, TRUE) )
        printf ("NtStackTrace: failed to initialize symbols\n");

// Allocate and initialize frame and symbol structures

    frm = (STACKFRAME64*)malloc (sizeof_STACKFRAME);
    memset (frm, 0, sizeof_STACKFRAME);

    sym = (IMAGEHLP_SYMBOL64*)malloc (sizeof_symbol);
    memset (sym, 0, sizeof_symbol);
    sym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL64);
    sym->MaxNameLength = sizeof_Name-1;


// Initialize the starting point based on the architecture of the current machine

    machType = IMAGE_FILE_MACHINE_AMD64;

    CONTEXT ctx;
    memset(&ctx, 0, sizeof(CONTEXT));

    ctx.ContextFlags = CONTEXT_CONTROL;

    bool res = GetThreadContext(hThread, &ctx);

    if(!res)
    {
        std::cout << "get thread context failed with " << GetLastError() << std::endl;

        assert(false);
        return -1;
    }

// The CONTEXT structure is not used on x86 systems

//	Initialize the STACKFRAME to describe the current routine

    frm->AddrPC.Mode = AddrModeFlat;
    frm->AddrStack.Mode = AddrModeFlat;
    frm->AddrFrame.Mode = AddrModeFlat;

    frm->AddrStack.Offset = ctx.Rsp;
    frm->AddrFrame.Offset = ctx.Rbp;
    frm->AddrPC.Offset = ctx.Rip;

    /*pSC = SymCleanup;
    pSFTA = SymFunctionTableAccess;
    pSGMB =   SymGetModuleBase;
    pSGMI =    SymGetModuleInfo;
    pSGO =    SymGetOptions;
    pSGSFA =    SymGetSymFromAddr;
    pSI =    SymInitialize;
    pSSO =    SymSetOptions;
    pSW =    StackWalk64;
    pUDSN =    UnDecorateSymbolName;*/

    std::cout << "fram " << frm->AddrFrame.Offset << std::endl;




    for (i=0; i<3; i++)
    {

// Call the routine to trace to the next frame

        stat = StackWalk64( machType, hProc, hThread, frm, &ctx, NULL, SymFunctionTableAccess64, SymGetModuleBase64, NULL );

        std::cout << "frame? " << frm->AddrFrame.Offset << std::endl;

        if ( !stat )
        {
            lastErr = GetLastError ();
            if (lastErr == ERROR_NOACCESS || lastErr == ERROR_INVALID_ADDRESS)
                printf (" <done>\n");	// Normal end-of-stack code
            else
                printf (" <stack walk terminated with error %d>\n",
                        lastErr);
            break;
        }
    }

// Decode the closest routine symbol name

    if ( SymGetSymFromAddr( hProc, frm->AddrPC.Offset, &displacement, sym ) )
        callingFunctionName = sym->Name;
    else
    {
        lastErr = GetLastError ();
        if (lastErr == ERROR_INVALID_ADDRESS)	// Seems normal for last frame on Intel
            callingFunctionName = "<No Symbol Available>";
        else
            callingFunctionName = std::to_string(lastErr);
    }



    /*free (ctx);	// If on Intel, freeing the NULL CONTEXT is a no-op...
    free (frm);
    free (sym);*/

    return 0;
}

/*std::string thread_to_stack(HANDLE h)
{
    ///pseudo handle
    HANDLE hProcess = GetCurrentProcess();

    STACKFRAME stack_frame;
    memset(&stack_frame, 0, sizeof(STACKFRAME));

    CONTEXT ctx;
    memset(&ctx, 0, sizeof(CONTEXT));

    GetThreadContext(h, &ctx);

    //StackWalk64(IMAGE_FILE_MACHINE_AMD64, hProcess, h, &stack_frame, &ctx, nullptr, )
}*/

std::string get_all_thread_stacktraces()
{
    //std::vector<int> thread_ids = ListProcessThreads(GetCurrentProcessId() );

    std::vector<HANDLE> thread_ids = get_thread_registration().fetch();

    for(HANDLE h : thread_ids)
    {

    }

    std::string out;

    GetCallingFunctionName(GetCurrentThread(), out);

    return out;
}

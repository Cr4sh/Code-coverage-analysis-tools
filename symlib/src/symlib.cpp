#include "stdafx.h"

typedef struct _SYMLIB_SYMBOL_INFO
{
    std::string Name;
    DWORD64 Offset;

} SYMLIB_SYMBOL_INFO,
*PSYMLIB_SYMBOL_INFO;

typedef std::map<std::string, SYMLIB_SYMBOL_INFO> SYMBOLS_LIST;

typedef struct _SYMLIB_MODULE_INFO
{
    HMODULE hModule;
    std::string ModuleName;
    SYMBOLS_LIST SymbolsList;

} SYMLIB_MODULE_INFO,
*PSYMLIB_MODULE_INFO;

typedef std::map<std::string, SYMLIB_MODULE_INFO> MODULES_LIST;

typedef struct _ENUM_CONTEXT
{
    DWORD64 dwModuleImageBase;

    DWORD64 dwBestSymbolOffset;
    DWORD dwBestSymbolDelta;

    DWORD64 dwSymbolOffset;    
    char *lpszSymbolName;

} ENUM_CONTEXT,
*PENUM_CONTEXT;

typedef BOOL (WINAPI * SYMLIB_ENUM_HANDLER)(
    PSYMLIB_SYMBOL_INFO SymbolInfo, 
    PENUM_CONTEXT Context
);

#ifdef PYTHON25
#define PYTHON_MODULE_NAME "symlib25"
#elif PYTHON26
#define PYTHON_MODULE_NAME "symlib"
#else
#error Python version is not specified
#endif

MODULES_LIST m_ModulesList;
//--------------------------------------------------------------------------------------
char *GetNameFromFullPath(const char *lpszPath)
{
    char *lpszName = (char *)lpszPath;

    for (int i = 0; i < strlen(lpszPath); i++)
    {
        if (lpszPath[i] == '\\' || lpszPath[i] == '/')
        {
            lpszName = (char *)lpszPath + i + 1;
        }
    }

    return lpszName;
}
//--------------------------------------------------------------------------------------
BOOL WINAPI FindSymbolByName(
    PSYMLIB_SYMBOL_INFO SymbolInfo, 
    PENUM_CONTEXT Context)
{
    // match symbol name
    if (!strcmp(SymbolInfo->Name.c_str(), Context->lpszSymbolName))
    {
        // stop enumeration
        Context->dwSymbolOffset = SymbolInfo->Offset;
        return FALSE;
    }

    return TRUE;
}
//--------------------------------------------------------------------------------------
BOOL WINAPI FindBestSymbolByAddress(
    PSYMLIB_SYMBOL_INFO SymbolInfo, 
    PENUM_CONTEXT Context)
{
    if (Context->dwSymbolOffset >= SymbolInfo->Offset)
    {
        DWORD dwDelta = (DWORD)(Context->dwSymbolOffset - SymbolInfo->Offset);

        if (dwDelta < Context->dwBestSymbolDelta || Context->dwBestSymbolOffset == 0)
        {
            Context->dwBestSymbolOffset = SymbolInfo->Offset;
            Context->dwBestSymbolDelta = dwDelta;
        }
    }

    return TRUE;
}
//--------------------------------------------------------------------------------------
BOOL WINAPI FindSymbolByAddress(
    PSYMLIB_SYMBOL_INFO SymbolInfo, 
    PENUM_CONTEXT Context)
{
    if (Context->dwSymbolOffset == SymbolInfo->Offset)
    {
        const char *lpszName = SymbolInfo->Name.c_str();

        size_t NameLen = strlen(lpszName);
        if (Context->lpszSymbolName = (char *)malloc(NameLen + 1))
        {
            strcpy(Context->lpszSymbolName, lpszName);
        }

        return FALSE;
    }

    return TRUE;
}
//--------------------------------------------------------------------------------------
BOOL CALLBACK SymlibLoadModuleSymbols(
    PSYMBOL_INFO pSymInfo,
    ULONG SymbolSize,
    PVOID UserContext)
{
    PSYMLIB_MODULE_INFO ModuleInfo = (PSYMLIB_MODULE_INFO)UserContext;

    try
    {
        SYMLIB_SYMBOL_INFO SymbolInfo;
        SymbolInfo.Name = std::string((char *)pSymInfo->Name);
        SymbolInfo.Offset = pSymInfo->Address - (DWORD64)ModuleInfo->hModule;

        // save symbol information
        ModuleInfo->SymbolsList[SymbolInfo.Name] = SymbolInfo;
    }        
    catch (...)
    {
        printf(__FUNCTION__"(): Exception occurs\n");
    }

    return TRUE;
}
//--------------------------------------------------------------------------------------
HMODULE SymlibLoadModule(const char *lpszModuleName)
{
    try
    {
        std::string ModuleName = std::string(lpszModuleName);

        // check for the allready loaded module
        MODULES_LIST::iterator it = m_ModulesList.find(ModuleName);
        if (it != m_ModulesList.end())
        {
            // return existing module handle
            return it->second.hModule;
        }

        // load target module
        HMODULE hModule = LoadLibraryEx(lpszModuleName, NULL, 0);
        if (hModule)
        {

#ifdef DBG

            char szModulePath[MAX_PATH];
            GetModuleFileName(hModule, szModulePath, MAX_PATH);
            DbgMsg(__FILE__, __LINE__, "SYMLIB: Module loaded from \"%s\"\n", szModulePath);

#endif // DBG

            // try to load debug symbols for module
            if (SymLoadModuleEx(GetCurrentProcess(), NULL, GetNameFromFullPath(lpszModuleName), NULL, (DWORD64)hModule, 0, NULL, 0))
            {
                SYMLIB_MODULE_INFO ModuleInfo;
                ModuleInfo.hModule = hModule;
                ModuleInfo.ModuleName = ModuleName;

                // get specified symbol address by name
                if (SymEnumSymbols(
                    GetCurrentProcess(),
                    (DWORD64)hModule,
                    NULL,
                    SymlibLoadModuleSymbols,
                    (PVOID)&ModuleInfo))
                {
                    DbgMsg(
                        __FILE__, __LINE__, 
                        "SYMLIB: %d symbols loaded for \"%s\"\n", 
                        ModuleInfo.SymbolsList.size(), lpszModuleName
                    );
                }
                else
                {
                    DbgMsg(__FILE__, __LINE__, "SymEnumSymbols() ERROR 0x%.8x\n", GetLastError());
                }

                // save module information
                m_ModulesList[ModuleName] = ModuleInfo;

                return hModule;
            }

            FreeLibrary(hModule);
        }
        else
        {
            DbgMsg(__FILE__, __LINE__, "LoadLibraryEx() ERROR 0x%.8x\n", GetLastError());
        }
    }
    catch (...)
    {
        printf(__FUNCTION__"(): Exception occurs\n");
    }   

    return NULL;
}
//--------------------------------------------------------------------------------------
BOOL SymlibEnumerateSymbols(
    const char *lpszModuleName, 
    SYMLIB_ENUM_HANDLER Handler, 
    PENUM_CONTEXT Context)
{
    // load target module
    HMODULE hModule = SymlibLoadModule(lpszModuleName);
    if (hModule)
    {
        SYMLIB_MODULE_INFO ModuleInfo = m_ModulesList[std::string(lpszModuleName)];        

        // enumerate available symbols
        SYMBOLS_LIST::iterator it = ModuleInfo.SymbolsList.begin();
        for (it; it != ModuleInfo.SymbolsList.end(); ++it) 
        {
            SYMLIB_SYMBOL_INFO SymbolInfo = it->second;

            // call caller-specified symbol handler
            if (!Handler(&SymbolInfo, Context))
            {
                // stop enumeration
                break;
            }
        }

        return TRUE;
    }
    
    return FALSE;
}
//--------------------------------------------------------------------------------------
PyObject *addrbyname(PyObject* self, PyObject* pArgs)
{   
    PyObject *Ret = Py_None;
    char *lpszSymbolName = NULL, *lpszModuleName = NULL;
    
    Py_INCREF(Py_None);

    if (!PyArg_ParseTuple(pArgs, "ss", &lpszModuleName, &lpszSymbolName)) 
    {
        DbgMsg(__FILE__, __LINE__, __FUNCTION__"(): Error while parsing input arguments\n");
        goto end;
    }    

    ENUM_CONTEXT Context;
    ZeroMemory(&Context, sizeof(Context));
    Context.lpszSymbolName = lpszSymbolName;

    if (SymlibEnumerateSymbols(lpszModuleName, FindSymbolByName, &Context))
    {
        if (Context.dwSymbolOffset > 0)
        {
            Ret = PyLong_FromUnsignedLong((DWORD)Context.dwSymbolOffset);
            Py_DECREF(Py_None);
        }
        else
        {
            DbgMsg(__FILE__, __LINE__, __FUNCTION__"(): Symbol is not found\n");
        }
    }
    else
    {
        DbgMsg(__FILE__, __LINE__, __FUNCTION__"(): Error while enumerating symbols\n");
    }       

end:  
     
    return Ret;
}
//--------------------------------------------------------------------------------------
PyObject *namebyaddr(PyObject* self, PyObject* pArgs)
{
    PyObject *Ret = Py_None;
    char *lpszModuleName = NULL;
    DWORD dwOffset = 0;
    
    Py_INCREF(Py_None);

    if (!PyArg_ParseTuple(pArgs, "sk", &lpszModuleName, &dwOffset)) 
    {
        DbgMsg(__FILE__, __LINE__, __FUNCTION__"(): Error while parsing input arguments\n");
        goto end;
    }    

    ENUM_CONTEXT Context;
    ZeroMemory(&Context, sizeof(Context));
    Context.dwSymbolOffset = (DWORD64)dwOffset;

    if (SymlibEnumerateSymbols(lpszModuleName, FindSymbolByAddress, &Context))
    {
        if (Context.lpszSymbolName)
        {
            Ret = PyString_FromString((const char *)Context.lpszSymbolName);

            free(Context.lpszSymbolName);
            Py_DECREF(Py_None);
        }
        else
        {
            DbgMsg(__FILE__, __LINE__, __FUNCTION__"(): Symbol is not found\n");
        }
    }
    else
    {
        DbgMsg(__FILE__, __LINE__, __FUNCTION__"(): Error while enumerating symbols\n");
    }        

end:  
      
    return Ret;
}
//--------------------------------------------------------------------------------------
PyObject *bestbyaddr(PyObject* self, PyObject* pArgs)
{
    PyObject *Ret = Py_None;
    char *lpszModuleName = NULL;
    DWORD dwOffset = 0;

    Py_INCREF(Py_None);

    if (!PyArg_ParseTuple(pArgs, "sk", &lpszModuleName, &dwOffset)) 
    {
        DbgMsg(__FILE__, __LINE__, __FUNCTION__"(): Error while parsing input arguments\n");
        goto end;
    }    

    ENUM_CONTEXT Context;
    ZeroMemory(&Context, sizeof(Context));
    Context.dwSymbolOffset = (DWORD64)dwOffset;

    if (SymlibEnumerateSymbols(lpszModuleName, FindBestSymbolByAddress, &Context))
    {
        if (Context.dwBestSymbolOffset > 0)
        {
            Context.dwSymbolOffset = Context.dwBestSymbolOffset;
            if (SymlibEnumerateSymbols(lpszModuleName, FindSymbolByAddress, &Context))
            {
                if (Context.lpszSymbolName)
                {                    
                    Ret = PyList_New(0);
                    PyList_Insert(Ret, 0, PyString_FromString((const char *)Context.lpszSymbolName));
                    PyList_Insert(Ret, 1, PyLong_FromUnsignedLong(Context.dwBestSymbolDelta));

                    free(Context.lpszSymbolName);
                    Py_DECREF(Py_None);
                }
                else
                {
                    DbgMsg(__FILE__, __LINE__, __FUNCTION__"(): Symbol is not found\n");
                }
            }
            else
            {
                DbgMsg(__FILE__, __LINE__, __FUNCTION__"(): Error while enumerating symbols\n");
            } 
        }
        else
        {
            DbgMsg(__FILE__, __LINE__, __FUNCTION__"(): Symbol is not found\n");
        }
    }
    else
    {
        DbgMsg(__FILE__, __LINE__, __FUNCTION__"(): Error while enumerating symbols\n");
    }        

end:  

    return Ret;
}
//--------------------------------------------------------------------------------------
static PyMethodDef m_Methods[] = 
{
    { "addrbyname", addrbyname, METH_VARARGS, "Get symbol offset by name."                      },
    { "namebyaddr", namebyaddr, METH_VARARGS, "Get symbol name by offset."                      },
    { "bestbyaddr", bestbyaddr, METH_VARARGS, "Get the more suitable symbol name by address."   },
    { NULL,         NULL,       0,            NULL                                              }
};

BOOL SymlibInitialize(void)
{   
    // initialize python module
    Py_InitModule(PYTHON_MODULE_NAME, m_Methods);

    return TRUE;
}
//--------------------------------------------------------------------------------------
BOOL SymlibUninitialize(void)
{   
    MODULES_LIST::iterator it = m_ModulesList.begin();

    // enumerate loaded modules
    for (it; it != m_ModulesList.end(); ++it) 
    {
        // unload module
        SymUnloadModule64(GetCurrentProcess(), (DWORD64)it->second.hModule);
        FreeLibrary(it->second.hModule);
    }

    // flush modules list list
    m_ModulesList.clear();

    return TRUE;
}
//--------------------------------------------------------------------------------------
PyMODINIT_FUNC initsymlib(void)
{
    SymlibInitialize();
}
//--------------------------------------------------------------------------------------
PyMODINIT_FUNC initsymlib25(void)
{
    SymlibInitialize();
}
//--------------------------------------------------------------------------------------
BOOL APIENTRY DllMain( 
    HMODULE hModule,
    DWORD ul_reason_for_call,
    LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:        

        DbgMsg(__FILE__, __LINE__, "SYMLIB: DLL_PROCESS_ATTACH\n");        

        char szSymbolsPath[MAX_PATH], szSymbolsDir[MAX_PATH];
        GetCurrentDirectory(MAX_PATH - 1, szSymbolsDir);
        strcat(szSymbolsDir, "\\Symbols");

        // create directory for debug symbols
        CreateDirectory(szSymbolsDir, NULL);

        sprintf(
            szSymbolsPath, 
            "%s;SRV*%s*http://msdl.microsoft.com/download/symbols", 
            szSymbolsDir, szSymbolsDir
        );

        // set symbol path and initialize symbol server client
        if (!SymInitialize(GetCurrentProcess(), szSymbolsPath, FALSE))
        {
            DbgMsg(__FILE__, __LINE__, "SymInitialize() ERROR %d\n", GetLastError());
            return FALSE;
        }
        else
        {
            DbgMsg(__FILE__, __LINE__, "SYMLIB: Symbols path is \"%s\"\n", szSymbolsPath);
        }

        break;

    case DLL_PROCESS_DETACH:

        DbgMsg(__FILE__, __LINE__, "SYMLIB: DLL_PROCESS_DETACH\n");

        SymlibUninitialize();

        break;
    }

    return TRUE;
}
//--------------------------------------------------------------------------------------
// EoF

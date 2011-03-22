/*
=========================================================================

    Code coverage analysis tool: 
    Testing program.

    This program runs IEXPLORE.EXE with PIN Toolkit, and generates code 
    coverage map for it.

    Usage:

    Just run coverage_test_with_pin.bat command line scenario.

    Developed by:

    Oleksiuk Dmitry, eSage Lab
    mailto:dmitry@esagelab.com
    http://www.esagelab.com/

=========================================================================
*/

#include "stdafx.h"

// image file execution options key name
#define IFEO_KEY_NAME "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options"

// 'Debugger' option name for image file execution options key
#define IFEO_VAL_NAME "Debugger"

#define APPPATH_KEY_NAME "SOFTWARE\\coverage_test"
#define APPPATH_VAL_NAME "cmdline"

#define IECFG_KEY_NAME "Software\\Microsoft\\Internet Explorer\\Main"
#define IECFG_VAL_NAME "TabProcGrowth"

#define TEST_URL L"http://google.com"
//--------------------------------------------------------------------------------------
BOOL ExecCmd(DWORD *exitcode, char *cmd)
{
    BOOL ret = FALSE;

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    memset(&pi, 0, sizeof(pi));
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);    

    if (exitcode)
    {
        *exitcode = 0;
    }

    // create suspended process
    if (CreateProcess(NULL, cmd, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
    {               
        // execute process and wait for the termination
        ResumeThread(pi.hThread);
        WaitForSingleObject(pi.hProcess, INFINITE);

        if (exitcode)
        {
            // query process exit code
            if (GetExitCodeProcess(pi.hProcess, exitcode))
            {
                ret = TRUE;
            }
            else
            {
                printf("ERROR: GetExitCodeProcess() fails; code: %d\n", GetLastError());
            }
        }
        else
        {
            ret = TRUE;
        }        
    }
    else
    {
        printf("ERROR: CreateProcess() fails; code: %d\n", GetLastError());
    }  

    return ret;
}
//--------------------------------------------------------------------------------------
BOOL IeOpenUrl(PCWSTR lpszUrl, DWORD dwNum)
{    
    printf(__FUNCTION__"(): Opening \"%ws\"...\n", lpszUrl);

    // create web-browser instance
    IWebBrowser2 *pBrowser = NULL;
    HRESULT hr = CoCreateInstance(
        CLSID_InternetExplorer, 
        NULL, 
        CLSCTX_LOCAL_SERVER, 
        IID_IWebBrowser2, 
        (PVOID *)&pBrowser
    );
    if (SUCCEEDED(hr)) 
    {        
        VARIANT vEmpty;
        VariantInit(&vEmpty);

        pBrowser->put_Visible(VARIANT_TRUE);

        BSTR bUrl = SysAllocString(lpszUrl);
        if (bUrl)
        {            
            for (DWORD i = 0; i < dwNum; i++)
            {
                // go to the our url
                hr = pBrowser->Navigate(bUrl, &vEmpty, &vEmpty, &vEmpty, &vEmpty);
                if (SUCCEEDED(hr))
                {
                    while (true)
                    {
                        VARIANT_BOOL vBusy;

                        // loop untill page not loaded
                        hr = pBrowser->get_Busy(&vBusy);
                        if (SUCCEEDED(hr))
                        {
                            if (vBusy == VARIANT_FALSE)
                            {   
                                printf(__FUNCTION__"(): DONE\n");

                                Sleep(1000);

                                break;
                            }
                        }
                        else
                        {
                            printf("pBrowser->get_Busy() ERROR 0x%.8x\n", hr);
                            break;
                        }

                        Sleep(100);
                    }                             
                }
                else
                {
                    printf("pBrowser->Navigate() ERROR 0x%.8x\n", hr);
                }                
            }            

            SysFreeString(bUrl);
        }    
        else
        {
            printf("SysAllocString() ERROR\n");
        }

        pBrowser->Quit();        
    }
    else
    {
        printf("CoCreateInstance() ERROR 0x%.8x\n", hr);
    }

    return TRUE;
}
//--------------------------------------------------------------------------------------
BOOL SetImageExecutionDebuggerOption(char *lpszImage, char *lpszValue)
{
    BOOL bRet = FALSE;
    char szKeyName[MAX_PATH];
    HKEY hKey;

    strcpy_s(szKeyName, MAX_PATH, IFEO_KEY_NAME);
    strcat_s(szKeyName, MAX_PATH, "\\");
    strcat_s(szKeyName, MAX_PATH, lpszImage);

    // create key for this executable image
    LONG Code = RegCreateKey(HKEY_LOCAL_MACHINE, szKeyName, &hKey);
    if (Code == ERROR_SUCCESS)
    {
        if (lpszValue)
        {
            DWORD dwDataSize = lstrlen(lpszValue) + 1;

            // set debugging options for this image
            Code = RegSetValueEx(hKey, IFEO_VAL_NAME, 0, REG_SZ, (PBYTE)lpszValue, dwDataSize);
            if (Code == ERROR_SUCCESS)
            {
                // done...
                bRet = TRUE;
            }
            else
            {
                printf(__FUNCTION__"() ERROR: RegSetValueEx() fails; Code: %d\n", Code);
            }
        }   
        else
        {
            // delete existing value
            bRet = RegDeleteValue(hKey, IFEO_VAL_NAME);
        }

        RegCloseKey(hKey);
    }
    else
    {
        printf(__FUNCTION__"() ERROR: RegCreateKey() fails; Code: %d\n", Code);
    }

    return bRet;
}
//--------------------------------------------------------------------------------------
BOOL SetOptionalApp(char *lpszValue)
{
    BOOL bRet = FALSE;
    HKEY hKey;

    LONG Code = RegCreateKey(HKEY_LOCAL_MACHINE, APPPATH_KEY_NAME, &hKey);
    if (Code == ERROR_SUCCESS)
    {
        if (lpszValue)
        {
            DWORD dwDataSize = lstrlen(lpszValue) + 1;

            Code = RegSetValueEx(hKey, APPPATH_VAL_NAME, 0, REG_SZ, (PBYTE)lpszValue, dwDataSize);
            if (Code == ERROR_SUCCESS)
            {
                // done...
                bRet = TRUE;
            }
            else
            {
                printf(__FUNCTION__"() ERROR: RegSetValueEx() fails; Code: %d\n", Code);
            }
        }   
        else
        {
            // delete existing value
            bRet = RegDeleteValue(hKey, APPPATH_VAL_NAME);
        }

        RegCloseKey(hKey);
    }
    else
    {
        printf(__FUNCTION__"() ERROR: RegCreateKey() fails; Code: %d\n", Code);
    }

    return bRet;
}
//--------------------------------------------------------------------------------------
BOOL GetOptionalApp(char *lpszValue, DWORD dwValueLen)
{
    BOOL bRet = FALSE;
    HKEY hKey;

    LONG Code = RegCreateKey(HKEY_LOCAL_MACHINE, APPPATH_KEY_NAME, &hKey);
    if (Code == ERROR_SUCCESS)
    {        
        DWORD dwDataSize = dwValueLen;

        Code = RegQueryValueEx(hKey, APPPATH_VAL_NAME, NULL, NULL, (LPBYTE)lpszValue, &dwDataSize);
        if (Code == ERROR_SUCCESS)
        {
            // done...
            bRet = TRUE;
        }
        else
        {
            printf(__FUNCTION__"() ERROR: RegQueryValueEx() fails; Code: %d\n", Code);
        }       

        RegCloseKey(hKey);
    }
    else
    {
        printf(__FUNCTION__"() ERROR: RegCreateKey() fails; Code: %d\n", Code);
    }

    return bRet;
}
//--------------------------------------------------------------------------------------
BOOL DisableIeMultiprocessMode(void)
{
    BOOL bRet = FALSE;
    HKEY hKey;

    LONG Code = RegCreateKey(HKEY_LOCAL_MACHINE, IECFG_KEY_NAME, &hKey);
    if (Code == ERROR_SUCCESS)
    {        
        DWORD dwTabProcGrowth = 0;

        Code = RegSetValueEx(hKey, IECFG_VAL_NAME, 0, REG_DWORD, (const BYTE *)&dwTabProcGrowth, sizeof(DWORD));
        if (Code == ERROR_SUCCESS)
        {
            // done...
            bRet = TRUE;
        }
        else
        {
            printf(__FUNCTION__"() ERROR: RegSetValueEx() fails; Code: %d\n", Code);
        }       

        RegCloseKey(hKey);
    }
    else
    {
        printf(__FUNCTION__"() ERROR: RegCreateKey() fails; Code: %d\n", Code);
    }

    return bRet;
}
//--------------------------------------------------------------------------------------
BOOL EnableIeMultiprocessMode(void)
{
    BOOL bRet = FALSE;
    HKEY hKey;

    LONG Code = RegCreateKey(HKEY_LOCAL_MACHINE, IECFG_KEY_NAME, &hKey);
    if (Code == ERROR_SUCCESS)
    {        
        bRet = RegDeleteValue(hKey, IECFG_VAL_NAME);
        RegCloseKey(hKey);
    }
    else
    {
        printf(__FUNCTION__"() ERROR: RegCreateKey() fails; Code: %d\n", Code);
    }

    return bRet;
}
//--------------------------------------------------------------------------------------
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) 
{ 
    if (fdwCtrlType == CTRL_C_EVENT || 
        fdwCtrlType == CTRL_CLOSE_EVENT) 
    { 
        printf("Terminating program...\n");

        // Handle the CTRL-C signal. 
        EnableIeMultiprocessMode();
        SetOptionalApp(NULL);

        ExitProcess(0);
    } 

    return FALSE;
}
//--------------------------------------------------------------------------------------
int _tmain(int argc, _TCHAR* argv[])
{
    char *lpszCmdLine = GetCommandLine();
    char *lpszCmd = strstr(lpszCmdLine, "@ ");
    if (lpszCmd == NULL)
    {
        /*
            External program command line is NOT available: 
            we was started by the user form console.
        */
        char szSelfPath[MAX_PATH], szExecPath[MAX_PATH];
        GetModuleFileName(GetModuleHandle(NULL), szSelfPath, MAX_PATH);
        DWORD dwTestIterations = 1;

        SetConsoleCtrlHandler(CtrlHandler, TRUE);

        // register current executable as debugger for the IEXPLORE.EXE program
        sprintf_s(szExecPath, "\"%s\" @", szSelfPath);
        SetImageExecutionDebuggerOption("IEXPLORE.EXE", szExecPath);

        for (int i = 1; i < argc; i++)
        {
            if (!strcmp(argv[i], "--iterations") && argc > i + 1)
            {
                dwTestIterations = atoi(argv[i + 1]);
                printf("Iterations count: %d\n", dwTestIterations);
            }
            else if (!strcmp(argv[i], "--instrumentation-path") && argc > i + 1)
            {
                // instrumentation program (pin.exe) path has been specified, 
                // save it into the registry.
                SetOptionalApp(argv[i + 1]);
                printf("Instrumentation tool path: \"%s\"\n", argv[i + 1]);
            }
        }

        // initialize COM
        HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (FAILED(hr))
        {
            printf("CoInitializeEx() ERROR 0x%.8x\n", hr);
            goto end;
        }

        DWORD dwTime = GetTickCount();

        // change IE settings to swith it into the single process mode
        DisableIeMultiprocessMode();

        // this function executes Internet Explorer through COM and opens specified URL
        IeOpenUrl(TEST_URL, dwTestIterations);

        dwTime = GetTickCount() - dwTime;

        printf("Execution time: %d ms\n", dwTime);

        EnableIeMultiprocessMode();
        SetOptionalApp(NULL);
    }
    else
    {
        /*
            External program command line is available: 
            we was started by the system, 'cause "Debugger" option for
            the some program was set.
        */
        DWORD dwExitCode = 0;
        char szOptional[MAX_PATH];

        lpszCmd += 2;        

        // remove "Debugger" option
        SetImageExecutionDebuggerOption("IEXPLORE.EXE", NULL);

        // query optional path to the instrumentation program (pin.exe).
        if (GetOptionalApp(szOptional, MAX_PATH))
        {
            char szCmdLine[MAX_PATH];
            sprintf_s(szCmdLine, "\"%s\" %s", szOptional, lpszCmd);
            printf("CMDLINE: %s\n", szCmdLine);

            // execute external program with the instrumentation program
            ExecCmd(&dwExitCode, szCmdLine);
        }                
        else
        {
            // instrumentation program is not available, just execute specified external program
            printf("CMDLINE: %s\n", lpszCmd);
            ExecCmd(&dwExitCode, lpszCmd);
        }        
    }    

end:
    printf("Press any key to quit...\n");
    _getch();

	return 0;
}
//--------------------------------------------------------------------------------------
// EoF

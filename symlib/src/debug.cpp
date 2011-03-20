#include "stdafx.h"
//--------------------------------------------------------------------------------------
#ifdef DBG
//--------------------------------------------------------------------------------------
void DbgMsg(char *lpszFile, int iLine, char *lpszMsg, ...)
{
    va_list mylist;
    va_start(mylist, lpszMsg);

    int len = _vscprintf(lpszMsg, mylist) + 0x100;
    
    char *lpszBuff = (char *)malloc(len);
    if (lpszBuff == NULL)
    {
        va_end(mylist);
        return;
    }

    char *lpszOutBuff = (char *)malloc(len);
    if (lpszOutBuff == NULL)
    {
        free(lpszBuff);
        va_end(mylist);
        return;
    }
    
    vsprintf(lpszBuff, lpszMsg, mylist);
    va_end(mylist);

    sprintf(lpszOutBuff, "[%.5d] %s(%d) : %s", GetCurrentProcessId(), lpszFile, iLine, lpszBuff);	

    OutputDebugString(lpszOutBuff);
    
    HANDLE hStd = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStd != INVALID_HANDLE_VALUE)
    {
        DWORD dwWritten = 0;
        WriteFile(hStd, lpszBuff, strlen(lpszBuff), &dwWritten, NULL);
    }
        
    free(lpszBuff);
    free(lpszOutBuff);
}
//--------------------------------------------------------------------------------------
#endif DBG
//--------------------------------------------------------------------------------------
// EoF

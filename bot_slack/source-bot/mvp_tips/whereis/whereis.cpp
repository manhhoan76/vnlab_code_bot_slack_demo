// whereis.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "resource.h"

#define MAX_STRING 1024

/****************************************************************************
*                               GetErrorString
* Inputs:
*       DWORD err: Error code
*       LPCTSTR param: A string parameter (or NULL)
* Result: LPCTSTR
*       Pointer to string
* Effect: 
*       Allocates the buffer
* Notes:
*       The caller must call LocalFree to release the buffer
****************************************************************************/

LPCTSTR GetErrorString(DWORD err, LPCTSTR param)
   {
    LPTSTR msg;
    LPCTSTR args[] = {param};

    ::FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_ARGUMENT_ARRAY, 
                  NULL, err, 0, (LPTSTR)&msg, 0, (va_list *)args);
    return msg;
   }

/****************************************************************************
*                                   TryLoad
* Inputs:
*       LPCTSTR name: The filename to try
*       LPCTSTR ext: An extension to append, or NULL
* Result: HMODULE
*       The module handle, or NULL
* Effect: 
*       Loads the module
* Notes:
*       Use ::GetLastError to decide why the module handle is NULL
****************************************************************************/

HMODULE TryLoad(LPCTSTR name, LPCTSTR ext, LPTSTR fullname)
    {
     StringCchCopy(fullname, MAX_PATH, name);
     if(ext != NULL)
         { /* add extension */
          StringCchCat(fullname, MAX_PATH, ext);
         } /* add extension */
     HMODULE h = ::LoadLibraryEx(fullname, NULL, 
                                 DONT_RESOLVE_DLL_REFERENCES);
     return h;
    }

/****************************************************************************
*                                 ReportError
* Inputs:
*       DWORD err: Error code
* Result: void
*       
* Effect: 
*       Prints out the error message
****************************************************************************/

void ReportError(DWORD err, LPCTSTR param)
    {
     LPCTSTR p = GetErrorString(err, param);
     _tprintf(_T("%s\n"), p);
     LocalFree((LPVOID)p);
    } // ReportError

/****************************************************************************
*                                 ShowModule
* Inputs:
*       HMODULE h: Module handle to display
* Result: int
*       0 if successful
*       1 if error
* Effect: 
*       Displays the module name
****************************************************************************/

int ShowModule(HMODULE h, BOOL toclip)
    {
     TCHAR name[MAX_PATH];
     DWORD len = GetModuleFileName(h, name, MAX_PATH);

     if(len == 0)
        { /* failed to get name */
         DWORD err = ::GetLastError();
         ReportError(err, NULL);
         return 1;
        } /* failed to get name */
     _tprintf(_T("%s\n"), name);

     if(toclip)
        { /* save to clipboard */
         if(!::OpenClipboard(NULL))
            { /* failed to get clipboard */
             return 1;
            } /* failed to get clipboard */
         ::EmptyClipboard();
         HGLOBAL mem = ::GlobalAlloc(GMEM_MOVEABLE, (_tcslen(name) + 1) * sizeof(TCHAR));
         if(mem != NULL)
            { /* got it */
             LPTSTR p = (LPTSTR)::GlobalLock(mem);
             memcpy(p, name, _tcslen(name) + 1 * sizeof(TCHAR));
             ::GlobalUnlock(mem);
             ::SetClipboardData(CF_TEXT, mem);
            } /* got it */
         ::CloseClipboard();
        } /* save to clipboard */
     return 0;
    } // ShowModule

/****************************************************************************
*                                 QueryStringValue
* Inputs:
*       LPVOID info: VersionInfo block
*       LPCTSTR field: Query string field
*       LPTSTR * buffer: Place to put pointer to it
* Result: BOOL
*       TRUE if found
*       FALSE if error
* Effect: 
*       Reads the data
****************************************************************************/

#ifndef CP_UTF16
#define CP_UTF16 1200
#endif

BOOL QueryStringValue(void * const info, LPCTSTR field, LPTSTR *buffer)
    {
     TCHAR query[MAX_STRING];

     TCHAR idefaultlanguage_data[6];
     LCID lcid = LOCALE_USER_DEFAULT;
     if(::GetLocaleInfo(lcid, LOCALE_IDEFAULTLANGUAGE,
                      idefaultlanguage_data,
                      sizeof(idefaultlanguage_data)/sizeof(TCHAR)))
        { /* got lcid */
         StringCchPrintf(query, MAX_STRING, _T("StringFileInfo\\%s%04x\\%s"), idefaultlanguage_data, CP_UTF16, field);
         UINT len;
         return ::VerQueryValue(info, query, (LPVOID*)buffer, &len);

        } /* got lcid */
     return FALSE;
    } // QueryValue

/****************************************************************************
*                                ShowAppValule
* Inputs:
*       LPCTSTR desc: The descriptor (value name) of the value
*       LPCTSTR value: The value itself
*       DWORD type: The type of the value
*       DWORD width: The maximum width of all names
* Result: void
*       
* Effect: 
*       Shows the string, and if it is an expandable string, displays the
*       expanded value
****************************************************************************/

#define DEFAULT_NAME _T("(Default)")

void ShowAppValue(LPCTSTR desc, LPCTSTR value, DWORD type, DWORD width)
    {
     LPCTSTR valname = _tcslen(desc) == 0 ? DEFAULT_NAME : desc;
     switch(type)
        { /* type */
         case REG_SZ:
            _tprintf(_T("%*s => \"%s\"\n"), width, valname, value);
            break;
         case REG_EXPAND_SZ:
            _tprintf(_T("%*s => \"%s\"\n"), width, valname, value);
            { /* expand it */
             TCHAR Expanded[3 * MAX_PATH];
             DWORD len = ExpandEnvironmentStrings(value, Expanded, sizeof(Expanded) / sizeof(TCHAR)); 
             if(len > 0)
                _tprintf(_T("%*.s => \"%s\"\n"), width, _T(""), Expanded);
            } /* expand it */
            break;
         case REG_DWORD:
            { /* REG_DWORD */
             DWORD d = *(DWORD*)value;
             _tprintf(_T("%*s => 0x%08x (%d)\n"), width, valname, d, d);
            } /* REG_DWORD */
            break;
         default:
            return;
        } /* type */
    } // ShowAppValue

/****************************************************************************
*                                 ShowAppPath
* Inputs:
*       LPCTSTR app: App name, such as xxx.exe, or just xxx
* Result: void
*       
* Effect: 
*       Looks up the App Path and Path options
****************************************************************************/

void ShowAppPath(LPCTSTR app)
    {
     TCHAR file[MAX_PATH];
     TCHAR ext[MAX_PATH];
     _tsplitpath(app, NULL, NULL, file, ext);

     if(! (_tcslen(ext) == 0 || _tcsicmp(ext, _T(".exe")) == 0) )
        { /* no app path */
         return;
        } /* no app path */

#define APP_PATH_KEY  _T("Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\")
     TCHAR AppPathKey[MAX_PATH];
     StringCchCopy(AppPathKey, MAX_PATH, APP_PATH_KEY);

     StringCchCat(AppPathKey, MAX_PATH, file);
     StringCchCat(AppPathKey, MAX_PATH, _T(".exe"));

     HKEY key;
     LONG result = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE, AppPathKey, 0, KEY_QUERY_VALUE, &key);
     switch(result)
        { /* RegOpen */
         case ERROR_SUCCESS:
            break;
         default:
            return ;  // not found
        } /* RegOpen */

     _tprintf(_T("HKLM\\%s\n"), AppPathKey);

     DWORD maxlen = 0;
     
     result =  ::RegQueryInfoKey(key,
                                 NULL,        // class name of this key
                                 NULL,        // class name length for this key
                                 NULL,        // reserved
                                 NULL,        // subkeys
                                 NULL,        // max len subkeys
                                 NULL,        // class max length
                                 NULL,        // values
                                 &maxlen,     // max value name length
                                 NULL,        // max value length
                                 NULL,        // security descriptor
                                 NULL);       // FILETIME
     if(result != ERROR_SUCCESS || maxlen < _tcslen(DEFAULT_NAME))
        maxlen = max(16, _tcslen(DEFAULT_NAME));

     BOOL running = TRUE;

     for(DWORD i = 0; running; i++)
        { /* scan values */
         TCHAR name[MAX_PATH];
         DWORD nameLength = sizeof(name);
         TCHAR data[MAX_PATH];
         DWORD dataLength = sizeof(data);
         DWORD type;
         
         LONG result = RegEnumValue(key, i, name, &nameLength, NULL, &type, (LPBYTE)data, &dataLength);

         switch(result)
            { /* RegQueryValue */
             case ERROR_NO_MORE_ITEMS:
                running = FALSE;
                continue;
             case ERROR_SUCCESS:
                ShowAppValue(name, data, type, maxlen);
                break;
             case ERROR_FILE_NOT_FOUND:
                break;
             default:
                ::RegCloseKey(key);
                return;
            } /* RegQueryValue */
        } /* scan values */

     ::RegCloseKey(key);
    } // ShowAppPath

/****************************************************************************
*                                   _tmain
* Inputs:
*       argc:
*       argv:
* Result: int
*       0 if successful
*       1 if error
* Effect: 
*       Prints out the module source based on the search path
****************************************************************************/

int _tmain(int argc, TCHAR* argv[])
   {
    if(argc < 2 || _tcsicmp(argv[1], _T("?")) == 0)
       { /* missing arg */
        TCHAR usage[MAX_STRING];
        TCHAR module[MAX_PATH];
        TCHAR name[MAX_PATH];
        ::GetModuleFileName(NULL, module, MAX_PATH);
        _tsplitpath(module, NULL, NULL, name, NULL);
        ::LoadString(GetModuleHandle(NULL), IDS_USAGE, usage, MAX_STRING);
        _tprintf(usage, name);

        //******************
        // Show version info
        //******************

        DWORD ignored; // required, but unused
        DWORD size = ::GetFileVersionInfoSize(module, &ignored);
        LPVOID versionInfo = new BYTE[size];
        if(::GetFileVersionInfo(module, NULL, size, versionInfo))
           { /* has version info */
            TCHAR fmt[MAX_STRING];
            ::LoadString(::GetModuleHandle(NULL), IDS_VERSION_ID, fmt, MAX_STRING);

            VS_FIXEDFILEINFO * info;
            UINT len;
     
            ::VerQueryValue(versionInfo, _T("\\"), (LPVOID *)&info, &len);
            
            _tprintf(fmt, HIWORD(info->dwProductVersionMS),
                          LOWORD(info->dwProductVersionMS),
                          HIWORD(info->dwProductVersionLS),
                          LOWORD(info->dwProductVersionLS));


            //********************************
            // LOCALE_IDEFAULTLANGUAGE
            //********************************
            LPTSTR str;
            ::QueryStringValue(versionInfo, _T("LegalCopyright"), &str);
            _tprintf(_T("%s\n"), str);

            delete [ ] versionInfo;
           } /* has version info */
        return 1;
       } /* missing arg */

    BOOL toclip = FALSE;
    BOOL showerrors = FALSE;

    for(int i = 2; i < argc; i++)
       { /* check options */
        //****************
        // -c
        //****************
        if(_tcsicmp(argv[i], _T("-c")) == 0)
           { /* copy to clipboard */
            toclip = TRUE;
            continue;
           } /* copy to clipboard */
        //****************
        // -e
        //****************
        if(_tcsicmp(argv[i], _T("-e")) == 0)
           { /* show errors */
            showerrors = TRUE;
            continue;
           } /* show errors */
        
        //****************
        // unknown
        //****************
        TCHAR fmt[MAX_STRING];
        ::LoadString(GetModuleHandle(NULL), IDS_UNKNOWN_OPTION, fmt, MAX_STRING);
        _tprintf(fmt, argv[i]);
        return 1;
       } /* check options */

    if(!showerrors)
       ::SetErrorMode(SEM_FAILCRITICALERRORS);

    TCHAR fullname[MAX_PATH];
    HMODULE h = TryLoad(argv[1], NULL, fullname);
    if(h != NULL)
       { /* found it */
        BOOL result = ShowModule(h, toclip);
        ShowAppPath(argv[1]);
        return result;
       } /* found it */

    DWORD err = ::GetLastError(); // report this error

    TCHAR ext[MAX_PATH];
    _tsplitpath(argv[1], NULL, NULL, NULL, ext);

    if(_tcslen(ext) == 0)
       { /* had no extension */
        static const LPCTSTR extensions[] = {
           _T(".exe"), // This must be at offset 0!
           _T(".dll"),
           _T(".ocx"),
           _T(".sys"),
           _T(".com"),
           NULL};

        for(i = 0; extensions[i] != NULL; i++)
           { /* try each */
            h = TryLoad(argv[1], extensions[i], fullname);
            if(h != NULL)
               break;
            DWORD err2 = ::GetLastError();
            if(err2 != ERROR_MOD_NOT_FOUND)
               { /* really bad */
                err = err2;
                break;
               } /* really bad */
           } /* try each */
        
       } /* had no extension */

    if(h == NULL)
       { /* failed */
        ReportError(err, fullname);
        // Now see if it can be found in the Registry
        ShowAppPath(argv[1]);
        return 1;
       } /* failed */
    if(ShowModule(h, toclip) != 0)
        return 1;
    ShowAppPath(argv[1]);
    return 0;
   }


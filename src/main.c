#include <Windows.h>
#include <process.h>
#include <shellapi.h>
#include <tchar.h>

/*
** -------------------------------------------------
** SECTION Types
** -------------------------------------------------
*/

typedef enum W_execution {
    W_EXECUTION_DISPLAY_REQUIRED,
    W_EXECUTION_SYSTEM_REQUIRED,
    W_EXECUTION_NORMAL,
    W_EXECUTION_COUNT,
} W_execution;

typedef enum W_argument {
    W_ARGUMENT_NONE,
    W_ARGUMENT_ALLOW_SLEEP,
    W_ARGUMENT_NO_SLEEP,
    W_ARGUMENT_DISPLAY,
    W_ARGUMENT_ERROR,
} W_argument;

/*
** -------------------------------------------------
** SECTION Constants
** -------------------------------------------------
*/

enum {
    W_MENU_ID_KEEP_DISPLAY_ON = 1,
    W_MENU_ID_NO_SLEEP,
    W_MENU_ID_ALLOW_SLEEP,
    W_MENU_ID_EXIT,

    W_MENU_ID_STARTUP_DISPLAY,
    W_MENU_ID_STARTUP_NO_SLEEP,
    W_MENU_ID_STARTUP_ALLOW_SLEEP,
    W_MENU_ID_STARTUP_REMOVE,

    W_MENU_ID_SHOW_LOGS,
};

enum {
    W_MESSAGE_ID_TRAY = WM_APP + 1,
};

/*
** -------------------------------------------------
** SECTION Globals
** -------------------------------------------------
*/

static BOOL W_SHOW_CONSOLE;

static HICON       W_EXECUTION_ICONS[W_EXECUTION_COUNT];
static W_execution W_CURRENT_EXECUTION = W_EXECUTION_NORMAL;

static const TCHAR W_RUN_KEY[]   = TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run");
static const TCHAR W_RUN_VALUE[] = TEXT("Wakelock");

/*
** -------------------------------------------------
** SECTION Macros
** -------------------------------------------------
*/

#define W_LOG(text)                                                                                   \
    (OutputDebugString(TEXT(text)),                                                                   \
     WriteConsole(GetStdHandle(STD_ERROR_HANDLE), TEXT(text), ARRAYSIZE(TEXT(text)) - 1, NULL, NULL))

/*
** -------------------------------------------------
** SECTION Prototypes
** -------------------------------------------------
*/

static LRESULT CALLBACK W_Window_Proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
static void             W_Window_SetMode(HWND hwnd, W_execution execution);
static void             W_Window_SetNotifyTip(NOTIFYICONDATA *notify, W_execution execution);
static void             W_AllowDarkMode(HMODULE theme);
static W_argument       W_ParseArguments(LPCTSTR p);
static DWORD            W_Startup_Set(W_execution execution);
static DWORD            W_Startup_Remove(void);
static LSTATUS          W_Startup_Query(W_execution *execution);
static BOOL             W_Execution_Set(W_execution execution);

/*
** -------------------------------------------------
** SECTION Entry
** -------------------------------------------------
*/

int main(void)
{
    DWORD e;

    W_argument arg = W_ParseArguments(GetCommandLine());

    switch (arg)
    {
    case W_ARGUMENT_NONE:
    case W_ARGUMENT_DISPLAY    : W_Execution_Set(W_EXECUTION_DISPLAY_REQUIRED); break;
    case W_ARGUMENT_ALLOW_SLEEP: W_CURRENT_EXECUTION = W_EXECUTION_NORMAL; break;
    case W_ARGUMENT_NO_SLEEP   : W_Execution_Set(W_EXECUTION_SYSTEM_REQUIRED); break;
    case W_ARGUMENT_ERROR      : W_LOG("Bad arguments provided to the commandline.\n"); return ERROR_INVALID_COMMAND_LINE;
    }

    HINSTANCE hInstance = GetModuleHandle(NULL);

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    for (size_t ii = 0; ii < ARRAYSIZE(W_EXECUTION_ICONS); ii++)
    {
        W_EXECUTION_ICONS[ii] = LoadIcon(hInstance, MAKEINTRESOURCE(ii + 100));

        if (W_EXECUTION_ICONS[ii] == NULL)
            return e = GetLastError(), W_LOG("LoadIcon failed.\n"), e;
    }

    WNDCLASS wc = {
        .lpfnWndProc = W_Window_Proc,
        .hInstance   = hInstance,
        .hIcon       = W_EXECUTION_ICONS[W_EXECUTION_DISPLAY_REQUIRED], /* Use bright screen
                                                                           for task manager
                                                                           icon. */
        .lpszClassName = TEXT("Wakelock-MSG"),
    };

    if (!RegisterClass(&wc))
        return e = GetLastError(), W_LOG("RegisterClass failed.\n"), e;

    HMODULE hTheme = LoadLibraryW(L"uxtheme.dll");

    if (hTheme)
        W_AllowDarkMode(hTheme);

    HWND hWnd = CreateWindowEx(0, wc.lpszClassName, NULL, WS_OVERLAPPED, 0, 0, 0, 0, /* HWND_MESSAGE */ NULL, NULL, hInstance, NULL);

    if (hWnd == NULL)
        return e = GetLastError(), W_LOG("CreateWindowExW failed.\n"), e;

    if (hTheme)
        FreeLibrary(hTheme);

    BOOL bRet;
    MSG  Msg = {0};

    while ((bRet = GetMessage(&Msg, NULL, 0, 0)) != 0)
    {
        if (bRet == -1)
        {
            e = GetLastError();
            W_LOG("GetMessage failed.\n");
            DestroyWindow(hWnd);
            return e;
        }
        else
        {
            TranslateMessage(&Msg);
            DispatchMessage(&Msg);
        }
    }
}

int __stdcall W_CRT_Entry(void)
{
    __security_init_cookie();
    ExitProcess(main());
}

/*
** -------------------------------------------------
** SECTION Window
** -------------------------------------------------
*/

static LRESULT CALLBACK W_Window_Proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (Msg == W_MESSAGE_ID_TRAY)
    {
        if (lParam == WM_LBUTTONUP)
        {
            W_Window_SetMode(hWnd, (W_CURRENT_EXECUTION + 1) % W_EXECUTION_COUNT);
            return 0;
        }
        else if (lParam == WM_RBUTTONUP)
        {
            HMENU menu = CreatePopupMenu();

            if (!menu)
                return W_LOG("CreatePopupMenu failed.\n"), 0;

            HMENU startup = CreatePopupMenu();
            if (!startup)
            {
                W_LOG("CreatePopupMenu failed.\n");
                DestroyMenu(menu);
                return 0;
            }

            AppendMenu(
                menu,
                MF_STRING | (W_SHOW_CONSOLE ? MF_CHECKED : MF_UNCHECKED),
                W_MENU_ID_SHOW_LOGS,
                TEXT("Show console")
            );

            AppendMenu(menu, MF_SEPARATOR, 0, NULL);

            AppendMenu(menu, MF_STRING, W_MENU_ID_ALLOW_SLEEP, TEXT("Allow sleep"));
            AppendMenu(menu, MF_STRING, W_MENU_ID_NO_SLEEP, TEXT("Prevent sleep"));
            AppendMenu(menu, MF_STRING, W_MENU_ID_KEEP_DISPLAY_ON, TEXT("Keep display on"));

            CheckMenuRadioItem(
                menu,
                W_MENU_ID_KEEP_DISPLAY_ON,
                W_MENU_ID_ALLOW_SLEEP,
                W_MENU_ID_KEEP_DISPLAY_ON + W_CURRENT_EXECUTION,
                MF_BYCOMMAND
            );

            AppendMenu(startup, MF_STRING, W_MENU_ID_STARTUP_ALLOW_SLEEP, TEXT("Allow sleep"));
            AppendMenu(startup, MF_STRING, W_MENU_ID_STARTUP_NO_SLEEP, TEXT("Prevent sleep"));
            AppendMenu(startup, MF_STRING, W_MENU_ID_STARTUP_DISPLAY, TEXT("Keep display on"));
            AppendMenu(startup, MF_STRING, W_MENU_ID_STARTUP_REMOVE, TEXT("Remove startup"));

            W_execution registered;

            if (!W_Startup_Query(&registered))
            {
                CheckMenuRadioItem(
                    startup,
                    W_MENU_ID_STARTUP_DISPLAY,
                    W_MENU_ID_STARTUP_REMOVE,
                    W_MENU_ID_STARTUP_DISPLAY + registered,
                    MF_BYCOMMAND
                );
            }

            AppendMenu(menu, MF_SEPARATOR, 0, NULL);
            AppendMenu(menu, MF_POPUP, (UINT_PTR)startup, TEXT("Run at startup"));
            AppendMenu(menu, MF_SEPARATOR, 0, NULL);
            AppendMenu(menu, MF_STRING, W_MENU_ID_EXIT, TEXT("Exit"));

            POINT point;

            if (!GetCursorPos(&point))
            {
                W_LOG("GetCursorPos failed.\n");
                DestroyMenu(menu);
                return 0;
            }

            SetForegroundWindow(hWnd);

            TrackPopupMenu(menu, TPM_RIGHTBUTTON, point.x, point.y, 0, hWnd, NULL);
            PostMessage(hWnd, WM_NULL, 0, 0);

            DestroyMenu(menu);

            return 0;
        }
    }
    else if (Msg == WM_COMMAND)
    {
        switch (LOWORD(wParam))
        {
        case W_MENU_ID_ALLOW_SLEEP    : W_Window_SetMode(hWnd, W_EXECUTION_NORMAL); return 0;
        case W_MENU_ID_NO_SLEEP       : W_Window_SetMode(hWnd, W_EXECUTION_SYSTEM_REQUIRED); return 0;
        case W_MENU_ID_KEEP_DISPLAY_ON: W_Window_SetMode(hWnd, W_EXECUTION_DISPLAY_REQUIRED); return 0;
        case W_MENU_ID_EXIT           : DestroyWindow(hWnd); return 0;

        case W_MENU_ID_STARTUP_ALLOW_SLEEP: {
            W_Startup_Set(W_EXECUTION_NORMAL);
        }
            return 0;
        case W_MENU_ID_STARTUP_NO_SLEEP: {
            W_Startup_Set(W_EXECUTION_SYSTEM_REQUIRED);
        }
            return 0;
        case W_MENU_ID_STARTUP_DISPLAY: {
            W_Startup_Set(W_EXECUTION_DISPLAY_REQUIRED);
        }
            return 0;
        case W_MENU_ID_STARTUP_REMOVE: {
            W_Startup_Remove();
        }
            return 0;
        case W_MENU_ID_SHOW_LOGS: {
            if (W_SHOW_CONSOLE == FALSE)
            {
                if (AllocConsole())
                {
                    HWND console = GetConsoleWindow();

                    if (console != NULL)
                    {
                        HMENU menu = GetSystemMenu(console, FALSE);

                        if (menu != NULL)
                            DeleteMenu(menu, SC_CLOSE, MF_BYCOMMAND);
                    }

                    W_SHOW_CONSOLE = TRUE;
                }
            }
            else
            {
                if (FreeConsole())
                    W_SHOW_CONSOLE = FALSE;
            }
        }
            return 0;
        }
    }
    else if (Msg == WM_CREATE)
    {
        NOTIFYICONDATA notify = {
            .cbSize           = sizeof(notify),
            .hWnd             = hWnd,
            .uID              = 0,
            .uFlags           = NIF_ICON | NIF_TIP | NIF_SHOWTIP | NIF_MESSAGE,
            .hIcon            = W_EXECUTION_ICONS[W_CURRENT_EXECUTION],
            .uCallbackMessage = W_MESSAGE_ID_TRAY,
        };

        W_Window_SetNotifyTip(&notify, W_CURRENT_EXECUTION);

        if (!Shell_NotifyIcon(NIM_ADD, &notify))
        {
            W_LOG("Shell_NotifyIcon(NIM_ADD) failed.\n");
            return -1; /* We are in WM_CREATE */
        }

        notify.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIcon(NIM_SETVERSION, &notify);

        return 0;
    }
    else if (Msg == WM_DESTROY)
    {
        NOTIFYICONDATA notify = {
            .cbSize = sizeof(notify),
            .hWnd   = hWnd,
            .uID    = 0,
        };

        Shell_NotifyIcon(NIM_DELETE, &notify);
        SetThreadExecutionState(ES_CONTINUOUS);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, Msg, wParam, lParam);
}

static void W_Window_SetMode(HWND hWnd, W_execution execution)
{
    BOOL change_execution = TRUE;

restore:
    NOTIFYICONDATA notify = {
        .cbSize           = sizeof(notify),
        .hWnd             = hWnd,
        .uID              = 0,
        .uFlags           = NIF_ICON | NIF_TIP | NIF_SHOWTIP | NIF_MESSAGE,
        .hIcon            = W_EXECUTION_ICONS[execution],
        .uCallbackMessage = W_MESSAGE_ID_TRAY,
    };

    W_Window_SetNotifyTip(&notify, execution);

    if (!Shell_NotifyIcon(NIM_MODIFY, &notify))
    {
        if (change_execution)
            W_LOG("Shell_NotifyIcon failed. No actions are taken.\n");
        else
            MessageBox(
                hWnd,
                TEXT("The displayed state might be inconsistent."),
                TEXT("Execution State"),
                MB_OK | MB_ICONWARNING
            );

        return;
    }

    if (change_execution)
    {
        change_execution = FALSE;

        if (!W_Execution_Set(execution))
        {
            W_LOG("Restoring ui to previous mode.\n");
            execution = W_CURRENT_EXECUTION;
            goto restore;
        }
    }
}

static void W_Window_SetNotifyTip(NOTIFYICONDATA *notify, W_execution execution)
{
    switch ((int)execution)
    {
    case W_EXECUTION_NORMAL: {
        memcpy(notify->szTip, TEXT("Sleep is allowed."), sizeof(TEXT("Sleep is allowed.")));
    }
    break;

    case W_EXECUTION_SYSTEM_REQUIRED: {
        memcpy(notify->szTip, TEXT("System won't sleep."), sizeof(TEXT("System won't sleep.")));
    }
    break;

    case W_EXECUTION_DISPLAY_REQUIRED: {
        memcpy(notify->szTip, TEXT("Screen won't sleep."), sizeof(TEXT("Screen won't sleep.")));
    }
    break;
    }
}

/*
** -------------------------------------------------
** SECTION Execution
** -------------------------------------------------
*/

static BOOL W_Execution_Set(W_execution execution)
{
    static const DWORD flags[3] = {
        ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED | ES_CONTINUOUS,
        ES_SYSTEM_REQUIRED | ES_CONTINUOUS,
        ES_CONTINUOUS,
    };

    if (execution >= W_EXECUTION_COUNT)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        W_LOG("Invalid execution state.\n");
        return FALSE;
    }

    BOOL ok = SetThreadExecutionState(flags[execution]);

    if (ok)
    {
        W_CURRENT_EXECUTION = execution;

        switch ((int)execution)
        {
        case W_EXECUTION_NORMAL: W_LOG("Execution state set to normal.\n"); break;

        case W_EXECUTION_SYSTEM_REQUIRED: W_LOG("Execution state set to system required.\n"); break;

        case W_EXECUTION_DISPLAY_REQUIRED: W_LOG("Execution state set to display required.\n"); break;
        }
    }
    else
    {
        switch ((int)execution)
        {
        case W_EXECUTION_NORMAL: W_LOG("Failed to set execution state to normal.\n"); break;

        case W_EXECUTION_SYSTEM_REQUIRED: W_LOG("Failed to set execution state to system required.\n"); break;

        case W_EXECUTION_DISPLAY_REQUIRED: W_LOG("Failed to set execution state to display required.\n"); break;
        }
    }

    return ok;
}

/*
** -------------------------------------------------
** SECTION Startup
** -------------------------------------------------
*/

static DWORD W_Startup_Set(W_execution execution)
{
    if (execution >= W_EXECUTION_COUNT)
        return ERROR_INVALID_PARAMETER;

    static const TCHAR *const W_EXECUTION_ARGUMENTS[W_EXECUTION_COUNT] = {
        TEXT("--display"),
        TEXT("--no-sleep"),
        TEXT("--allow-sleep"),
    };

    enum {
        W_EXECUTION_ARGUMENT_MAX = ARRAYSIZE(TEXT("--allow-sleep")) - 1,
        W_COMMAND_EXTRA          = 2 + 1 + W_EXECUTION_ARGUMENT_MAX + 1,
    };

    HANDLE heap = GetProcessHeap();

    if (heap == NULL)
        return GetLastError();

    HKEY key;
    LONG status = RegCreateKeyEx(HKEY_CURRENT_USER, W_RUN_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL);

    if (status != ERROR_SUCCESS)
        return (DWORD)status;

    DWORD capacity = 2 * MAX_PATH;

    TCHAR *command = HeapAlloc(heap, 0, (SIZE_T)(capacity + W_COMMAND_EXTRA) * sizeof(*command));

    if (!command)
    {
        RegCloseKey(key);
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    DWORD length;

    for (;;)
    {
        length = GetModuleFileName(NULL, command + 1, capacity);

        if (length == 0)
        {
            status = GetLastError();
            HeapFree(heap, 0, command);
            RegCloseKey(key);
            return (DWORD)status;
        }

        if (length < capacity)
            break;

        if (capacity > MAXDWORD / 2)
        {
            HeapFree(heap, 0, command);
            RegCloseKey(key);
            return ERROR_INSUFFICIENT_BUFFER;
        }

        capacity *= 2;

        TCHAR *tmp = command;
        command    = HeapReAlloc(heap, 0, command, (SIZE_T)(capacity + W_COMMAND_EXTRA) * sizeof(*command));

        if (!command)
        {
            HeapFree(heap, 0, tmp);
            RegCloseKey(key);
            return ERROR_NOT_ENOUGH_MEMORY;
        }
    }

    command[0]          = TEXT('"');
    command[length + 1] = TEXT('"');

    const TCHAR *argument = W_EXECUTION_ARGUMENTS[execution];
    TCHAR       *p        = command + length + 2;

    *p++ = TEXT(' ');

    while ((*p++ = *argument++))
        ;

    status = RegSetValueEx(key, W_RUN_VALUE, 0, REG_SZ, (const BYTE *)command, (DWORD)((p - command) * sizeof(*command)));

    HeapFree(heap, 0, command);
    RegCloseKey(key);

    return (DWORD)status;
}

static DWORD W_Startup_Remove(void)
{
    HKEY key;
    LONG status = RegOpenKeyEx(HKEY_CURRENT_USER, W_RUN_KEY, 0, KEY_SET_VALUE, &key);

    if (status != ERROR_SUCCESS)
        return (DWORD)status;

    status = RegDeleteValue(key, W_RUN_VALUE);

    if (status == ERROR_FILE_NOT_FOUND)
        status = ERROR_SUCCESS;

    RegCloseKey(key);

    return (DWORD)status;
}

static LSTATUS W_Startup_Query(W_execution *execution)
{
    HANDLE heap = GetProcessHeap();

    if (heap == NULL)
        return GetLastError();

    *execution = W_EXECUTION_COUNT;

    HKEY    key;
    LSTATUS status = RegOpenKeyEx(HKEY_CURRENT_USER, W_RUN_KEY, 0, KEY_QUERY_VALUE, &key);

    if (status == ERROR_FILE_NOT_FOUND)
        return ERROR_SUCCESS;

    if (status != ERROR_SUCCESS)
        return status;

    DWORD type = 0;
    DWORD size = 0;

    status = RegQueryValueEx(key, W_RUN_VALUE, NULL, &type, NULL, &size);

    if (status == ERROR_FILE_NOT_FOUND)
    {
        RegCloseKey(key);
        return ERROR_SUCCESS;
    }

    if (status != ERROR_SUCCESS)
    {
        RegCloseKey(key);
        return status;
    }

    if (type != REG_SZ)
    {
        RegCloseKey(key);
        return ERROR_DATATYPE_MISMATCH;
    }

    TCHAR *value = HeapAlloc(heap, 0, size + sizeof(TCHAR));

    if (value == NULL)
    {
        RegCloseKey(key);
        return ERROR_OUTOFMEMORY;
    }

    DWORD data_size = size;

    status = RegQueryValueEx(key, W_RUN_VALUE, NULL, &type, (LPBYTE)value, &data_size);

    if (status != ERROR_SUCCESS)
    {
        HeapFree(heap, 0, value);
        RegCloseKey(key);
        return status;
    }

    value[data_size / sizeof(*value)] = TEXT('\0');

    switch (W_ParseArguments(value))
    {
    case W_ARGUMENT_ALLOW_SLEEP: *execution = W_EXECUTION_NORMAL; break;

    case W_ARGUMENT_NO_SLEEP: *execution = W_EXECUTION_SYSTEM_REQUIRED; break;

    case W_ARGUMENT_DISPLAY: *execution = W_EXECUTION_DISPLAY_REQUIRED; break;

    case W_ARGUMENT_NONE:
    case W_ARGUMENT_ERROR:
    default              : status = ERROR_BAD_FORMAT; break;
    }

    HeapFree(heap, 0, value);
    RegCloseKey(key);

    return status;
}

/*
** -------------------------------------------------
** SECTION Theme
** -------------------------------------------------
*/

static void W_AllowDarkMode(HMODULE theme)
{
    typedef enum {
        Default,
        AllowDark,
        ForceDark,
        ForceLight,
        Max,
    } PreferredAppMode;

    typedef PreferredAppMode(WINAPI * SetPreferredAppModeFn)(PreferredAppMode mode);

    SetPreferredAppModeFn SetPreferredAppMode = (SetPreferredAppModeFn)GetProcAddress(theme, MAKEINTRESOURCEA(135));

    if (SetPreferredAppMode)
        SetPreferredAppMode(AllowDark);
}

/*
** -------------------------------------------------
** SECTION Arguments
** -------------------------------------------------
*/

static W_argument W_ParseArguments(LPCTSTR p)
{
    /* Skip executable name. */
    if (*p == TEXT('"'))
    {
        ++p;

        while (*p && *p != TEXT('"'))
            ++p;

        if (*p == TEXT('"'))
            ++p;
    }
    else
    {
        while (*p && *p != TEXT(' ') && *p != TEXT('\t'))
            ++p;
    }

    /* Skip whitespace after executable name. */
    while (*p == TEXT(' ') || *p == TEXT('\t'))
        ++p;

    if (*p == TEXT('\0'))
        return W_ARGUMENT_NONE;

    if (*p != TEXT('-'))
        return W_ARGUMENT_ERROR;

    ++p;

    if (*p == TEXT('-'))
        ++p;

    if (_tcscmp(p, TEXT("allow-sleep")) == 0)
        return W_ARGUMENT_ALLOW_SLEEP;

    if (_tcscmp(p, TEXT("no-sleep")) == 0)
        return W_ARGUMENT_NO_SLEEP;

    if (_tcscmp(p, TEXT("display")) == 0)
        return W_ARGUMENT_DISPLAY;

    return W_ARGUMENT_ERROR;
}

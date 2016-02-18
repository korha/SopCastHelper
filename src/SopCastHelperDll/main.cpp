#include <windows.h>
#include <commctrl.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <cassert>

#define EXPORT //__declspec(dllexport)

static const wchar_t *const g_wGuidCaption = L"SopCast::da69e434-4c65-f223-64be-0cc07a9410da";

enum
{
    eBtnAppMuteId = 10005,
    eBtnAppExplayerId = 10008,
    eBtnAppAddressLabelId = 1052,
    eBtnAppAddressBoxId = 1005,
    eBtnAppRunId = 1035,

    eBtnCollapse = 5000,
    eBtnExpand,
    eBtnMute,
    eBtnUnMute,
    eBtnLock,
    eBtnStream
};

static wchar_t g_wPath[MAX_PATH*3];
static SOCKADDR_IN g_sockAddrIn;
static SOCKET g_sockId = INVALID_SOCKET;
static u_short g_iPort;
static DWORD g_dwCurrentThreadId;
static HWND g_hWndBtnLock, g_hWndBtnAppExplayer, g_hWndBtnAppMute;
static PROCESS_INFORMATION g_Pi;
static STARTUPINFO g_Si;
static HFONT g_hFont;
static LONG g_iCx, g_iCy;

//-------------------------------------------------------------------------------------------------
void fSetMute(const WINBOOL bMute)
{
    //COM already initialized
    IMMDeviceEnumerator *immDeviceEnumerator;
    if (CoCreateInstance(__uuidof(MMDeviceEnumerator), 0, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<LPVOID*>(&immDeviceEnumerator)) == S_OK)
    {
        IMMDevice *immDeviceDefault;
        HRESULT hr = immDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &immDeviceDefault);
        immDeviceEnumerator->Release();
        if (hr == S_OK)
        {
            IAudioSessionManager *iAudioSessionManager;
            hr = immDeviceDefault->Activate(__uuidof(IAudioSessionManager), CLSCTX_ALL, 0, reinterpret_cast<void**>(&iAudioSessionManager));
            immDeviceDefault->Release();
            if (hr == S_OK)
            {
                ISimpleAudioVolume *iSimpleAudioVolume;
                hr = iAudioSessionManager->GetSimpleAudioVolume(0, FALSE, &iSimpleAudioVolume);
                iAudioSessionManager->Release();
                if (hr == S_OK)
                {
                    iSimpleAudioVolume->SetMute(bMute, 0);
                    iSimpleAudioVolume->Release();
                }
            }
        }
    }
}

//-------------------------------------------------------------------------------------------------
void fLock()
{
    if (IsWindowEnabled(g_hWndBtnAppExplayer))
    {
        PostMessage(g_hWndBtnAppMute, BM_CLICK, 0, 0);        //hack
        PostMessage(g_hWndBtnAppExplayer, BM_CLICK, 0, 0);
        PostMessage(g_hWndBtnAppMute, BM_CLICK, 0, 0);        //hack
    }
    else
        MessageBox(0, L"Run broadcast!", L"SopCastHelper", MB_ICONWARNING);
}

//-------------------------------------------------------------------------------------------------
LRESULT CALLBACK WindowProcSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        switch (wParam)
        {
        case eBtnCollapse:
            SetWindowPos(hWnd, HWND_TOP, 0, 0, 237, 70, SWP_NOMOVE | SWP_NOZORDER);
            break;
        case eBtnExpand:
            SetWindowPos(hWnd, HWND_TOP, 0, 0, g_iCx, g_iCy, SWP_NOMOVE | SWP_NOZORDER);
            break;
        case eBtnMute:
            fSetMute(TRUE);
            break;
        case eBtnUnMute:
            fSetMute(FALSE);
            break;
        case eBtnLock:
            if (g_sockId == INVALID_SOCKET)
                fLock();
            else        //Unlock
            {
                closesocket(g_sockId);
                g_sockId = INVALID_SOCKET;
                g_iPort = 0;
                SetWindowText(g_hWndBtnLock, L"Lock");
            }
            break;
        case eBtnStream:
            if (g_iPort)
            {
                const HANDLE hFile = CreateFile(g_wPath+MAX_PATH*2, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
                if (hFile != INVALID_HANDLE_VALUE)
                {
                    DWORD dwBytes;
                    LARGE_INTEGER iFileSize;
                    if (GetFileSizeEx(hFile, &iFileSize) &&
                            iFileSize.HighPart == 0 &&
                            iFileSize.LowPart%sizeof(wchar_t) == 0 &&
                            iFileSize.LowPart &&
                            iFileSize.LowPart/sizeof(wchar_t) < MAX_PATH*2-25/* http://127.0.0.1:65535/`*/)
                        dwBytes = ReadFile(hFile, g_wPath, iFileSize.LowPart, &dwBytes, 0) && dwBytes == iFileSize.LowPart;
                    CloseHandle(hFile);

                    if (dwBytes)
                    {
                        wchar_t *pDelim = g_wPath+iFileSize.LowPart/sizeof(wchar_t);
                        wcscpy(pDelim, L" http://127.0.0.1:");
                        pDelim += 18;
                        _ultow(g_iPort, pDelim, 10);
                        wcscat(pDelim, L"/");
                        if (CreateProcess(0, g_wPath, 0, 0, FALSE, CREATE_UNICODE_ENVIRONMENT, 0, 0, &g_Si, &g_Pi))
                        {
                            CloseHandle(g_Pi.hThread);
                            CloseHandle(g_Pi.hProcess);
                        }
                    }
                }
            }
            else
                fLock();
            break;
        default: return DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }
        return 0;
    }
    case WM_COPYDATA:
    {
        const COPYDATASTRUCT *const pCopyData = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
        const ULONG_PTR iPort = pCopyData->dwData;
        if (pCopyData->cbData == 0 && pCopyData->lpData == 0 && iPort > 0 && iPort <= 65535)
        {
            g_iPort = 0;
            if (g_sockId != INVALID_SOCKET)
                closesocket(g_sockId);
            g_sockId = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (g_sockId != INVALID_SOCKET)
            {
                g_sockAddrIn.sin_port = (iPort & 0xFF) << 8 | iPort >> 8;        //to network byte order
                if (connect(g_sockId, static_cast<const SOCKADDR*>(static_cast<const void*>(&g_sockAddrIn)), sizeof(SOCKADDR_IN)) == NO_ERROR)
                {
                    g_iPort = iPort;
                    SetWindowText(g_hWndBtnLock, L"Unlock");
                }
                else
                {
                    closesocket(g_sockId);
                    g_sockId = INVALID_SOCKET;
                }
            }
        }
        return TRUE;
    }
    case WM_NCDESTROY:
    {
        RemoveWindowSubclass(hWnd, WindowProcSubclass, uIdSubclass);
        if (g_sockId != INVALID_SOCKET)
            closesocket(g_sockId);
        DeleteObject(g_hFont);
        break;
    }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

//-------------------------------------------------------------------------------------------------
VOID CALLBACK TimerProc(HWND, UINT, UINT_PTR idEvent, DWORD)
{
    HWND hWnd = 0;
    while ((hWnd = FindWindowEx(0, hWnd, L"#32770", 0)))
        if (GetWindowThreadProcessId(hWnd, 0) == g_dwCurrentThreadId &&
                GetWindowText(hWnd, g_wPath, 17/*SopCast - Login`?*/) > 10 &&
                wcscmp(g_wPath, L"SopCast - Login") != 0)
        {
            if (KillTimer(0, idEvent))
            {
                HWND hWndTemp = 0;
                while ((hWndTemp = FindWindowEx(0, hWndTemp, L"#32770", L"")))
                    if (GetWindowThreadProcessId(hWndTemp, 0) == g_dwCurrentThreadId)
                    {
                        if ((hWndTemp = FindWindowEx(hWndTemp, 0, L"AfxOleControl70su", L"")) &&
                                (hWndTemp = FindWindowEx(hWndTemp, 0, L"#32770", L"")) &&
                                ((g_hWndBtnAppMute = GetDlgItem(hWndTemp, eBtnAppMuteId))))
                            g_hWndBtnAppExplayer = GetDlgItem(hWndTemp, eBtnAppExplayerId);
                        break;
                    }

                if (g_hWndBtnAppExplayer &&
                        (hWndTemp = GetDlgItem(hWnd, eBtnAppAddressLabelId)) && ShowWindow(hWndTemp, SW_HIDE) &&
                        (hWndTemp = GetDlgItem(hWnd, eBtnAppAddressBoxId)) && SetWindowPos(hWndTemp, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER) &&
                        (hWndTemp = GetDlgItem(hWnd, eBtnAppRunId)) && SetWindowPos(hWndTemp, HWND_TOP, 177, 22, 53, 22, SWP_NOZORDER))
                    if (const HINSTANCE hInst = GetModuleHandle(0))
                        if (const HWND hWndBtnCollapse = CreateWindowEx(0, WC_BUTTON, L"-", WS_CHILD | WS_VISIBLE, 1, 22, 16, 22, hWnd, reinterpret_cast<HMENU>(eBtnCollapse), hInst, 0))
                            if (const HWND hWndBtnExpand = CreateWindowEx(0, WC_BUTTON, L"+", WS_CHILD | WS_VISIBLE, 18, 22, 16, 22, hWnd, reinterpret_cast<HMENU>(eBtnExpand), hInst, 0))
                            {
                                const bool bIsVistaOrHigher = (GetVersion() & 0xFF) >= 6;        //NT6+
                                HWND hWndBtnMute = 0, hWndBtnUnMute = 0;
                                if (!bIsVistaOrHigher ||
                                        ((hWndBtnMute = CreateWindowEx(0, WC_BUTTON, L"M", WS_CHILD | WS_VISIBLE, 35, 22, 16, 22, hWnd, reinterpret_cast<HMENU>(eBtnMute), hInst, 0)) &&
                                         (hWndBtnUnMute = CreateWindowEx(0, WC_BUTTON, L"U", WS_CHILD | WS_VISIBLE, 52, 22, 16, 22, hWnd, reinterpret_cast<HMENU>(eBtnUnMute), hInst, 0))))
                                    if ((g_hWndBtnLock = CreateWindowEx(0, WC_BUTTON, L"Lock", WS_CHILD | WS_VISIBLE, 69, 22, 53, 22, hWnd, reinterpret_cast<HMENU>(eBtnLock), hInst, 0)))
                                        if (const HWND hWndBtnStream = CreateWindowEx(0, WC_BUTTON, L"Stream", WS_CHILD | WS_VISIBLE, 123, 22, 53, 22, hWnd, reinterpret_cast<HMENU>(eBtnStream), hInst, 0))
                                        {
                                            RECT rect;
                                            if (GetWindowRect(hWnd, &rect) &&
                                                    SetWindowPos(hWnd, HWND_TOP, 0, 0, 237, 70, SWP_NOMOVE | SWP_NOZORDER) &&
                                                    SetWindowText(hWnd, g_wGuidCaption))
                                            {
                                                NONCLIENTMETRICS nonClientMetrics;
                                                nonClientMetrics.cbSize = sizeof(NONCLIENTMETRICS);
                                                if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &nonClientMetrics, 0) &&
                                                        (g_hFont = CreateFontIndirect(&nonClientMetrics.lfMessageFont)))
                                                {
                                                    SendMessage(hWndBtnCollapse, WM_SETFONT, reinterpret_cast<WPARAM>(g_hFont), FALSE);
                                                    SendMessage(hWndBtnExpand, WM_SETFONT, reinterpret_cast<WPARAM>(g_hFont), FALSE);
                                                    if (bIsVistaOrHigher)
                                                    {
                                                        SendMessage(hWndBtnMute, WM_SETFONT, reinterpret_cast<WPARAM>(g_hFont), FALSE);
                                                        SendMessage(hWndBtnUnMute, WM_SETFONT, reinterpret_cast<WPARAM>(g_hFont), FALSE);
                                                    }
                                                    SendMessage(g_hWndBtnLock, WM_SETFONT, reinterpret_cast<WPARAM>(g_hFont), FALSE);
                                                    SendMessage(hWndBtnStream, WM_SETFONT, reinterpret_cast<WPARAM>(g_hFont), FALSE);

                                                    if (SetWindowSubclass(hWnd, WindowProcSubclass, 1, 0))
                                                    {
                                                        g_sockAddrIn.sin_family = AF_INET;
                                                        g_sockAddrIn.sin_addr.S_un.S_addr = 127 | 0 << 8 | 0 << 16 | 1 << 24;        //localhost
                                                        assert(g_sockAddrIn.sin_addr.S_un.S_un_b.s_b1 == 127 && g_sockAddrIn.sin_addr.S_un.S_un_b.s_b2 == 0 &&
                                                               g_sockAddrIn.sin_addr.S_un.S_un_b.s_b3 == 0 && g_sockAddrIn.sin_addr.S_un.S_un_b.s_b4 == 1);
                                                        g_iCx = rect.right-rect.left;
                                                        g_iCy = rect.bottom-rect.top;
                                                        g_Si.cb = sizeof(STARTUPINFO);
                                                    }
                                                    else
                                                        DeleteObject(g_hFont);
                                                }
                                            }
                                        }
                            }
            }
            break;
        }
}

//-------------------------------------------------------------------------------------------------
EXPORT WINBOOL WINAPI MiniDumpWriteDumpStub(HANDLE, DWORD, HANDLE, int, CONST PVOID, CONST PVOID, CONST PVOID)
{
    return FALSE;
}

//-------------------------------------------------------------------------------------------------
BOOL WINAPI DllMain(HINSTANCE, DWORD fdwReason, LPVOID)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        assert(htons(54321) == ((54321 & 0xFF) << 8 | 54321 >> 8));
        if ((g_dwCurrentThreadId = GetCurrentThreadId()))
        {
            const DWORD dwLen = GetModuleFileName(0, g_wPath+MAX_PATH*2, MAX_PATH-4/*.ini*/+1);
            if (dwLen >= 4 && dwLen < MAX_PATH-4/*.ini*/ && SetTimer(0, 1, 1000, TimerProc))
            {
                wcscpy(g_wPath+MAX_PATH*2+dwLen, L".ini");
                return TRUE;
            }
        }
    }
    return FALSE;
}

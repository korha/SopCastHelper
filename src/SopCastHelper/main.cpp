#include <windows.h>
#include <tlhelp32.h>

static const wchar_t *const g_wGuidCaption = L"SopCast::da69e434-4c65-f223-64be-0cc07a9410da";

int main()
{
    if (wchar_t *wCmdLine = GetCommandLine())
    {
        while (*wCmdLine == L' ' || *wCmdLine == L'\t')
            ++wCmdLine;
        if (*wCmdLine != L'\0')
        {
            //1st
            if (*wCmdLine++ == L'"')
            {
                while (*wCmdLine != L'\"')
                {
                    if (*wCmdLine == L'\0')
                        return 0;
                    ++wCmdLine;
                }
                ++wCmdLine;
                if (*wCmdLine != L' ' && *wCmdLine != L'\t')
                    return 0;
            }
            else
                while (*wCmdLine != L' ' && *wCmdLine != L'\t')
                {
                    if (*wCmdLine == L'\0' || *wCmdLine == L'\"')
                        return 0;
                    ++wCmdLine;
                }

            //2nd
            do {++wCmdLine;}
            while (*wCmdLine == L' ' || *wCmdLine == L'\t');
            if (*wCmdLine != L'\0')
            {
                wchar_t *wArg = wCmdLine;
                if (*wCmdLine++ == L'"')
                {
                    while (*wCmdLine != L'\"')
                    {
                        if (*wCmdLine == L'\0')
                            return 0;
                        ++wCmdLine;
                    }
                    if (wCmdLine[1] != L' ' && wCmdLine[1] != L'\t' && wCmdLine[1] != L'\0')
                        return 0;
                    ++wArg;
                }
                else
                    while (*wCmdLine != L' ' && *wCmdLine != L'\t' && *wCmdLine != L'\0')
                        ++wCmdLine;
                *wCmdLine = L'\0';

                if (wcsncmp(wArg, L"http://127.0.0.1:", 17) == 0)
                {
                    wArg += 17;
                    if (wchar_t *const pDelim = wcschr(wArg, L'/'))
                    {
                        *pDelim = L'\0';
                        wchar_t *wOk;
                        unsigned long iVal = wcstoul(wArg, &wOk, 10);
                        if (!(*wOk || errno) && iVal > 0 && iVal <= 65535)
                        {
                            DWORD dwPid = GetCurrentProcessId();
                            if (dwPid != ASFW_ANY)
                            {
                                const HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
                                if (hSnapshot != INVALID_HANDLE_VALUE)
                                {
                                    DWORD dwParentPid = ASFW_ANY;
                                    PROCESSENTRY32 processEntry32;
                                    processEntry32.dwSize = sizeof(PROCESSENTRY32);
                                    if (Process32First(hSnapshot, &processEntry32))
                                    {
                                        do
                                        {
                                            if (processEntry32.th32ProcessID == dwPid)
                                            {
                                                dwParentPid = processEntry32.th32ParentProcessID;
                                                break;
                                            }
                                        } while (Process32Next(hSnapshot, &processEntry32));
                                    }
                                    CloseHandle(hSnapshot);

                                    if (dwParentPid != ASFW_ANY)
                                    {
                                        HWND hWnd = 0;
                                        while ((hWnd = FindWindowEx(0, hWnd, L"#32770", g_wGuidCaption)))
                                        {
                                            dwPid = ASFW_ANY;
                                            if (GetWindowThreadProcessId(hWnd, &dwPid) && dwPid == dwParentPid)
                                            {
                                                COPYDATASTRUCT copyData;
                                                copyData.dwData = iVal;
                                                copyData.cbData = 0;
                                                copyData.lpData = 0;
                                                SendMessageTimeout(hWnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&copyData), SMTO_ABORTIFHUNG, 0, 0);
                                                break;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}

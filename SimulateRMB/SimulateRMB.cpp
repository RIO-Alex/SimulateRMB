#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <queue>
#include <mutex>

static constexpr WORD TARGET_XBUTTON = XBUTTON2;

static HHOOK g_hook = nullptr;
static HANDLE g_event = nullptr;
std::queue<int> g_queue;
std::mutex g_mutex;

static DWORD WINAPI WorkerThread(LPVOID)
{
    while (true)
    {
        WaitForSingleObject(g_event, INFINITE);

        int action = 0;

        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (!g_queue.empty()) {
                action = g_queue.front();
                g_queue.pop();
            }
        }

        if (action == 0) 
            break;

        INPUT input {};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = (action == 1) ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;

        if (SendInput(1, &input, sizeof(INPUT)) == 0)
            std::cerr << "SendInput failed: " << GetLastError() << '\n';

        std::cout << (action == 1 ? "[DOWN] -> RMB DOWN\n" : "[UP]   -> RMB UP\n");
    }
    return 0;
}

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        auto* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);

        bool bIsMyButton = (wParam == WM_XBUTTONDOWN || wParam == WM_XBUTTONUP) && (HIWORD(ms->mouseData) == TARGET_XBUTTON);

        if (bIsMyButton)
        {
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                g_queue.push(wParam == WM_XBUTTONDOWN ? 1 : -1);
            }

            SetEvent(g_event);
            return 1;
        }
    }

    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

int main()
{
    std::cout << "Mouse side-button -> RMB remapper\n";
    std::cout << "Press Ctrl+C to exit.\n\n";

    g_hook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, nullptr, 0);
    if (!g_hook)
    {
        std::cerr << "SetWindowsHookEx failed: " << GetLastError() << '\n';
        return 1;
    }

    g_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!g_event)
    {
        std::cerr << "CreateEvent failed: " << GetLastError() << '\n';
        return 1;
    }

    HANDLE thread = CreateThread(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
    
    if (!thread)
    {
        std::cerr << "CreateThread failed: " << GetLastError() << '\n';
        CloseHandle(g_event);
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    SetEvent(g_event);
    WaitForSingleObject(thread, INFINITE);

    UnhookWindowsHookEx(g_hook);
    CloseHandle(thread);
    CloseHandle(g_event);

    return 0;
}

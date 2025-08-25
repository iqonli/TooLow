#include <windows.h>
#include <commctrl.h>  // 包含轨迹栏控件相关定义
#include <string.h>
#include <bits/stdc++.h>

// 注意：使用MinGW编译时，需要在编译命令中添加 -lcomctl32 参数来链接comctl32库
// 编译命令示例：g++ TooLowGUI.cpp -o TooLowGUI.exe -lcomctl32
using namespace std;

const char* MAIN_WINDOW_TITLE = "太Low - 屏幕颜色遮罩 v2.0";
const char* MASK_WINDOW_TITLE = "屏幕遮罩";

// 全局变量
HWND hMaskWnd;         // 遮罩窗口句柄
HWND hMainWnd;         // 主窗口句柄
HWND hSliderR, hSliderG, hSliderB, hSliderA; // 滑块控件句柄
HWND hStaticR, hStaticG, hStaticB, hStaticA; // 显示当前值的静态文本
HWND hwndPreview;      // 颜色预览区域句柄

int r = 255, g = 255, b = 255;      // RGB颜色值
int alpha = 0;                // 透明度值(0-100)

// 数值范围限制函数
int clamp(int val, int min, int max)
{
    return val < min ? min : (val > max ? max : val);
}

// 遮罩窗口消息处理函数
LRESULT CALLBACK MaskWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

// 更新遮罩窗口的颜色和透明度
void UpdateMaskWindow()
{
    // 设置透明度和颜色
    SetLayeredWindowAttributes(hMaskWnd, 0, (BYTE)(alpha * 2.55f), LWA_ALPHA);

    // 更新窗口背景色
    HDC hdc = GetDC(hMaskWnd);
    HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
    RECT rect;
    GetClientRect(hMaskWnd, &rect);
    FillRect(hdc, &rect, hBrush);
    ReleaseDC(hMaskWnd, hdc);
    DeleteObject(hBrush);

    // 更新静态文本显示的值
    char buffer[20];
    sprintf(buffer, "R: %d", r);
    SetWindowText(hStaticR, buffer);
    sprintf(buffer, "G: %d", g);
    SetWindowText(hStaticG, buffer);
    sprintf(buffer, "B: %d", b);
    SetWindowText(hStaticB, buffer);
    sprintf(buffer, "A: %d%%", alpha);
    SetWindowText(hStaticA, buffer);

    // 更新颜色预览区域
    if (hwndPreview != NULL)
    {
        hdc = GetDC(hwndPreview);
        hBrush = CreateSolidBrush(RGB(r, g, b));
        GetClientRect(hwndPreview, &rect);
        FillRect(hdc, &rect, hBrush);
        ReleaseDC(hwndPreview, hdc);
        DeleteObject(hBrush);
    }
}

// 主窗口消息处理函数
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CREATE:
        {
            // 创建滑块控件
            hSliderR = CreateWindow(
                TRACKBAR_CLASS, "R",
                WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS | TBS_ENABLESELRANGE,
                50, 30, 200, 30,
                hwnd, (HMENU)1001, GetModuleHandle(NULL), NULL
            );
            SendMessage(hSliderR, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
            SendMessage(hSliderR, TBM_SETPOS, TRUE, 0);

            hSliderG = CreateWindow(
                TRACKBAR_CLASS, "G",
                WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS | TBS_ENABLESELRANGE,
                50, 70, 200, 30,
                hwnd, (HMENU)1002, GetModuleHandle(NULL), NULL
            );
            SendMessage(hSliderG, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
            SendMessage(hSliderG, TBM_SETPOS, TRUE, 0);

            hSliderB = CreateWindow(
                TRACKBAR_CLASS, "B",
                WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS | TBS_ENABLESELRANGE,
                50, 110, 200, 30,
                hwnd, (HMENU)1003, GetModuleHandle(NULL), NULL
            );
            SendMessage(hSliderB, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
            SendMessage(hSliderB, TBM_SETPOS, TRUE, 0);

            hSliderA = CreateWindow(
                TRACKBAR_CLASS, "Alpha",
                WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS | TBS_ENABLESELRANGE,
                50, 150, 200, 30,
                hwnd, (HMENU)1004, GetModuleHandle(NULL), NULL
            );
            SendMessage(hSliderA, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
            SendMessage(hSliderA, TBM_SETPOS, TRUE, 0);

            // 创建静态文本
            hStaticR = CreateWindow(
                "STATIC", "R: 0",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                260, 30, 80, 20,
                hwnd, NULL, GetModuleHandle(NULL), NULL
            );

            hStaticG = CreateWindow(
                "STATIC", "G: 0",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                260, 70, 80, 20,
                hwnd, NULL, GetModuleHandle(NULL), NULL
            );

            hStaticB = CreateWindow(
                "STATIC", "B: 0",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                260, 110, 80, 20,
                hwnd, NULL, GetModuleHandle(NULL), NULL
            );

            hStaticA = CreateWindow(
                "STATIC", "透明度: 0%",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                260, 150, 80, 20,
                hwnd, NULL, GetModuleHandle(NULL), NULL
            );

            // 创建颜色预览区域
            hwndPreview = CreateWindow(
                "STATIC", "",
                WS_CHILD | WS_VISIBLE | SS_WHITERECT,
                100, 190, 200, 60,
                hwnd, NULL, GetModuleHandle(NULL), NULL
            );

            return 0;
        }

        case WM_HSCROLL:
        {
            // 处理滑块事件
            if (lParam == (LPARAM)hSliderR)
            {
                r = SendMessage(hSliderR, TBM_GETPOS, 0, 0);
                r = clamp(r, 0, 255);
            }
            else if (lParam == (LPARAM)hSliderG)
            {
                g = SendMessage(hSliderG, TBM_GETPOS, 0, 0);
                g = clamp(g, 0, 255);
            }
            else if (lParam == (LPARAM)hSliderB)
            {
                b = SendMessage(hSliderB, TBM_GETPOS, 0, 0);
                b = clamp(b, 0, 255);
            }
            else if (lParam == (LPARAM)hSliderA)
            {
                alpha = SendMessage(hSliderA, TBM_GETPOS, 0, 0);
                alpha = clamp(alpha, 0, 100);
            }

            // 更新显示的值
            UpdateMaskWindow();
            return 0;
        }

        // 删除了应用按钮，因此移除WM_COMMAND处理


        case WM_DESTROY:
            DestroyWindow(hMaskWnd);  // 销毁遮罩窗口
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 初始化通用控件
    InitCommonControls();

    // 注册遮罩窗口类
    WNDCLASSEX maskWc = { sizeof(WNDCLASSEX), CS_CLASSDC, MaskWindowProc, 0, 0, hInstance };
    maskWc.lpszClassName = "MaskWindowClass";
    RegisterClassEx(&maskWc);

    // 注册主窗口类
    WNDCLASSEX mainWc = { sizeof(WNDCLASSEX), CS_CLASSDC, MainWindowProc, 0, 0, hInstance };
    mainWc.lpszClassName = "MainWindowClass";
    RegisterClassEx(&mainWc);

    // 创建遮罩窗口（全屏、置顶、透明）
    RECT screenRect;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &screenRect, 0);
    hMaskWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        maskWc.lpszClassName, MASK_WINDOW_TITLE, WS_POPUP,
        screenRect.left, screenRect.top, screenRect.right - screenRect.left, screenRect.bottom - screenRect.top,
        NULL, NULL, hInstance, NULL
    );

    // 设置初始透明度为0（完全透明）
    SetLayeredWindowAttributes(hMaskWnd, 0, 0, LWA_ALPHA);
    ShowWindow(hMaskWnd, SW_SHOWNOACTIVATE);

    // 创建主窗口
    hMainWnd = CreateWindowEx(
        0,
        mainWc.lpszClassName, MAIN_WINDOW_TITLE, WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,  // 窗口大小
        NULL, NULL, hInstance, NULL
    );

    ShowWindow(hMainWnd, nCmdShow);
    UpdateWindow(hMainWnd);

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 清理资源
    UnregisterClass(maskWc.lpszClassName, hInstance);
    UnregisterClass(mainWc.lpszClassName, hInstance);
    return 0;
}

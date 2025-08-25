#include <windows.h>
#include <commctrl.h>  // 轨迹栏控件相关定义
#include <string.h>
#include <vector>
#include <bits/stdc++.h>

// 编译命令（MinGW）：g++ TooLowGUI.cpp -o TooLowGUI.exe -lcomctl32
using namespace std;

const char* MAIN_WINDOW_TITLE = "太Low - 屏幕颜色遮罩 v2.0";
const char* MASK_WINDOW_TITLE = "屏幕遮罩";

// 函数前向声明（全局作用域）
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData);
LRESULT CALLBACK MaskWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void UpdateMaskWindow();
int clamp(int val, int min, int max);

// 全局变量（所有函数可访问）
vector<HWND> hMaskWnds;       // 所有遮罩窗口句柄
HWND hMainWnd;                // 主窗口句柄
HWND hSliderR, hSliderG, hSliderB, hSliderA; // 滑块控件句柄
HWND hStaticR, hStaticG, hStaticB, hStaticA; // 数值显示静态文本
HWND hwndPreview;             // 颜色预览区域句柄
HWND hCheckTopmost;           // 完全置顶复选框
HWND hCheckMainOnly;          // 仅主显示器复选框

int r = 255, g = 255, b = 255; // RGB颜色值（初始白色）
int alpha = 0;                 // 透明度（0-100，初始完全透明）
bool isTopmost = true;         // 默认完全置顶
bool isMainOnly = false;       // 默认所有显示器生效

// 数值范围限制函数
int clamp(int val, int min, int max) {
	return val < min ? min : (val > max ? max : val);
}

// 遮罩窗口消息处理函数（全局函数）
LRESULT CALLBACK MaskWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

// 更新遮罩窗口颜色/透明度 + 控件显示（全局函数）
void UpdateMaskWindow() {
	// 1. 更新所有遮罩窗口
	for (HWND hMaskWnd : hMaskWnds) {
		// 设置透明度（alpha 0-100 → 0-255）
		SetLayeredWindowAttributes(hMaskWnd, 0, (BYTE)(alpha * 2.55f), LWA_ALPHA);
		
		// 绘制窗口背景色
		HDC hdc = GetDC(hMaskWnd);
		HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
		RECT rect;
		GetClientRect(hMaskWnd, &rect);
		FillRect(hdc, &rect, hBrush);
		ReleaseDC(hMaskWnd, hdc);
		DeleteObject(hBrush); // 释放画笔资源，避免内存泄漏
	}
	
	// 2. 更新数值显示文本
	char buffer[20];
	sprintf(buffer, "R: %d", r);
	SetWindowText(hStaticR, buffer);
	sprintf(buffer, "G: %d", g);
	SetWindowText(hStaticG, buffer);
	sprintf(buffer, "B: %d", b);
	SetWindowText(hStaticB, buffer);
	sprintf(buffer, "A: %d%%", alpha);
	SetWindowText(hStaticA, buffer);
	
	// 3. 更新颜色预览区域
	if (hwndPreview != NULL) {
		HDC hdc = GetDC(hwndPreview);
		HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
		RECT rect;
		GetClientRect(hwndPreview, &rect);
		FillRect(hdc, &rect, hBrush);
		ReleaseDC(hwndPreview, hdc);
		DeleteObject(hBrush);
	}
}

// 显示器枚举回调函数（全局函数，解决嵌套定义错误）
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	vector<HWND>* maskWnds = reinterpret_cast<vector<HWND>*>(dwData);
	HINSTANCE hInstance = GetModuleHandle(NULL);
	
	// 获取显示器信息（包含位置、是否为主显示器）
	MONITORINFOEX monitorInfo;
	monitorInfo.cbSize = sizeof(MONITORINFOEX);
	GetMonitorInfo(hMonitor, &monitorInfo);
	
	// 若勾选“仅主显示器”，则跳过非主显示器
	if (isMainOnly && !(monitorInfo.dwFlags & MONITORINFOF_PRIMARY)) {
		return TRUE; // 继续枚举其他显示器
	}
	
	// 创建遮罩窗口（全屏、分层透明、点击穿透）
	DWORD exStyle = WS_EX_LAYERED | WS_EX_TRANSPARENT; // WS_EX_TRANSPARENT使窗口不响应鼠标
	if (isTopmost) {
		exStyle |= WS_EX_TOPMOST; // 置顶（优先级高于任务栏）
	}
	
	HWND hMaskWnd = CreateWindowEx(
								   exStyle,
								   "MaskWindowClass", MASK_WINDOW_TITLE,
								   WS_POPUP, // 无标题栏的弹出窗口（全屏用）
								   monitorInfo.rcMonitor.left,  // 窗口左上角X（显示器左边界）
								   monitorInfo.rcMonitor.top,   // 窗口左上角Y（显示器上边界）
								   monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, // 窗口宽度
								   monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top, // 窗口高度
								   NULL, NULL, hInstance, NULL
								   );
	
	// 设置置顶状态
	SetWindowPos(hMaskWnd, isTopmost ? HWND_TOPMOST : HWND_NOTOPMOST, 
				 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	
	// 初始透明度（完全透明）
	SetLayeredWindowAttributes(hMaskWnd, 0, 0, LWA_ALPHA);
	ShowWindow(hMaskWnd, SW_SHOWNOACTIVATE); // 显示但不激活（避免抢占焦点）
	
	maskWnds->push_back(hMaskWnd);
	return TRUE;
}

// 主窗口消息处理函数（全局函数）
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_CREATE: {
		// 1. 创建RGB滑块（范围0-255，初始位置255）
		hSliderR = CreateWindow(
								TRACKBAR_CLASS, "R",
								WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
								50, 30, 200, 30,
								hwnd, (HMENU)1001, GetModuleHandle(NULL), NULL
								);
		SendMessage(hSliderR, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
		SendMessage(hSliderR, TBM_SETPOS, TRUE, r); // 初始位置与全局变量一致
		
		hSliderG = CreateWindow(
								TRACKBAR_CLASS, "G",
								WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
								50, 70, 200, 30,
								hwnd, (HMENU)1002, GetModuleHandle(NULL), NULL
								);
		SendMessage(hSliderG, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
		SendMessage(hSliderG, TBM_SETPOS, TRUE, g);
		
		hSliderB = CreateWindow(
								TRACKBAR_CLASS, "B",
								WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
								50, 110, 200, 30,
								hwnd, (HMENU)1003, GetModuleHandle(NULL), NULL
								);
		SendMessage(hSliderB, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
		SendMessage(hSliderB, TBM_SETPOS, TRUE, b);
		
		// 2. 创建透明度滑块（范围0-100，初始位置0）
		hSliderA = CreateWindow(
								TRACKBAR_CLASS, "Alpha",
								WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
								50, 150, 200, 30,
								hwnd, (HMENU)1004, GetModuleHandle(NULL), NULL
								);
		SendMessage(hSliderA, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
		SendMessage(hSliderA, TBM_SETPOS, TRUE, alpha);
		
		// 3. 创建数值显示静态文本（格式统一为“A: 0%”）
		hStaticR = CreateWindow(
								"STATIC", "R: 255",
								WS_CHILD | WS_VISIBLE | SS_LEFT,
								260, 30, 80, 20,
								hwnd, NULL, GetModuleHandle(NULL), NULL
								);
		hStaticG = CreateWindow(
								"STATIC", "G: 255",
								WS_CHILD | WS_VISIBLE | SS_LEFT,
								260, 70, 80, 20,
								hwnd, NULL, GetModuleHandle(NULL), NULL
								);
		hStaticB = CreateWindow(
								"STATIC", "B: 255",
								WS_CHILD | WS_VISIBLE | SS_LEFT,
								260, 110, 80, 20,
								hwnd, NULL, GetModuleHandle(NULL), NULL
								);
		hStaticA = CreateWindow(
								"STATIC", "A: 0%",
								WS_CHILD | WS_VISIBLE | SS_LEFT,
								260, 150, 80, 20,
								hwnd, NULL, GetModuleHandle(NULL), NULL
								);
		
		// 4. 创建颜色预览区域
		hwndPreview = CreateWindow(
								   "STATIC", "",
								   WS_CHILD | WS_VISIBLE | SS_WHITERECT,
								   100, 190, 200, 60,
								   hwnd, NULL, GetModuleHandle(NULL), NULL
								   );
		
		// 5. 创建复选框（修复BS_CHECKED错误，用CheckDlgButton设初始选中）
		hCheckTopmost = CreateWindow(
									 "BUTTON", "完全置顶",
									 WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, // 仅保留基础样式
									 50, 260, 100, 20,
									 hwnd, (HMENU)3001, GetModuleHandle(NULL), NULL
									 );
		CheckDlgButton(hwnd, 3001, BST_CHECKED); // 初始选中“完全置顶”
		
		hCheckMainOnly = CreateWindow(
									  "BUTTON", "仅主显示器",
									  WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
									  160, 260, 100, 20,
									  hwnd, (HMENU)3002, GetModuleHandle(NULL), NULL
									  );
		
		return 0;
	}
		
		// 处理滑块拖动事件
		case WM_HSCROLL: {
			if (lParam == (LPARAM)hSliderR) {
				r = SendMessage(hSliderR, TBM_GETPOS, 0, 0);
				r = clamp(r, 0, 255);
			} else if (lParam == (LPARAM)hSliderG) {
				g = SendMessage(hSliderG, TBM_GETPOS, 0, 0);
				g = clamp(g, 0, 255);
			} else if (lParam == (LPARAM)hSliderB) {
				b = SendMessage(hSliderB, TBM_GETPOS, 0, 0);
				b = clamp(b, 0, 255);
			} else if (lParam == (LPARAM)hSliderA) {
				alpha = SendMessage(hSliderA, TBM_GETPOS, 0, 0);
				alpha = clamp(alpha, 0, 100);
			}
			UpdateMaskWindow(); // 同步更新遮罩和显示
			return 0;
		}
		
		// 处理复选框点击事件
		case WM_COMMAND: {
			if (LOWORD(wParam) == 3001) { // 完全置顶
				isTopmost = IsDlgButtonChecked(hwnd, 3001) == BST_CHECKED;
				// 更新所有遮罩窗口的置顶状态
				for (HWND hMaskWnd : hMaskWnds) {
					SetWindowPos(hMaskWnd, isTopmost ? HWND_TOPMOST : HWND_NOTOPMOST,
								 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				}
			} else if (LOWORD(wParam) == 3002) { // 仅主显示器
				isMainOnly = IsDlgButtonChecked(hwnd, 3002) == BST_CHECKED;
				// 销毁旧遮罩，重新创建（按新设置生效）
				for (HWND hMaskWnd : hMaskWnds) DestroyWindow(hMaskWnd);
				hMaskWnds.clear();
				EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&hMaskWnds));
				UpdateMaskWindow();
			}
			return 0;
		}
		
		// 主窗口关闭时清理资源
	case WM_DESTROY:
		for (HWND hMaskWnd : hMaskWnds) DestroyWindow(hMaskWnd);
		PostQuitMessage(0);
		return 0;
		
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

// 程序入口（WinMain）
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	// 初始化通用控件（轨迹栏、复选框等依赖此函数）
	InitCommonControls();
	
	// 1. 注册遮罩窗口类
	WNDCLASSEX maskWc = {0};
	maskWc.cbSize = sizeof(WNDCLASSEX);
	maskWc.lpfnWndProc = MaskWindowProc;
	maskWc.hInstance = hInstance;
	maskWc.lpszClassName = "MaskWindowClass";
	RegisterClassEx(&maskWc);
	
	// 2. 注册主窗口类
	WNDCLASSEX mainWc = {0};
	mainWc.cbSize = sizeof(WNDCLASSEX);
	mainWc.lpfnWndProc = MainWindowProc;
	mainWc.hInstance = hInstance;
	mainWc.lpszClassName = "MainWindowClass";
	RegisterClassEx(&mainWc);
	
	// 3. 枚举所有显示器，创建遮罩窗口（调用全局的MonitorEnumProc）
	EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&hMaskWnds));
	
	// 4. 创建主窗口（禁止最大化，大小400x300）
	hMainWnd = CreateWindowEx(
							  0,
							  "MainWindowClass", MAIN_WINDOW_TITLE,
							  WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX, // 移除最大化按钮
							  CW_USEDEFAULT, CW_USEDEFAULT, 400, 300,
							  NULL, NULL, hInstance, NULL
							  );
	
	// 5. 显示主窗口并启动消息循环
	ShowWindow(hMainWnd, nCmdShow);
	UpdateWindow(hMainWnd);
	
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	
	// 6. 程序退出前注销窗口类（释放资源）
	UnregisterClass("MaskWindowClass", hInstance);
	UnregisterClass("MainWindowClass", hInstance);
	return 0;
}

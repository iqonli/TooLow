#include <windows.h>
#include <commctrl.h>
#include <string.h>
#include <vector>
#include <bits/stdc++.h>

// 编译命令（MinGW）：g++ TooLowGUI.cpp -o TooLowGUI.exe -lcomctl32 -mwindows
using namespace std;

// 窗口标题常量
const char* MAIN_WINDOW_TITLE = "太Low - 屏幕颜色遮罩 v2.0";
// 版权信息常量
const char* COPYRIGHT_TEXT = "by IQ Online Studio, github.com/iqonli/TooLow";
// 定时器ID（自动刷新置顶用）
const UINT TIMER_AUTO_REFRESH = 10086;

// 新增：用于传递遮罩窗口列表和序号的结构体（适配标题编号）
struct MonitorData {
	vector<HWND>* maskWnds;  // 遮罩窗口句柄列表
	int windowIndex;         // 遮罩窗口序号（1开始，主显示器为1）
};

// 函数前向声明（全局作用域）
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData);
LRESULT CALLBACK MaskWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void UpdateMaskWindow();
int clamp(int val, int min, int max);
void RecreateMaskWindows();

// 全局变量
vector<HWND> hMaskWnds;       // 所有遮罩窗口句柄
HWND hMainWnd;                // 主窗口句柄
HWND hSliderR, hSliderG, hSliderB, hSliderA; // 滑块控件
HWND hStaticR, hStaticG, hStaticB, hStaticA; // 数值显示文本
HWND hwndPreview;             // 颜色预览区
HWND hCheckTopmost;           // 完全置顶复选框
HWND hCheckMainOnly;          // 仅主显示器复选框
HWND hCheckAutoRefresh;       // 自动刷新置顶复选框
HWND hStaticCopyright;        // 版权信息文本控件

int r = 255, g = 255, b = 255; // RGB颜色（初始白色）
int alpha = 0;                 // 透明度（0-100）
bool isTopmost = true;         // 是否完全置顶（高于任务栏）
bool isMainOnly = false;       // 是否仅主显示器
bool isAutoRefreshTopmost = false; // 是否自动刷新置顶

// 数值范围限制
int clamp(int val, int min, int max) {
	return val < min ? min : (val > max ? max : val);
}

// 遮罩窗口消息处理
LRESULT CALLBACK MaskWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_DESTROY:
		return 0;
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

// 更新遮罩颜色/透明度 + 控件显示
void UpdateMaskWindow() {
	// 1. 更新所有遮罩窗口
	for (HWND hMaskWnd : hMaskWnds) {
		if (hMaskWnd == NULL) continue;
		
		// 设置透明度（0-100 → 0-255）
		SetLayeredWindowAttributes(hMaskWnd, 0, (BYTE)(alpha * 2.55f), LWA_ALPHA);
		
		// 绘制背景色
		HDC hdc = GetDC(hMaskWnd);
		HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
		RECT rect;
		GetClientRect(hMaskWnd, &rect);
		FillRect(hdc, &rect, hBrush);
		ReleaseDC(hMaskWnd, hdc);
		DeleteObject(hBrush);
	}
	
	// 2. 更新数值显示
	char buffer[20];
	sprintf(buffer, "R: %d", r);
	SetWindowText(hStaticR, buffer);
	sprintf(buffer, "G: %d", g);
	SetWindowText(hStaticG, buffer);
	sprintf(buffer, "B: %d", b);
	SetWindowText(hStaticB, buffer);
	sprintf(buffer, "A: %d%%", alpha);
	SetWindowText(hStaticA, buffer);
	
	// 3. 更新颜色预览区
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

// 重新创建所有遮罩窗口（修改：传递序号用于标题编号）
void RecreateMaskWindows() {
	// 1. 销毁旧遮罩窗口
	for (HWND hMaskWnd : hMaskWnds) {
		if (hMaskWnd != NULL) {
			DestroyWindow(hMaskWnd);
		}
	}
	hMaskWnds.clear();
	
	// 短暂延迟确保窗口资源释放
	Sleep(20);
	
	// 新增：初始化遮罩窗口序号（主显示器为1，其余按顺序2/3/4...）
	MonitorData monitorData;
	monitorData.maskWnds = &hMaskWnds;
	monitorData.windowIndex = 1; // 序号从1开始
	
	// 2. 重新创建遮罩窗口（传递带序号的结构体）
	EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitorData));
	
	// 3. 应用当前颜色和透明度设置
	UpdateMaskWindow();
}

// 显示器枚举回调（创建遮罩窗口，修改：按序号生成标题）
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	// 新增：解析传递的结构体（包含窗口列表和序号）
	MonitorData* pMonitorData = reinterpret_cast<MonitorData*>(dwData);
	if (pMonitorData == NULL || pMonitorData->maskWnds == NULL) return FALSE;
	
	vector<HWND>* maskWnds = pMonitorData->maskWnds;
	int& windowIndex = pMonitorData->windowIndex; // 引用序号，方便修改
	HINSTANCE hInstance = GetModuleHandle(NULL);
	
	// 1. 获取显示器信息
	MONITORINFOEX monitorInfo = {0};
	monitorInfo.cbSize = sizeof(MONITORINFOEX);
	if (!GetMonitorInfo(hMonitor, &monitorInfo)) {
		windowIndex++; // 即使获取信息失败，序号也递增（避免重复）
		return TRUE;
	}
	
	// 2. 仅主显示器：跳过非主显示器
	if (isMainOnly && !(monitorInfo.dwFlags & MONITORINFOF_PRIMARY)) {
		windowIndex++;
		return TRUE;
	}
	
	// 3. 确定窗口区域
	RECT windowRect;
	if (isTopmost) {
		// 置顶：使用全屏区域（覆盖任务栏）
		windowRect = monitorInfo.rcMonitor;
	} else {
		// 非置顶：使用工作区（排除任务栏）
		SystemParametersInfo(SPI_GETWORKAREA, 0, &windowRect, 0);
		IntersectRect(&windowRect, &windowRect, &monitorInfo.rcMonitor);
	}
	
	// 4. 计算窗口宽高
	int winWidth = windowRect.right - windowRect.left;
	int winHeight = windowRect.bottom - windowRect.top;
	if (winWidth <= 0 || winHeight <= 0) {
		windowIndex++;
		return TRUE;
	}
	
	// 新增：生成遮罩窗口标题（主显示器为“遮罩窗口-1”，其余按序号）
	char maskWindowTitle[50];
	sprintf(maskWindowTitle, "遮罩窗口-%d", windowIndex);
	// 主显示器特殊处理（确保主显示器一定是“遮罩窗口-1”，即使枚举顺序变化）
	if (monitorInfo.dwFlags & MONITORINFOF_PRIMARY) {
		sprintf(maskWindowTitle, "遮罩窗口-1");
		windowIndex = 2; // 主显示器用了1，下一个从2开始
	} else {
		windowIndex++; // 非主显示器序号递增
	}
	
	// 5. 创建遮罩窗口（使用生成的标题）
	DWORD exStyle = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW; 
	if (isTopmost) {
		exStyle |= WS_EX_TOPMOST;
	}
	
	HWND hMaskWnd = CreateWindowEx(
								   exStyle,
								   "MaskWindowClass", maskWindowTitle, // 传入动态生成的标题
								   WS_POPUP,
								   windowRect.left,
								   windowRect.top,
								   winWidth,
								   winHeight,
								   NULL, NULL, hInstance, NULL
								   );
	
	if (hMaskWnd == NULL) {
		return TRUE;
	}
	
	// 6. 设置窗口属性
	if (isTopmost) {
		SetWindowPos(hMaskWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	} else {
		SetWindowPos(hMaskWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		SetWindowPos(hMaskWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
	SetLayeredWindowAttributes(hMaskWnd, 0, 0, LWA_ALPHA);
	ShowWindow(hMaskWnd, SW_SHOWNOACTIVATE);
	
	// 7. 添加到列表
	maskWnds->push_back(hMaskWnd);
	return TRUE;
}

// 主窗口消息处理（修改：增大版权字体）
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		// 窗口创建：初始化所有控件
		case WM_CREATE: {
		// 1. 注册定时器
		SetTimer(hwnd, TIMER_AUTO_REFRESH, 1000, NULL);
		
		// 新增：创建版权信息文本控件（字体增大为12号加粗）
		hStaticCopyright = CreateWindow(
										"STATIC", COPYRIGHT_TEXT,
										WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
										50, 10, 300, 25,  // 高度从20改为25，适配更大字体
										hwnd, NULL, GetModuleHandle(NULL), NULL
										);
		// 修改：版权字体改为12号加粗（原10号常规），提升视觉清晰度
		HFONT hFont = CreateFont(
								 12, 0, 0, 0, FW_BOLD,  // 高度12，字重加粗（FW_BOLD）
								 FALSE, FALSE, FALSE,
								 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
								 DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "微软雅黑"
								 );
		SendMessage(hStaticCopyright, WM_SETFONT, (WPARAM)hFont, TRUE);
		
		// 2. 创建RGB滑块（y坐标从30开始，避开版权文本）
		hSliderR = CreateWindow(
								TRACKBAR_CLASS, "R",
								WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
								50, 40, 200, 30,  // y从30改为40，与版权文本间距增大
								hwnd, (HMENU)1001, GetModuleHandle(NULL), NULL
								);
		SendMessage(hSliderR, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
		SendMessage(hSliderR, TBM_SETPOS, TRUE, r);
		
		hSliderG = CreateWindow(
								TRACKBAR_CLASS, "G",
								WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
								50, 80, 200, 30,  // 所有滑块y坐标同步下移10
								hwnd, (HMENU)1002, GetModuleHandle(NULL), NULL
								);
		SendMessage(hSliderG, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
		SendMessage(hSliderG, TBM_SETPOS, TRUE, g);
		
		hSliderB = CreateWindow(
								TRACKBAR_CLASS, "B",
								WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
								50, 120, 200, 30,
								hwnd, (HMENU)1003, GetModuleHandle(NULL), NULL
								);
		SendMessage(hSliderB, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
		SendMessage(hSliderB, TBM_SETPOS, TRUE, b);
		
		// 3. 透明度滑块
		hSliderA = CreateWindow(
								TRACKBAR_CLASS, "Alpha",
								WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
								50, 160, 200, 30,
								hwnd, (HMENU)1004, GetModuleHandle(NULL), NULL
								);
		SendMessage(hSliderA, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
		SendMessage(hSliderA, TBM_SETPOS, TRUE, alpha);
		
		// 4. 数值显示文本（y坐标同步下移10）
		hStaticR = CreateWindow("STATIC", "R: 255", WS_CHILD | WS_VISIBLE | SS_LEFT, 260, 40, 80, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
		hStaticG = CreateWindow("STATIC", "G: 255", WS_CHILD | WS_VISIBLE | SS_LEFT, 260, 80, 80, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
		hStaticB = CreateWindow("STATIC", "B: 255", WS_CHILD | WS_VISIBLE | SS_LEFT, 260, 120, 80, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
		hStaticA = CreateWindow("STATIC", "A: 0%",  WS_CHILD | WS_VISIBLE | SS_LEFT, 260, 160, 80, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
		
		// 5. 颜色预览区（y坐标下移10）
		hwndPreview = CreateWindow(
								   "STATIC", "",
								   WS_CHILD | WS_VISIBLE | SS_WHITERECT,
								   100, 200, 200, 60,  // y从190改为200
								   hwnd, NULL, GetModuleHandle(NULL), NULL
								   );
		
		// 6. 复选框（y坐标下移10）
		hCheckTopmost = CreateWindow(
									 "BUTTON", "完全置顶",
									 WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
									 50, 270, 100, 20,  // y从260改为270
									 hwnd, (HMENU)3001, GetModuleHandle(NULL), NULL
									 );
		CheckDlgButton(hwnd, 3001, BST_CHECKED);
		
		hCheckMainOnly = CreateWindow(
									  "BUTTON", "仅主显示器",
									  WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
									  160, 270, 100, 20,
									  hwnd, (HMENU)3002, GetModuleHandle(NULL), NULL
									  );
		
		hCheckAutoRefresh = CreateWindow(
										 "BUTTON", "自动刷新置顶",
										 WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
										 270, 270, 120, 20,
										 hwnd, (HMENU)3003, GetModuleHandle(NULL), NULL
										 );
		
		isAutoRefreshTopmost = false;
		CheckDlgButton(hwnd, 3003, BST_UNCHECKED);
		EnableWindow(hCheckAutoRefresh, isTopmost);
		
		return 0;
	}
		
		// 滑块拖动：更新颜色/透明度
		case WM_HSCROLL: {
			if (lParam == (LPARAM)hSliderR) {
				r = clamp(SendMessage(hSliderR, TBM_GETPOS, 0, 0), 0, 255);
			} else if (lParam == (LPARAM)hSliderG) {
				g = clamp(SendMessage(hSliderG, TBM_GETPOS, 0, 0), 0, 255);
			} else if (lParam == (LPARAM)hSliderB) {
				b = clamp(SendMessage(hSliderB, TBM_GETPOS, 0, 0), 0, 255);
			} else if (lParam == (LPARAM)hSliderA) {
				alpha = clamp(SendMessage(hSliderA, TBM_GETPOS, 0, 0), 0, 100);
			}
			UpdateMaskWindow();
			return 0;
		}
		
		// 复选框点击：处理置顶、仅主显示器、自动刷新
		case WM_COMMAND: {
			if (LOWORD(wParam) == 3001) {
				isTopmost = (IsDlgButtonChecked(hwnd, 3001) == BST_CHECKED);
				EnableWindow(hCheckAutoRefresh, isTopmost);
				RecreateMaskWindows();
			} else if (LOWORD(wParam) == 3002) {
				isMainOnly = (IsDlgButtonChecked(hwnd, 3002) == BST_CHECKED);
				RecreateMaskWindows();
			} else if (LOWORD(wParam) == 3003) {
				isAutoRefreshTopmost = (IsDlgButtonChecked(hwnd, 3003) == BST_CHECKED);
			}
			return 0;
		}
		
		// 定时器：1秒刷新一次置顶
		case WM_TIMER: {
			if (wParam == TIMER_AUTO_REFRESH && isTopmost && isAutoRefreshTopmost) {
				for (HWND hMaskWnd : hMaskWnds) {
					if (hMaskWnd == NULL) continue;
					SetWindowPos(hMaskWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				}
			}
			return 0;
		}
		
		// 窗口销毁：清理资源
	case WM_DESTROY:
		KillTimer(hwnd, TIMER_AUTO_REFRESH);
		for (HWND hMaskWnd : hMaskWnds) {
			if (hMaskWnd != NULL) DestroyWindow(hMaskWnd);
		}
		PostQuitMessage(0);
		return 0;
		
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

// 程序入口
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	// 隐藏控制台窗口（若存在）
	HWND hConsoleWnd = GetConsoleWindow();
	if (hConsoleWnd != NULL) {
		ShowWindow(hConsoleWnd, SW_HIDE);
	}
	
	// 初始化通用控件
	InitCommonControls();
	
	// 1. 注册遮罩窗口类
	WNDCLASSEX maskWc = {0};
	maskWc.cbSize = sizeof(WNDCLASSEX);
	maskWc.style = CS_CLASSDC;
	maskWc.lpfnWndProc = MaskWindowProc;
	maskWc.hInstance = hInstance;
	maskWc.lpszClassName = "MaskWindowClass";
	maskWc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	if (!RegisterClassEx(&maskWc)) {
		MessageBox(NULL, "遮罩窗口类注册失败！", "错误", MB_ICONERROR);
		return 1;
	}
	
	// 2. 注册主窗口类
	WNDCLASSEX mainWc = {0};
	mainWc.cbSize = sizeof(WNDCLASSEX);
	mainWc.style = CS_CLASSDC;
	mainWc.lpfnWndProc = MainWindowProc;
	mainWc.hInstance = hInstance;
	mainWc.lpszClassName = "MainWindowClass";
	mainWc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	if (!RegisterClassEx(&mainWc)) {
		MessageBox(NULL, "主窗口类注册失败！", "错误", MB_ICONERROR);
		UnregisterClass(maskWc.lpszClassName, hInstance);
		return 1;
	}
	
	// 3. 创建初始遮罩窗口
	RecreateMaskWindows();
	
	// 4. 创建主窗口（高度增加40，适配字体和控件下移）
	DWORD mainExStyle = WS_EX_APPWINDOW; 
	hMainWnd = CreateWindowEx(
							  mainExStyle,
							  "MainWindowClass", MAIN_WINDOW_TITLE,
							  WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
							  CW_USEDEFAULT, CW_USEDEFAULT, 400, 420,  // 高度从380改为420
							  NULL, NULL, hInstance, NULL
							  );
	if (hMainWnd == NULL) {
		MessageBox(NULL, "主窗口创建失败！", "错误", MB_ICONERROR);
		UnregisterClass(maskWc.lpszClassName, hInstance);
		UnregisterClass(mainWc.lpszClassName, hInstance);
		return 1;
	}
	
	// 5. 显示主窗口并启动消息循环
	ShowWindow(hMainWnd, nCmdShow);
	UpdateWindow(hMainWnd);
	
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	
	// 6. 程序退出：注销窗口类
	UnregisterClass("MaskWindowClass", hInstance);
	UnregisterClass("MainWindowClass", hInstance);
	return 0;
}

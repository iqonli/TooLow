#include <windows.h>
#include <commctrl.h>
#include <string.h>
#include <vector>
#include <string>
#include <cctype>

// 编译命令（MinGW）：g++ TooLowGUI.cpp -o TooLowGUI.exe -lcomctl32 -mwindows
using namespace std;

// 窗口标题常量
const char* MAIN_WINDOW_TITLE = "太Low - 屏幕颜色遮罩 v2.0";
// 版权信息常量
const char* COPYRIGHT_TEXT = "by IQ Online Studio, github.com/iqonli/TooLow";
// 定时器ID（自动刷新置顶用）
const UINT TIMER_AUTO_REFRESH = 10086;

// 遮罩窗口信息结构体（关联窗口句柄与对应显示器）
struct MaskWindowInfo
{
	HWND hWnd;         // 遮罩窗口句柄
	HMONITOR hMonitor; // 对应显示器句柄
};

// 用于传递遮罩窗口列表和序号的结构体（适配标题编号）
struct MonitorData
{
	vector<MaskWindowInfo>* maskWnds; // 遮罩窗口信息列表
	int windowIndex;                  // 遮罩窗口序号（1开始，主显示器为1）
};

// 函数前向声明（全局作用域）
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData);
LRESULT CALLBACK MaskWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void UpdateMaskWindow();
int clamp(int val, int min, int max);
void RecreateMaskWindows();
string getCurrentHex();                // 根据RGB生成6位小写Hex字符串
bool hexToRgb(const string& hex, int& r, int& g, int& b); // Hex转RGB
HWND findCurrentMonitorMaskWnd();      // 找到GUI所在显示器的遮罩窗口
bool IsAllMasksVisible();              // 新增：检查所有遮罩窗口是否均显示

// 全局变量
vector<MaskWindowInfo> hMaskWnds;     // 所有遮罩窗口信息
HWND hMainWnd;                        // 主窗口句柄
HWND hSliderR, hSliderG, hSliderB, hSliderA; // 滑块控件
HWND hStaticR, hStaticG, hStaticB, hStaticA; // 数值显示文本
HWND hwndPreview;                     // 颜色预览区
HWND hCheckTopmost;                   // 完全置顶复选框
HWND hCheckAutoRefresh;               // 自动刷新置顶复选框
HWND hStaticCopyright;                // 版权信息文本控件
HWND hStaticHexLabel;                 // Hex标签控件（显示"HEX: #"）
HWND hEditHex;                        // Hex颜色输入框
HWND hBtnCloseMask;                   // 关闭此屏遮罩按钮
HWND hBtnOpenMask;                    // 打开此屏遮罩按钮
HWND hBtnToggleAllMasks;              // 新增：关闭全部遮罩/打开遮罩按钮
bool isUpdatingHex = false;           // 防Hex输入框循环更新标志

int r = 255, g = 255, b = 255; // RGB颜色（初始白色）
int alpha = 0;                 // 透明度（0-100）
bool isTopmost = true;         // 是否完全置顶（高于任务栏）
bool isAutoRefreshTopmost = false; // 是否自动刷新置顶

// 数值范围限制
int clamp(int val, int min, int max)
{
	return val < min ? min : (val > max ? max : val);
}

// 遮罩窗口消息处理
LRESULT CALLBACK MaskWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
		return 0;
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

// 根据当前RGB生成6位小写Hex字符串（无#和Alpha）
string getCurrentHex()
{
	char buffer[7];
	sprintf(buffer, "%02x%02x%02x", r, g, b); // 格式化为6位小写Hex
	return string(buffer);
}

// Hex字符串转RGB（返回是否转换成功，成功则更新r/g/b引用）
bool hexToRgb(const string& hex, int& r, int& g, int& b)
{
	if (hex.length() != 6) return false;
	for (char c : hex)
	{
		if (!isxdigit(c)) return false;
	}
	r = strtol(hex.substr(0, 2).c_str(), NULL, 16);
	g = strtol(hex.substr(2, 2).c_str(), NULL, 16);
	b = strtol(hex.substr(4, 2).c_str(), NULL, 16);
	r = clamp(r, 0, 255);
	g = clamp(g, 0, 255);
	b = clamp(b, 0, 255);
	return true;
}

// 找到GUI主窗口所在显示器对应的遮罩窗口句柄
HWND findCurrentMonitorMaskWnd()
{
	if (hMainWnd == NULL || hMaskWnds.empty()) return NULL;
	
	RECT mainRect;
	if (!GetWindowRect(hMainWnd, &mainRect)) return NULL;
	
	HMONITOR hCurrentMonitor = MonitorFromRect(&mainRect, MONITOR_DEFAULTTONEAREST);
	if (hCurrentMonitor == NULL) return NULL;
	
	for (const auto& info : hMaskWnds)
	{
		if (info.hMonitor == hCurrentMonitor)
		{
			return info.hWnd;
		}
	}
	return NULL;
}

// 新增：检查所有遮罩窗口是否均处于显示状态
bool IsAllMasksVisible()
{
	if (hMaskWnds.empty()) return false; // 无窗口时视为未全部显示
	
	// 遍历所有窗口，只要有一个不可见则返回false
	for (const auto& info : hMaskWnds)
	{
		if (info.hWnd == NULL || !IsWindowVisible(info.hWnd))
		{
			return false;
		}
	}
	return true; // 所有窗口均可见
}

// 更新遮罩颜色/透明度 + 控件显示
void UpdateMaskWindow()
{
	// 1. 更新所有遮罩窗口
	for (const auto& info : hMaskWnds)
	{
		HWND hMaskWnd = info.hWnd;
		if (hMaskWnd == NULL) continue;
		
		SetLayeredWindowAttributes(hMaskWnd, 0, (BYTE)(alpha * 2.55f), LWA_ALPHA);
		HDC hdc = GetDC(hMaskWnd);
		HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
		RECT rect;
		GetClientRect(hMaskWnd, &rect);
		FillRect(hdc, &rect, hBrush);
		ReleaseDC(hMaskWnd, hdc);
		DeleteObject(hBrush);
	}
	
	// 2. 更新RGB数值显示
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
	if (hwndPreview != NULL)
	{
		HDC hdc = GetDC(hwndPreview);
		HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
		RECT rect;
		GetClientRect(hwndPreview, &rect);
		FillRect(hdc, &rect, hBrush);
		ReleaseDC(hwndPreview, hdc);
		DeleteObject(hBrush);
	}
	
	// 4. 同步更新Hex输入框（防循环更新）
	if (hEditHex != NULL && !isUpdatingHex)
	{
		isUpdatingHex = true;
		string hex = getCurrentHex();
		SetWindowTextA(hEditHex, hex.c_str());
		int len = hex.length();
		SendMessage(hEditHex, EM_SETSEL, (WPARAM)len, (LPARAM)len);
		isUpdatingHex = false;
	}
}

// 重新创建所有遮罩窗口
void RecreateMaskWindows()
{
	// 1. 销毁旧遮罩窗口
	for (const auto& info : hMaskWnds)
	{
		if (info.hWnd != NULL)
		{
			DestroyWindow(info.hWnd);
		}
	}
	hMaskWnds.clear();
	
	Sleep(20); // 确保资源释放
	
	// 初始化遮罩窗口序号（主显示器为1）
	MonitorData monitorData;
	monitorData.maskWnds = &hMaskWnds;
	monitorData.windowIndex = 1;
	
	// 2. 重新创建遮罩窗口（所有显示器均创建，删除仅主屏幕逻辑）
	EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitorData));
	
	// 3. 应用当前设置
	UpdateMaskWindow();
	
	// 4. 更新"关闭全部遮罩/打开"按钮文本（默认全开启，显示"关闭全部遮罩"）
	if (hBtnToggleAllMasks != NULL)
	{
		SetWindowText(hBtnToggleAllMasks, IsAllMasksVisible() ? "关闭全部遮罩" : "打开全部遮罩");
	}
}

// 显示器枚举回调（删除仅主屏幕过滤逻辑）
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
	MonitorData* pMonitorData = reinterpret_cast<MonitorData*>(dwData);
	if (pMonitorData == NULL || pMonitorData->maskWnds == NULL) return FALSE;
	
	vector<MaskWindowInfo>* maskWnds = pMonitorData->maskWnds;
	int& windowIndex = pMonitorData->windowIndex;
	HINSTANCE hInstance = GetModuleHandle(NULL);
	
	// 获取显示器信息
	MONITORINFOEX monitorInfo = {0};
	monitorInfo.cbSize = sizeof(MONITORINFOEX);
	if (!GetMonitorInfo(hMonitor, &monitorInfo))
	{
		windowIndex++;
		return TRUE;
	}
	
	// 确定窗口区域（删除仅主屏幕判断）
	RECT windowRect;
	if (isTopmost)
	{
		windowRect = monitorInfo.rcMonitor; // 置顶：覆盖任务栏
	}
	else
	{
		SystemParametersInfo(SPI_GETWORKAREA, 0, &windowRect, 0); // 非置顶：排除任务栏
		IntersectRect(&windowRect, &windowRect, &monitorInfo.rcMonitor);
	}
	
	int winWidth = windowRect.right - windowRect.left;
	int winHeight = windowRect.bottom - windowRect.top;
	if (winWidth <= 0 || winHeight <= 0)
	{
		windowIndex++;
		return TRUE;
	}
	
	// 生成遮罩窗口标题（主显示器固定为1，其余按序号递增）
	char maskWindowTitle[50];
	if (monitorInfo.dwFlags & MONITORINFOF_PRIMARY)
	{
		sprintf(maskWindowTitle, "遮罩窗口-1");
		windowIndex = 2; // 主显示器用1，下一个从2开始
	}
	else
	{
		sprintf(maskWindowTitle, "遮罩窗口-%d", windowIndex);
		windowIndex++;
	}
	
	// 创建遮罩窗口
	DWORD exStyle = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW;
	if (isTopmost) exStyle |= WS_EX_TOPMOST;
	
	HWND hMaskWnd = CreateWindowEx(
								   exStyle,
								   "MaskWindowClass", maskWindowTitle,
								   WS_POPUP,
								   windowRect.left, windowRect.top,
								   winWidth, winHeight,
								   NULL, NULL, hInstance, NULL
								   );
	
	if (hMaskWnd == NULL)
	{
		return TRUE;
	}
	
	// 设置窗口属性
	if (isTopmost)
	{
		SetWindowPos(hMaskWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
	else
	{
		SetWindowPos(hMaskWnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		SetWindowPos(hMaskWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
	SetLayeredWindowAttributes(hMaskWnd, 0, 0, LWA_ALPHA);
	ShowWindow(hMaskWnd, SW_SHOWNOACTIVATE); // 默认显示所有遮罩
	
	// 加入遮罩窗口列表
	MaskWindowInfo info;
	info.hWnd = hMaskWnd;
	info.hMonitor = hMonitor;
	maskWnds->push_back(info);
	
	return TRUE;
}

// 主窗口消息处理（新增全部控制按钮逻辑）
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
	{
		// 注册定时器
		SetTimer(hwnd, TIMER_AUTO_REFRESH, 1000, NULL);
		
		// 版权信息控件
		hStaticCopyright = CreateWindow(
										"STATIC", COPYRIGHT_TEXT,
										WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
										50, 360, 300, 20,
										hwnd, NULL, GetModuleHandle(NULL), NULL
										);
		
		// RGB滑块
		hSliderR = CreateWindow(
								TRACKBAR_CLASS, "R",
								WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
								50, 40, 200, 30,
								hwnd, (HMENU)1001, GetModuleHandle(NULL), NULL
								);
		SendMessage(hSliderR, TBM_SETRANGE, TRUE, MAKELONG(0, 255));
		SendMessage(hSliderR, TBM_SETPOS, TRUE, r);
		
		hSliderG = CreateWindow(
								TRACKBAR_CLASS, "G",
								WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
								50, 80, 200, 30,
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
		
		// 透明度滑块
		hSliderA = CreateWindow(
								TRACKBAR_CLASS, "Alpha",
								WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
								50, 160, 200, 30,
								hwnd, (HMENU)1004, GetModuleHandle(NULL), NULL
								);
		SendMessage(hSliderA, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
		SendMessage(hSliderA, TBM_SETPOS, TRUE, alpha);
		
		// 数值显示文本
		hStaticR = CreateWindow("STATIC", "R: 255", WS_CHILD | WS_VISIBLE | SS_LEFT, 260, 40, 80, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
		hStaticG = CreateWindow("STATIC", "G: 255", WS_CHILD | WS_VISIBLE | SS_LEFT, 260, 80, 80, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
		hStaticB = CreateWindow("STATIC", "B: 255", WS_CHILD | WS_VISIBLE | SS_LEFT, 260, 120, 80, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
		hStaticA = CreateWindow("STATIC", "A: 0%",  WS_CHILD | WS_VISIBLE | SS_LEFT, 260, 160, 80, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
		
		// 颜色预览区
		hwndPreview = CreateWindow(
								   "STATIC", "",
								   WS_CHILD | WS_VISIBLE | SS_WHITERECT,
								   100, 200, 200, 60,
								   hwnd, NULL, GetModuleHandle(NULL), NULL
								   );
		
		// 复选框（删除仅主屏幕复选框，保留其他两个）
		hCheckTopmost = CreateWindow(
									 "BUTTON", "完全置顶",
									 WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
									 160, 270, 100, 20,
									 hwnd, (HMENU)3001, GetModuleHandle(NULL), NULL
									 );
		
		hCheckAutoRefresh = CreateWindow(
										 "BUTTON", "自动刷新置顶",
										 WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
										 270, 270, 120, 20,
										 hwnd, (HMENU)3003, GetModuleHandle(NULL), NULL
										 );
		
		// 初始化复选框状态
		CheckDlgButton(hwnd, 3001, BST_CHECKED); // 完全置顶默认勾选
		CheckDlgButton(hwnd, 3003, BST_UNCHECKED); // 自动刷新默认不勾选
		EnableWindow(hCheckAutoRefresh, isTopmost);
		
		// 新增：关闭全部遮罩/打开遮罩按钮（替换原仅主屏幕复选框位置，不遮挡其他控件）
		hBtnToggleAllMasks = CreateWindow(
										  "BUTTON", "关闭全部遮罩",  // 初始全开启，显示"关闭全部遮罩"
										  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
										  50, 270, 100, 20,       // 原仅主屏幕复选框位置，与右侧复选框间距合理
										  hwnd, (HMENU)5003,      // 新按钮ID
										  GetModuleHandle(NULL), NULL
										  );
		
		// Hex标签和输入框
		hStaticHexLabel = CreateWindow(
									   "STATIC", "HEX: #",
									   WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
									   50, 300, 50, 25,
									   hwnd, NULL, GetModuleHandle(NULL), NULL
									   );
		
		hEditHex = CreateWindowA(
								 "EDIT", "",
								 WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NOHIDESEL,
								 110, 300, 80, 25,
								 hwnd, (HMENU)4001, GetModuleHandle(NULL), NULL
								 );
		string initHex = getCurrentHex();
		SetWindowTextA(hEditHex, initHex.c_str());
		SendMessage(hEditHex, EM_SETSEL, (WPARAM)initHex.length(), (LPARAM)initHex.length());
		
		// 此屏遮罩控制按钮
		hBtnCloseMask = CreateWindow(
									 "BUTTON", "关闭此屏遮罩",
									 WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
									 50, 330, 120, 25,
									 hwnd, (HMENU)5001, GetModuleHandle(NULL), NULL
									 );
		
		hBtnOpenMask = CreateWindow(
									"BUTTON", "打开此屏遮罩",
									WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
									200, 330, 120, 25,
									hwnd, (HMENU)5002, GetModuleHandle(NULL), NULL
									);
		
		return 0;
	}
		
		// 绘制Hex输入框灰色边框
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			
			if (hEditHex != NULL && IsWindowVisible(hEditHex))
			{
				RECT editRect;
				GetWindowRect(hEditHex, &editRect);
				ScreenToClient(hwnd, (LPPOINT)&editRect.left);
				ScreenToClient(hwnd, (LPPOINT)&editRect.right);
				
				RECT borderRect = {
					editRect.left - 1,
					editRect.top - 1,
					editRect.right + 1,
					editRect.bottom + 1
				};
				
				HBRUSH hBorderBrush = CreateSolidBrush(RGB(127, 127, 127));
				FillRect(hdc, &borderRect, hBorderBrush);
				DeleteObject(hBorderBrush);
			}
			
			EndPaint(hwnd, &ps);
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
		
		// 滑块拖动事件
	case WM_HSCROLL:
		{
			if (lParam == (LPARAM)hSliderR)
			{
				r = clamp(SendMessage(hSliderR, TBM_GETPOS, 0, 0), 0, 255);
			}
			else if (lParam == (LPARAM)hSliderG)
			{
				g = clamp(SendMessage(hSliderG, TBM_GETPOS, 0, 0), 0, 255);
			}
			else if (lParam == (LPARAM)hSliderB)
			{
				b = clamp(SendMessage(hSliderB, TBM_GETPOS, 0, 0), 0, 255);
			}
			else if (lParam == (LPARAM)hSliderA)
			{
				alpha = clamp(SendMessage(hSliderA, TBM_GETPOS, 0, 0), 0, 100);
			}
			UpdateMaskWindow();
			return 0;
		}
		
		// 控件命令事件
	case WM_COMMAND:
		{
			WORD cmd = LOWORD(wParam);
			WORD event = HIWORD(wParam);
			
			// 完全置顶复选框
			if (cmd == 3001)
			{
				isTopmost = (IsDlgButtonChecked(hwnd, 3001) == BST_CHECKED);
				EnableWindow(hCheckAutoRefresh, isTopmost);
				RecreateMaskWindows();
			}
			// 自动刷新置顶复选框
			else if (cmd == 3003)
			{
				isAutoRefreshTopmost = (IsDlgButtonChecked(hwnd, 3003) == BST_CHECKED);
			}
			// Hex输入框编辑事件
			else if (cmd == 4001 && event == EN_CHANGE)
			{
				if (isUpdatingHex) return 0;
				
				char hexBuf[10];
				GetWindowTextA(hEditHex, hexBuf, sizeof(hexBuf));
				string inputHex(hexBuf);
				
				// 过滤非法字符（仅保留0-9、a-f，最多6位）
				string filteredHex;
				for (char c : inputHex)
				{
					if (isxdigit(c) && filteredHex.length() < 6)
					{
						filteredHex += tolower(c);
					}
				}
				
				// 修正输入框文本
				isUpdatingHex = true;
				SetWindowTextA(hEditHex, filteredHex.c_str());
				int len = filteredHex.length();
				SendMessage(hEditHex, EM_SETSEL, (WPARAM)len, (LPARAM)len);
				isUpdatingHex = false;
				
				// 合法Hex则更新RGB
				if (filteredHex.length() == 6)
				{
					int newR, newG, newB;
					if (hexToRgb(filteredHex, newR, newG, newB))
					{
						r = newR;
						g = newG;
						b = newB;
						SendMessage(hSliderR, TBM_SETPOS, TRUE, r);
						SendMessage(hSliderG, TBM_SETPOS, TRUE, g);
						SendMessage(hSliderB, TBM_SETPOS, TRUE, b);
						UpdateMaskWindow();
					}
				}
			}
			// 按钮点击事件（先处理全部控制按钮，再处理此屏控制按钮）
			else if (event == BN_CLICKED)
			{
				// 关闭全部遮罩/打开遮罩按钮
				if (cmd == 5003)
				{
					bool allVisible = IsAllMasksVisible();
					const char* newBtnText = NULL;
					
					if (allVisible)
					{
						// 所有遮罩均显示 → 全部隐藏
						for (const auto& info : hMaskWnds)
						{
							if (info.hWnd != NULL)
								ShowWindow(info.hWnd, SW_HIDE);
						}
						newBtnText = "打开全部遮罩";
					}
					else
					{
						// 存在遮罩未显示 → 全部显示
						for (const auto& info : hMaskWnds)
						{
							if (info.hWnd == NULL) continue;
							
							ShowWindow(info.hWnd, SW_SHOWNOACTIVATE);
							// 恢复置顶状态（若开启）
							if (isTopmost)
							{
								SetWindowPos(info.hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
							}
						}
						UpdateMaskWindow(); // 刷新颜色，避免默认白色
						newBtnText = "关闭全部遮罩";
					}
					
					// 更新按钮文本
					if (newBtnText != NULL && hBtnToggleAllMasks != NULL)
					{
						SetWindowText(hBtnToggleAllMasks, newBtnText);
					}
				}
				// 此屏遮罩控制按钮
				else
				{
					HWND hMaskWnd = findCurrentMonitorMaskWnd();
					if (hMaskWnd == NULL) return 0;
					
					if (cmd == 5001)   // 关闭此屏遮罩
					{
						ShowWindow(hMaskWnd, SW_HIDE);
						// 同步更新全部控制按钮文本（若所有窗口都隐藏）
						if (hBtnToggleAllMasks != NULL)
						{
							SetWindowText(hBtnToggleAllMasks, IsAllMasksVisible() ? "关闭全部遮罩" : "打开全部遮罩");
						}
					}
					else if (cmd == 5002)     // 打开此屏遮罩
					{
						ShowWindow(hMaskWnd, SW_SHOWNOACTIVATE);
						if (isTopmost)
						{
							SetWindowPos(hMaskWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
						}
						UpdateMaskWindow();
						// 同步更新全部控制按钮文本（若所有窗口都显示）
						if (hBtnToggleAllMasks != NULL)
						{
							SetWindowText(hBtnToggleAllMasks, IsAllMasksVisible() ? "关闭全部遮罩" : "打开全部遮罩");
						}
					}
				}
			}
			return 0;
		}
		
		// 定时器事件（自动刷新置顶）
	case WM_TIMER:
		{
			if (wParam == TIMER_AUTO_REFRESH && isTopmost && isAutoRefreshTopmost)
			{
				for (const auto& info : hMaskWnds)
				{
					HWND hMaskWnd = info.hWnd;
					if (hMaskWnd == NULL || !IsWindowVisible(hMaskWnd)) continue;
					SetWindowPos(hMaskWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				}
			}
			return 0;
		}
		
		// 窗口销毁
	case WM_DESTROY:
		KillTimer(hwnd, TIMER_AUTO_REFRESH);
		for (const auto& info : hMaskWnds)
		{
			if (info.hWnd != NULL) DestroyWindow(info.hWnd);
		}
		PostQuitMessage(0);
		return 0;
		
	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

// 程序入口
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// 隐藏控制台窗口
	HWND hConsoleWnd = GetConsoleWindow();
	if (hConsoleWnd != NULL)
	{
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
	if (!RegisterClassEx(&maskWc))
	{
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
	if (!RegisterClassEx(&mainWc))
	{
		MessageBox(NULL, "主窗口类注册失败！", "错误", MB_ICONERROR);
		UnregisterClass(maskWc.lpszClassName, hInstance);
		return 1;
	}
	
	// 3. 创建初始遮罩窗口（默认全部开启）
	RecreateMaskWindows();
	
	// 4. 创建主窗口
	DWORD mainExStyle = WS_EX_APPWINDOW;
	hMainWnd = CreateWindowEx(
							  mainExStyle,
							  "MainWindowClass", MAIN_WINDOW_TITLE,
							  WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
							  CW_USEDEFAULT, CW_USEDEFAULT, 400, 450,
							  NULL, NULL, hInstance, NULL
							  );
	if (hMainWnd == NULL)
	{
		MessageBox(NULL, "主窗口创建失败！", "错误", MB_ICONERROR);
		UnregisterClass(maskWc.lpszClassName, hInstance);
		UnregisterClass(mainWc.lpszClassName, hInstance);
		return 1;
	}
	
	// 5. 显示主窗口并启动消息循环
	ShowWindow(hMainWnd, nCmdShow);
	UpdateWindow(hMainWnd);
	
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	
	// 6. 程序退出：注销窗口类
	UnregisterClass("MaskWindowClass", hInstance);
	UnregisterClass("MainWindowClass", hInstance);
	return 0;
}

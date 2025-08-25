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

// 新增：遮罩窗口信息结构体（关联窗口句柄与对应显示器）
struct MaskWindowInfo
{
	HWND hWnd;         // 遮罩窗口句柄
	HMONITOR hMonitor; // 对应显示器句柄
};

// 用于传递遮罩窗口列表和序号的结构体（适配标题编号）
struct MonitorData
{
	vector<MaskWindowInfo>* maskWnds; // 改为遮罩窗口信息列表（原HWND列表）
	int windowIndex;                  // 遮罩窗口序号（1开始，主显示器为1）
};

// 函数前向声明（全局作用域）
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData);
LRESULT CALLBACK MaskWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void UpdateMaskWindow();
int clamp(int val, int min, int max);
void RecreateMaskWindows();
// 新增：辅助函数
string getCurrentHex();                // 根据RGB生成6位小写Hex字符串
bool hexToRgb(const string& hex, int& r, int& g, int& b); // Hex转RGB
HWND findCurrentMonitorMaskWnd();      // 找到GUI所在显示器的遮罩窗口

// 全局变量
vector<MaskWindowInfo> hMaskWnds;     // 所有遮罩窗口信息（原HWND列表）
HWND hMainWnd;                        // 主窗口句柄
HWND hSliderR, hSliderG, hSliderB, hSliderA; // 滑块控件
HWND hStaticR, hStaticG, hStaticB, hStaticA; // 数值显示文本
HWND hwndPreview;                     // 颜色预览区
HWND hCheckTopmost;                   // 完全置顶复选框
HWND hCheckMainOnly;                  // 仅主显示器复选框
HWND hCheckAutoRefresh;               // 自动刷新置顶复选框
HWND hStaticCopyright;                // 版权信息文本控件（修改：与RGB显示控件样式一致）
// 新增：功能1文本框、功能2按钮 + Hex标签（修改1新增）
HWND hStaticHexLabel;                 // Hex标签控件（显示"HEX: #"）
HWND hEditHex;                        // Hex颜色输入框
HWND hBtnCloseMask;                   // 关闭此屏遮罩按钮
HWND hBtnOpenMask;                    // 打开此屏遮罩按钮
bool isUpdatingHex = false;           // 防Hex输入框循环更新标志

int r = 255, g = 255, b = 255; // RGB颜色（初始白色）
int alpha = 0;                 // 透明度（0-100）
bool isTopmost = true;         // 是否完全置顶（高于任务栏）
bool isMainOnly = false;       // 是否仅主显示器
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

// 新增：根据当前RGB生成6位小写Hex字符串（无#和Alpha）
string getCurrentHex()
{
	char buffer[7];
	sprintf(buffer, "%02x%02x%02x", r, g, b); // 格式化为6位小写Hex
	return string(buffer);
}

// 新增：Hex字符串转RGB（返回是否转换成功，成功则更新r/g/b引用）
bool hexToRgb(const string& hex, int& r, int& g, int& b)
{
	// 必须是6位字符
	if (hex.length() != 6) return false;
	// 检查所有字符是否为合法Hex（0-9、a-f）
	for (char c : hex)
	{
		if (!isxdigit(c)) return false;
	}
	// 解析RGB值（16进制转10进制）
	r = strtol(hex.substr(0, 2).c_str(), NULL, 16);
	g = strtol(hex.substr(2, 2).c_str(), NULL, 16);
	b = strtol(hex.substr(4, 2).c_str(), NULL, 16);
	// 确保RGB在0-255范围内
	r = clamp(r, 0, 255);
	g = clamp(g, 0, 255);
	b = clamp(b, 0, 255);
	return true;
}

// 新增：找到GUI主窗口所在显示器对应的遮罩窗口句柄
HWND findCurrentMonitorMaskWnd()
{
	if (hMainWnd == NULL || hMaskWnds.empty()) return NULL;

	// 1. 获取主窗口的屏幕位置
	RECT mainRect;
	if (!GetWindowRect(hMainWnd, &mainRect)) return NULL;

	// 2. 找到主窗口所在的显示器
	HMONITOR hCurrentMonitor = MonitorFromRect(&mainRect, MONITOR_DEFAULTTONEAREST);
	if (hCurrentMonitor == NULL) return NULL;

	// 3. 匹配遮罩窗口列表中对应显示器的遮罩窗口
	for (const auto& info : hMaskWnds)
	{
		if (info.hMonitor == hCurrentMonitor)
		{
			return info.hWnd;
		}
	}
	return NULL;
}

// 更新遮罩颜色/透明度 + 控件显示（新增：同步更新Hex输入框）
void UpdateMaskWindow()
{
	// 1. 更新所有遮罩窗口
	for (const auto& info : hMaskWnds)
	{
		HWND hMaskWnd = info.hWnd;
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

	// 新增：4. 同步更新Hex输入框（防循环更新）
	if (hEditHex != NULL && !isUpdatingHex)
	{
		isUpdatingHex = true; // 开启更新标志，避免触发EN_CHANGE循环
		string hex = getCurrentHex();
		SetWindowTextA(hEditHex, hex.c_str());
		// 修复错误1：设置光标到文本末尾（避免输入顺序反转）
		int len = hex.length();
		SendMessage(hEditHex, EM_SETSEL, (WPARAM)len, (LPARAM)len);
		isUpdatingHex = false; // 关闭标志
	}
}

// 重新创建所有遮罩窗口（修改：适配MaskWindowInfo列表）
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

	// 短暂延迟确保窗口资源释放
	Sleep(20);

	// 初始化遮罩窗口序号（主显示器为1，其余按顺序2/3/4...）
	MonitorData monitorData;
	monitorData.maskWnds = &hMaskWnds;
	monitorData.windowIndex = 1; // 序号从1开始

	// 2. 重新创建遮罩窗口（传递带序号的结构体）
	EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitorData));

	// 3. 应用当前颜色和透明度设置
	UpdateMaskWindow();
}

// 显示器枚举回调（修改：适配MaskWindowInfo，按序号生成标题）
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
	// 解析传递的结构体（包含窗口列表和序号）
	MonitorData* pMonitorData = reinterpret_cast<MonitorData*>(dwData);
	if (pMonitorData == NULL || pMonitorData->maskWnds == NULL) return FALSE;

	vector<MaskWindowInfo>* maskWnds = pMonitorData->maskWnds;
	int& windowIndex = pMonitorData->windowIndex; // 引用序号，方便修改
	HINSTANCE hInstance = GetModuleHandle(NULL);

	// 1. 获取显示器信息
	MONITORINFOEX monitorInfo = {0};
	monitorInfo.cbSize = sizeof(MONITORINFOEX);
	if (!GetMonitorInfo(hMonitor, &monitorInfo))
	{
		windowIndex++; // 即使获取信息失败，序号也递增（避免重复）
		return TRUE;
	}

	// 2. 仅主显示器：跳过非主显示器
	if (isMainOnly && !(monitorInfo.dwFlags & MONITORINFOF_PRIMARY))
	{
		windowIndex++;
		return TRUE;
	}

	// 3. 确定窗口区域
	RECT windowRect;
	if (isTopmost)
	{
		// 置顶：使用全屏区域（覆盖任务栏）
		windowRect = monitorInfo.rcMonitor;
	}
	else
	{
		// 非置顶：使用工作区（排除任务栏）
		SystemParametersInfo(SPI_GETWORKAREA, 0, &windowRect, 0);
		IntersectRect(&windowRect, &windowRect, &monitorInfo.rcMonitor);
	}

	// 4. 计算窗口宽高
	int winWidth = windowRect.right - windowRect.left;
	int winHeight = windowRect.bottom - windowRect.top;
	if (winWidth <= 0 || winHeight <= 0)
	{
		windowIndex++;
		return TRUE;
	}

	// 生成遮罩窗口标题（主显示器为“遮罩窗口-1”，其余按序号）
	char maskWindowTitle[50];
	sprintf(maskWindowTitle, "遮罩窗口-%d", windowIndex);
	// 主显示器特殊处理（确保主显示器一定是“遮罩窗口-1”）
	if (monitorInfo.dwFlags & MONITORINFOF_PRIMARY)
	{
		sprintf(maskWindowTitle, "遮罩窗口-1");
		windowIndex = 2; // 主显示器用了1，下一个从2开始
	}
	else
	{
		windowIndex++; // 非主显示器序号递增
	}

	// 5. 创建遮罩窗口（使用生成的标题）
	DWORD exStyle = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW;
	if (isTopmost)
	{
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

	if (hMaskWnd == NULL)
	{
		return TRUE;
	}

	// 6. 设置窗口属性
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
	ShowWindow(hMaskWnd, SW_SHOWNOACTIVATE);

	// 7. 新增：将窗口句柄与显示器句柄关联，加入信息列表
	MaskWindowInfo info;
	info.hWnd = hMaskWnd;
	info.hMonitor = hMonitor;
	maskWnds->push_back(info);

	return TRUE;
}

// 主窗口消息处理（修改：界面调整+新增控件处理）
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		// 窗口创建：初始化所有控件（修改+新增）
		case WM_CREATE:
		{
			// 1. 注册定时器
			SetTimer(hwnd, TIMER_AUTO_REFRESH, 1000, NULL);

			// 修改1：版权信息控件（与RGB数值显示控件样式一致）
			hStaticCopyright = CreateWindow(
			                       "STATIC", COPYRIGHT_TEXT,
			                       WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
			                       50, 360, 300, 20,  // 移至窗口底部，尺寸与RGB显示控件一致
			                       hwnd, NULL, GetModuleHandle(NULL), NULL
			                   );
			// 移除自定义字体，使用默认字体（与hStaticR等一致）

			// 2. 创建RGB滑块（位置不变，适配后续控件）
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

			// 3. 透明度滑块
			hSliderA = CreateWindow(
			               TRACKBAR_CLASS, "Alpha",
			               WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
			               50, 160, 200, 30,
			               hwnd, (HMENU)1004, GetModuleHandle(NULL), NULL
			           );
			SendMessage(hSliderA, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
			SendMessage(hSliderA, TBM_SETPOS, TRUE, alpha);

			// 4. 数值显示文本
			hStaticR = CreateWindow("STATIC", "R: 255", WS_CHILD | WS_VISIBLE | SS_LEFT, 260, 40, 80, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
			hStaticG = CreateWindow("STATIC", "G: 255", WS_CHILD | WS_VISIBLE | SS_LEFT, 260, 80, 80, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
			hStaticB = CreateWindow("STATIC", "B: 255", WS_CHILD | WS_VISIBLE | SS_LEFT, 260, 120, 80, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);
			hStaticA = CreateWindow("STATIC", "A: 0%",  WS_CHILD | WS_VISIBLE | SS_LEFT, 260, 160, 80, 20, hwnd, NULL, GetModuleHandle(NULL), NULL);

			// 5. 颜色预览区
			hwndPreview = CreateWindow(
			                  "STATIC", "",
			                  WS_CHILD | WS_VISIBLE | SS_WHITERECT,
			                  100, 200, 200, 60,
			                  hwnd, NULL, GetModuleHandle(NULL), NULL
			              );

			// 修改2：调换“完全置顶”和“仅主显示器”位置
			hCheckMainOnly = CreateWindow( // 原“仅主显示器”移至左侧（x50）
			                     "BUTTON", "仅主显示器",
			                     WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
			                     50, 270, 100, 20,
			                     hwnd, (HMENU)3002, GetModuleHandle(NULL), NULL
			                 );

			hCheckTopmost = CreateWindow( // 原“完全置顶”移至右侧（x160）
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
			CheckDlgButton(hwnd, 3002, BST_UNCHECKED); // 仅主显示器默认不勾选
			CheckDlgButton(hwnd, 3003, BST_UNCHECKED); // 自动刷新默认不勾选
			EnableWindow(hCheckAutoRefresh, isTopmost);

			// --------------------------
			// 修改1：新增Hex标签（左侧显示"HEX: #"）+ 调整文本框位置到右侧
			// --------------------------
			hStaticHexLabel = CreateWindow(
			                      "STATIC", "HEX: #",
			                      WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
			                      50, 300, 50, 25,  // 左侧标签：x50, y300（与文本框同高）
			                      hwnd, NULL, GetModuleHandle(NULL), NULL
			                  );

			// 新增1：Hex颜色输入框（移至右侧，与标签对齐）
			hEditHex = CreateWindowA(
			               "EDIT", "",
			               WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NOHIDESEL,
			               110, 300, 80, 25,  // 右侧文本框：x110（标签右侧+10间距），宽度80
			               hwnd, (HMENU)4001, GetModuleHandle(NULL), NULL
			           );
			// 初始显示当前Hex值
			string initHex = getCurrentHex();
			SetWindowTextA(hEditHex, initHex.c_str());
			// 初始设置光标到末尾
			SendMessage(hEditHex, EM_SETSEL, (WPARAM)initHex.length(), (LPARAM)initHex.length());

			// 新增2：遮罩控制按钮（Hex输入框下一行）
			hBtnCloseMask = CreateWindow(
			                    "BUTTON", "关闭此屏遮罩",
			                    WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			                    50, 330, 120, 25,  // 左侧按钮
			                    hwnd, (HMENU)5001, GetModuleHandle(NULL), NULL
			                );

			hBtnOpenMask = CreateWindow(
			                   "BUTTON", "打开此屏遮罩",
			                   WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			                   200, 330, 120, 25,  // 右侧按钮（与文本框不重叠）
			                   hwnd, (HMENU)5002, GetModuleHandle(NULL), NULL
			               );

			return 0;
		}

		// --------------------------
		// 修改2：新增WM_PAINT消息，绘制Hex文本框的灰色边框
		// --------------------------
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);

			// 仅当文本框存在且可见时绘制边框
			if (hEditHex != NULL && IsWindowVisible(hEditHex))
			{
				RECT editRect;
				// 1. 获取文本框的屏幕坐标
				GetWindowRect(hEditHex, &editRect);
				// 2. 转换为当前主窗口的客户区坐标
				ScreenToClient(hwnd, (LPPOINT)&editRect.left);  // 转换左上角
				ScreenToClient(hwnd, (LPPOINT)&editRect.right); // 转换右下角

				// 3. 计算边框矩形（比文本框大1像素）
				RECT borderRect =
				{
					editRect.left - 1,   // 左移1像素
					editRect.top - 1,    // 上移1像素
					editRect.right + 1,  // 右移1像素
					editRect.bottom + 1  // 下移1像素
				};

				// 4. 创建127,127,127的灰色刷子并绘制边框
				HBRUSH hBorderBrush = CreateSolidBrush(RGB(127, 127, 127));
				FillRect(hdc, &borderRect, hBorderBrush);
				DeleteObject(hBorderBrush); // 释放资源
			}

			EndPaint(hwnd, &ps);
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}

		// 滑块拖动：更新颜色/透明度
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

		// 控件命令：复选框/输入框/按钮事件（新增：Hex输入框和按钮处理）
		case WM_COMMAND:
		{
			WORD cmd = LOWORD(wParam);
			WORD event = HIWORD(wParam);

			// 复选框事件
			if (cmd == 3001)   // 完全置顶
			{
				isTopmost = (IsDlgButtonChecked(hwnd, 3001) == BST_CHECKED);
				EnableWindow(hCheckAutoRefresh, isTopmost);
				RecreateMaskWindows();
			}
			else if (cmd == 3002)     // 仅主显示器
			{
				isMainOnly = (IsDlgButtonChecked(hwnd, 3002) == BST_CHECKED);
				RecreateMaskWindows();
			}
			else if (cmd == 3003)     // 自动刷新置顶
			{
				isAutoRefreshTopmost = (IsDlgButtonChecked(hwnd, 3003) == BST_CHECKED);
			}
			// 新增：Hex输入框编辑事件（EN_CHANGE）
			else if (cmd == 4001 && event == EN_CHANGE)
			{
				if (isUpdatingHex) return 0; // 忽略内部更新触发的事件

				// 1. 获取输入框文本
				char hexBuf[10];
				GetWindowTextA(hEditHex, hexBuf, sizeof(hexBuf));
				string inputHex(hexBuf);

				// 2. 过滤非法字符和超长内容（仅保留0-9、a-f，最多6位）
				string filteredHex;
				for (char c : inputHex)
				{
					if (isxdigit(c) && filteredHex.length() < 6)
					{
						filteredHex += tolower(c); // 转为小写统一格式
					}
				}

				// 3. 修正输入框文本（删除非法/超长字符）
				isUpdatingHex = true;
				SetWindowTextA(hEditHex, filteredHex.c_str());
				// 修复错误1：强制光标到文本末尾（核心修复点）
				int len = filteredHex.length();
				SendMessage(hEditHex, EM_SETSEL, (WPARAM)len, (LPARAM)len);
				isUpdatingHex = false;

				// 4. 若过滤后为6位合法Hex，更新RGB和滑块
				if (filteredHex.length() == 6)
				{
					int newR, newG, newB;
					if (hexToRgb(filteredHex, newR, newG, newB))
					{
						r = newR;
						g = newG;
						b = newB;
						// 更新滑块位置（同步显示）
						SendMessage(hSliderR, TBM_SETPOS, TRUE, r);
						SendMessage(hSliderG, TBM_SETPOS, TRUE, g);
						SendMessage(hSliderB, TBM_SETPOS, TRUE, b);
						// 刷新颜色
						UpdateMaskWindow();
					}
				}
			}
			// 新增：遮罩控制按钮事件（BN_CLICKED）
			else if (event == BN_CLICKED)
			{
				HWND hMaskWnd = findCurrentMonitorMaskWnd();
				if (hMaskWnd == NULL) return 0;

				if (cmd == 5001)   // 关闭此屏遮罩
				{
					ShowWindow(hMaskWnd, SW_HIDE);
				}
				else if (cmd == 5002)     // 打开此屏遮罩
				{
					ShowWindow(hMaskWnd, SW_SHOWNOACTIVATE);
					// 若开启置顶，恢复置顶状态
					if (isTopmost)
					{
						SetWindowPos(hMaskWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
					}
					// 修复错误2：重新绘制当前遮罩窗口颜色（避免默认白色）
					UpdateMaskWindow();
				}
			}
			return 0;
		}

		// 定时器：1秒刷新一次置顶
		case WM_TIMER:
		{
			if (wParam == TIMER_AUTO_REFRESH && isTopmost && isAutoRefreshTopmost)
			{
				for (const auto& info : hMaskWnds)
				{
					HWND hMaskWnd = info.hWnd;
					if (hMaskWnd == NULL) continue;
					SetWindowPos(hMaskWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				}
			}
			return 0;
		}

		// 窗口销毁：清理资源
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

// 程序入口（修改：主窗口高度适配新增控件）
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// 隐藏控制台窗口（若存在）
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

	// 3. 创建初始遮罩窗口
	RecreateMaskWindows();

	// 4. 创建主窗口（高度增加至450，适配新增的Hex输入框和按钮）
	DWORD mainExStyle = WS_EX_APPWINDOW;
	hMainWnd = CreateWindowEx(
	               mainExStyle,
	               "MainWindowClass", MAIN_WINDOW_TITLE,
	               WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
	               CW_USEDEFAULT, CW_USEDEFAULT, 400, 450,  // 高度从420→450，适配底部版权信息
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

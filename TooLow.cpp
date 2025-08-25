#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#include <bits/stdc++.h>
#define i_input _color(14);printf(">");_color();

using namespace std;

const char* WINDOW_TITLE = "窗口 颜色遮罩";  // 遮罩窗口标题
const char* CONSOLE_TITLE = "控制台 太Low - 屏幕颜色遮罩 v1.0";  // 控制台窗口标题

void _color(int __c = 7)
{
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), __c);
}

HWND hWnd;
int r = 0, g = 0, b = 0;
BYTE alpha = 0;  // 透明度值（0-255）

// 窗口消息处理函数
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_DESTROY:
			PostQuitMessage(0);  // 窗口销毁时退出消息循环
			return 0;
		default:
			return DefWindowProc(hwnd, uMsg, wParam, lParam);  // 默认消息处理
	}
}

// 数值范围限制函数
int clamp(int val, int min, int max)
{
	return val < min ? min : (val > max ? max : val);
}

// 输入处理线程（调节颜色和透明度）
unsigned int __stdcall InputThread(void* param)
{
	char line[100];
	while (1)
	{
		printf("\n输入RGB(0-255)和Alpha(0-100, 100为完全不透明)\n格式: ");
		_color(207);
		cout << "R";
		_color();  // 彩色提示
		cout << " " ;
		_color(175);
		cout << "G";
		_color();
		cout << " " ;
		_color(159);
		cout << "B";
		_color();
		printf(" A, 输入q退出\n");

		i_input;
		if (!fgets(line, sizeof(line), stdin))    // 读取输入
		{
			PostMessage(hWnd, WM_CLOSE, 0, 0);  // 关闭窗口
			break;
		}

		line[strcspn(line, "\n")] = 0;  // 清除换行符

		if (!strcmp(line, "q"))    // 输入q退出
		{
			PostMessage(hWnd, WM_CLOSE, 0, 0);
			break;
		}

		int r_val, g_val, b_val, a_val;
		if (sscanf(line, "%d %d %d %d", &r_val, &g_val, &b_val, &a_val) == 4)
		{
			// 范围限制
			r = clamp(r_val, 0, 255);
			g = clamp(g_val, 0, 255);
			b = clamp(b_val, 0, 255);
			a_val = clamp(a_val, 0, 100);

			// 设置透明度（0-100转换为0-255）
			SetLayeredWindowAttributes(hWnd, 0, (BYTE)(a_val * 2.55f), LWA_ALPHA);

			// 更新窗口背景色
			HDC hdc = GetDC(hWnd);
			HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
			RECT rect;
			GetClientRect(hWnd, &rect);
			FillRect(hdc, &rect, hBrush);
			ReleaseDC(hWnd, hdc);
			DeleteObject(hBrush);
		}
		else
		{
			printf("格式错误, 示例：0 0 0 50(黑色50%透明度)\n");
		}
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	SetConsoleTitle(CONSOLE_TITLE);  // 设置控制台标题

	// 程序启动提示
	puts("太Low - 屏幕颜色遮罩 v1.0");
	puts("by IQ Online Studio, github.com/iqonli");
	puts("功能: 通过半透明置顶遮罩窗口实现屏幕颜色和亮度调节");
	puts("操作: 输入R G B Alpha调节颜色(RGB:0-255, Alpha:0-100), 输入q退出");

	// 注册窗口类
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0, 0, hInstance };
	wc.lpszClassName = "BrightnessWindow";
	RegisterClassEx(&wc);

	// 创建全屏置顶窗口
	RECT rect;
	SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
	hWnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT,
	                      wc.lpszClassName, WINDOW_TITLE, WS_POPUP,
	                      rect.left, rect.top, rect.right-rect.left, rect.bottom-rect.top,
	                      NULL, NULL, hInstance, NULL);

	SetLayeredWindowAttributes(hWnd, 0, 0, LWA_ALPHA);  // 初始完全透明
	ShowWindow(hWnd, SW_SHOWNOACTIVATE);  // 显示窗口

	// 创建输入处理线程
	HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, InputThread, NULL, 0, NULL);
	CloseHandle(hThread);  // 释放线程句柄

	// 消息循环
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	UnregisterClass(wc.lpszClassName, hInstance);  // 清理资源
	return 0;
}


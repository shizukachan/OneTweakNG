#include "stdafx.h"

#include "Wrapper.h"

#include "Context.h"
#include "IDirect3D9.h"

MainContext context;

Config::Config()
{
	std::string inifile = FullPathFromPath(inifilename);

	CSimpleIniW ini;
	ini.LoadFile(inifile.c_str());

	u32 config_version = ini.GetLongValue(L"Version", L"Config");
	if (config_version != CONFIG_VERSION)
	{
		// save file and reload
		ini.Reset();

#define SETTING(_type, _func, _var, _section, _defaultval) \
	ini.Set##_func(L#_section, L#_var, _defaultval)
#include "Settings.h"
#undef SETTING

		ini.SetLongValue(L"Version", L"Config", CONFIG_VERSION);
		ini.SaveFile(inifile.c_str());
		ini.Reset();
		ini.LoadFile(inifile.c_str());
	}

#define SETTING(_type, _func, _var, _section, _defaultval) \
	_var = ini.Get##_func(L#_section, L#_var)
#include "Settings.h"
#undef SETTING
}

MainContext::MainContext()
{
	autofix = (AutoFixes) 0;
	windowmode = (WindowMode) 0;
	LogFile("OneTweakNG.log");

	if(config.GetAutoFix()) EnableAutoFix();
	fixFlags = 0;
	theVertexBuffer = 0;
	MH_Initialize();

	MH_CreateHook(D3D9DLL::Get().Direct3DCreate9, HookDirect3DCreate9, reinterpret_cast<void**>(&TrueDirect3DCreate9));
	MH_EnableHook(D3D9DLL::Get().Direct3DCreate9);

	MH_CreateHook(CreateWindowExA, HookCreateWindowExA, reinterpret_cast<void**>(&TrueCreateWindowExA));
	MH_EnableHook(CreateWindowExA);

	MH_CreateHook(CreateWindowExW, HookCreateWindowExW, reinterpret_cast<void**>(&TrueCreateWindowExW));
	MH_EnableHook(CreateWindowExW);

	MH_CreateHook(SetWindowLongA, HookSetWindowLongA, reinterpret_cast<void**>(&TrueSetWindowLongA));
	MH_EnableHook(SetWindowLongA);

	MH_CreateHook(SetWindowLongW, HookSetWindowLongW, reinterpret_cast<void**>(&TrueSetWindowLongW));
	MH_EnableHook(SetWindowLongW);
}

MainContext::~MainContext()
{
	while (::ShowCursor(TRUE) <= 0);
}

IDirect3D9 * WINAPI MainContext::HookDirect3DCreate9(UINT SDKVersion)
{
	IDirect3D9* d3d9 = context.TrueDirect3DCreate9(SDKVersion);
	if (d3d9)
	{
		return new hkIDirect3D9(d3d9);
	}

	return d3d9;
}

bool MainContext::BehaviorFlagsToString(DWORD BehaviorFlags, std::string* BehaviorFlagsString)
{
#define BF2STR(x) if (BehaviorFlags & x) BehaviorFlagsString->append(#x" ");

	BF2STR(D3DCREATE_ADAPTERGROUP_DEVICE);
	BF2STR(D3DCREATE_DISABLE_DRIVER_MANAGEMENT);
	BF2STR(D3DCREATE_DISABLE_DRIVER_MANAGEMENT_EX);
	BF2STR(D3DCREATE_DISABLE_PRINTSCREEN);
	BF2STR(D3DCREATE_DISABLE_PSGP_THREADING);
	BF2STR(D3DCREATE_ENABLE_PRESENTSTATS);
	BF2STR(D3DCREATE_FPU_PRESERVE);
	BF2STR(D3DCREATE_HARDWARE_VERTEXPROCESSING);
	BF2STR(D3DCREATE_MIXED_VERTEXPROCESSING);
	BF2STR(D3DCREATE_SOFTWARE_VERTEXPROCESSING);
	BF2STR(D3DCREATE_MULTITHREADED);
	BF2STR(D3DCREATE_NOWINDOWCHANGES);
	BF2STR(D3DCREATE_PUREDEVICE);
	BF2STR(D3DCREATE_SCREENSAVER);

#undef BF2STR

	if (BehaviorFlagsString->back() == ' ')
		BehaviorFlagsString->pop_back();

	return false;
}

bool MainContext::ApplyPresentationParameters(D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	if (pPresentationParameters)
	{
		if (config.GetTrippleBuffering())
		{
			pPresentationParameters->BackBufferCount = 3;
		}

		if (config.GetMultisample() > 0)
		{
			pPresentationParameters->SwapEffect = D3DSWAPEFFECT_DISCARD;
			pPresentationParameters->MultiSampleType = (D3DMULTISAMPLE_TYPE)config.GetMultisample();
			pPresentationParameters->MultiSampleQuality = 0;

			PrintLog("MultiSampleType %u, MultiSampleQuality %u", pPresentationParameters->MultiSampleType, pPresentationParameters->MultiSampleQuality);
		}

		if (config.GetPresentationInterval() != -1)
		{
			pPresentationParameters->PresentationInterval = config.GetPresentationInterval();
			PrintLog("PresentationInterval: PresentationInterval set to %u", pPresentationParameters->PresentationInterval);
		}

		if (config.GetBorderless())
		{
			if (pPresentationParameters->Windowed != TRUE)
			{
				windowmode = BORDERLESS;
				int cx = GetSystemMetrics(SM_CXSCREEN);
				int cy = GetSystemMetrics(SM_CYSCREEN);

				LONG_PTR dwStyle = GetWindowLongPtr(pPresentationParameters->hDeviceWindow, GWL_STYLE);
				LONG_PTR dwExStyle = GetWindowLongPtr(pPresentationParameters->hDeviceWindow, GWL_EXSTYLE);

				DWORD new_dwStyle = dwStyle & ~WS_OVERLAPPEDWINDOW;
				DWORD new_dwExStyle = dwExStyle & ~(WS_EX_OVERLAPPEDWINDOW);

				context.TrueSetWindowLongW(pPresentationParameters->hDeviceWindow, GWL_STYLE, new_dwStyle);
				context.TrueSetWindowLongW(pPresentationParameters->hDeviceWindow, GWL_EXSTYLE, new_dwExStyle);

				SetWindowPos(pPresentationParameters->hDeviceWindow, HWND_TOP, 0, 0, cx, cy, SWP_SHOWWINDOW | SWP_NOCOPYBITS | SWP_NOSENDCHANGING);

				PrintLog("pPresentationParameters->hDeviceWindow 0x%p: Borderless dwStyle: %lX->%lX", pPresentationParameters->hDeviceWindow, dwStyle, new_dwStyle);
				PrintLog("pPresentationParameters->hDeviceWindow 0x%p: Borderless dwExStyle: %lX->%lX", pPresentationParameters->hDeviceWindow, dwExStyle, new_dwExStyle);
				MessageBeep(MB_ICONASTERISK);

				if (config.GetForceWindowedMode())
				{
					pPresentationParameters->SwapEffect = pPresentationParameters->MultiSampleType == D3DMULTISAMPLE_NONE ? D3DSWAPEFFECT_DISCARD : D3DSWAPEFFECT_FLIP;
					pPresentationParameters->Windowed = TRUE;
					pPresentationParameters->FullScreen_RefreshRateInHz = 0;
					PrintLog("ForceWindowedMode");
				}
			}
			else
			{
				PrintLog("AlreadyWindowedMode");
move_windowed:
				windowmode = WINDOWED;
				PrintLog("DX CreateDevice: %ux%u",pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight);
				RECT rec;
				GetWindowRect(pPresentationParameters->hDeviceWindow,&rec);
				int cx = GetSystemMetrics(SM_CXSCREEN);
				int cy = GetSystemMetrics(SM_CYSCREEN);
				cy = (cy - (rec.bottom - rec.top))/2;
				cx = (cx - (rec.right - rec.left))/2;
				SetWindowPos(pPresentationParameters->hDeviceWindow, HWND_TOP, cx, cy, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOCOPYBITS | SWP_NOSENDCHANGING);
			}
		}
		else
		{
			if (pPresentationParameters->Windowed == TRUE)
			{
				goto move_windowed;
			}
			else //if (pPresentationParameters->Windowed != TRUE)
				windowmode = FULLSCREEN;
		}

		PrintLog("Window Mode: %d",windowmode);

		if (config.GetHideCursor()) while (::ShowCursor(FALSE) >= 0); // ShowCursor < 0 -> hidden

		return true;
	}
	return false;
}

bool MainContext::CheckWindow(HWND hWnd)
{
	std::unique_ptr<wchar_t[]> className(new wchar_t[MAX_PATH]);
	std::unique_ptr<wchar_t[]> windowName(new wchar_t[MAX_PATH]);

	GetClassNameW(hWnd, className.get(), MAX_PATH);
	GetWindowTextW(hWnd, windowName.get(), MAX_PATH);
	
	PrintLog("HWND 0x%p: ClassName \"%ls\", WindowName: \"%ls\"", hWnd, className.get(), windowName.get());
	bool class_found = config.GetWindowClass().compare(className.get()) == 0;
	bool window_found = config.GetWindowName().compare(windowName.get()) == 0;
	bool force = config.GetAllWindows();

	return class_found || window_found || force;
}

void MainContext::ApplyWndProc(HWND hWnd)
{
	if (config.GetAlwaysActive())
	{
		context.oldWndProc = (WNDPROC)context.TrueSetWindowLongA(hWnd, GWLP_WNDPROC, (LONG_PTR)context.WindowProc);
	}
}

void MainContext::ApplyBorderless(HWND hWnd)
{
/*	if (config.GetBorderless())
	{
		LONG_PTR dwStyle = GetWindowLongPtr(hWnd, GWL_STYLE);
		LONG_PTR dwExStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);

		DWORD new_dwStyle = dwStyle & ~WS_OVERLAPPEDWINDOW;
		DWORD new_dwExStyle = dwExStyle & ~(WS_EX_OVERLAPPEDWINDOW);

		context.TrueSetWindowLongW(hWnd, GWL_STYLE, new_dwStyle);
		context.TrueSetWindowLongW(hWnd, GWL_EXSTYLE, new_dwExStyle);

		int cx = GetSystemMetrics(SM_CXSCREEN);
		int cy = GetSystemMetrics(SM_CYSCREEN);

		SetWindowPos(hWnd, HWND_TOP, 0, 0, cx, cy, SWP_SHOWWINDOW | SWP_NOCOPYBITS | SWP_NOSENDCHANGING);

		PrintLog("HWND 0x%p: Borderless dwStyle: %lX->%lX", hWnd, dwStyle, new_dwStyle);
		PrintLog("HWND 0x%p: Borderless dwExStyle: %lX->%lX", hWnd, dwExStyle, new_dwExStyle);
		MessageBeep(MB_ICONASTERISK);
	}*/
}

LRESULT CALLBACK MainContext::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{

	case WM_ACTIVATE:
		switch (LOWORD(wParam))
		{
		case WA_ACTIVE:
		case WA_CLICKACTIVE:
			while (::ShowCursor(FALSE) >= 0);
			break;

		case WA_INACTIVE:
			if (context.config.GetAlwaysActive())
				return TRUE;

			while (::ShowCursor(TRUE) < 0);
			break;
		}

	case WM_ACTIVATEAPP:
		if (context.config.GetAlwaysActive())
			return TRUE;

	}
	return CallWindowProc(context.oldWndProc, hWnd, uMsg, wParam, lParam);
}

LONG WINAPI MainContext::HookSetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong)
{
	if (context.config.GetBorderless())
	{
		DWORD olddwNewLong = dwNewLong;
		if (nIndex == GWL_STYLE)
		{
			dwNewLong &= ~WS_OVERLAPPEDWINDOW;
			PrintLog("SetWindowLongA dwStyle: %lX->%lX", olddwNewLong, dwNewLong);
		}

		if (nIndex == GWL_EXSTYLE)
		{
			dwNewLong &= ~(WS_EX_OVERLAPPEDWINDOW);
			PrintLog("SetWindowLongA dwExStyle: %lX->%lX", olddwNewLong, dwNewLong);
		}
	}
	return context.TrueSetWindowLongA(hWnd, nIndex, dwNewLong);
}

LONG WINAPI MainContext::HookSetWindowLongW(HWND hWnd, int nIndex, LONG dwNewLong)
{
	if (context.config.GetBorderless())
	{
		DWORD olddwNewLong = dwNewLong;
		if (nIndex == GWL_STYLE)
		{
			dwNewLong &= ~WS_OVERLAPPEDWINDOW;
			PrintLog("SetWindowLongW dwStyle: %lX->%lX", olddwNewLong, dwNewLong);
		}

		if (nIndex == GWL_EXSTYLE)
		{
			dwNewLong &= ~(WS_EX_OVERLAPPEDWINDOW);
			PrintLog("SetWindowLongW dwExStyle: %lX->%lX", olddwNewLong, dwNewLong);
		}
	}
	return context.TrueSetWindowLongW(hWnd, nIndex, dwNewLong);
}

HWND WINAPI MainContext::HookCreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
	HWND hWnd = context.TrueCreateWindowExA(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
	if (!hWnd)
	{
		PrintLog("CreateWindowExA failed");
		return hWnd;
	}

	if (context.CheckWindow(hWnd))
	{
		context.ApplyWndProc(hWnd);
		context.ApplyBorderless(hWnd);
	}

	return hWnd;
}

HWND WINAPI MainContext::HookCreateWindowExW(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam)
{
	HWND hWnd = context.TrueCreateWindowExW(dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
	if (!hWnd)
	{
		PrintLog("CreateWindowExW failed");
		return hWnd;
	}

	if (context.CheckWindow(hWnd))
	{
		context.ApplyWndProc(hWnd);
		context.ApplyBorderless(hWnd);
	}

	return hWnd;
}

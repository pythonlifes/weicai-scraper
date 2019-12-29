#include "stdafx.h"
#include "stdio.h"
#include "WndMsgLoop.h"
#include "HookOffset.h"
#include "MsgProtocol.h"
#include "LogRecord.h"
#include "WCProcess.h"
#include "HttpRequest.h"
#include "json.hpp"


#pragma comment(lib, "rpcrt4.lib")
#include <rpc.h>


using json = nlohmann::json;

using namespace std;

DWORD isRegisterWnd;
LPCWSTR WeChatHelper;

// 初始化消息循环窗口
void InitWindow(HMODULE hModule)
{
	LogRecord(L"InitWindow", ofs);

	UUID uuid;
	UuidCreate(&uuid);
	char *str;
	UuidToStringA(&uuid, (RPC_CSTR*)&str);

	char ty[1024] = { 0 };
	sprintf_s(ty, sizeof(ty), "%s%s", "WeChatHelper", "");//str);
	TCHAR tstr[1024] = TEXT("");
	CharToTchar(ty, tstr);
	WeChatHelper = (LPCWSTR)tstr;
	LogRecord(WeChatHelper, ofs);

	RpcStringFreeA((RPC_CSTR*)&str);

	RegisterWindow(hModule);
}

// 注册窗口及消息循环
void RegisterWindow(HMODULE hModule)
{
	LogRecord(L"RegisterWindowMsgLoop", ofs);

	//1  设计一个窗口类
	WNDCLASS wnd;
	wnd.style = CS_VREDRAW | CS_HREDRAW;	//风格
	wnd.lpfnWndProc = WndProc;	//窗口消息回调函数指针.
	wnd.cbClsExtra = NULL;
	wnd.cbWndExtra = NULL;
	wnd.hInstance = hModule;
	wnd.hIcon = NULL;
	wnd.hCursor = NULL;
	wnd.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = WeChatHelper;
	//2  注册窗口类
	RegisterClass(&wnd);
	//3  创建窗口
	HWND hWnd = CreateWindow(
		WeChatHelper,	//窗口类名
		WeChatHelper,	//窗口名
		WS_OVERLAPPEDWINDOW,	//窗口风格
		10, 10, 500, 300,	//窗口位置
		NULL,	//父窗口句柄
		NULL,	//菜单句柄
		hModule,	//实例句柄
		NULL	//传递WM_CREATE消息时的附加参数
	);


	SetTimer(hWnd, 1, 1000, RegisterWnd);

	//4  更新显示窗口
	ShowWindow(hWnd, SW_HIDE);
	UpdateWindow(hWnd);
	//5  消息循环（消息泵）
	MSG  msg = {};
	//	5.1获取消息
	while (GetMessage(&msg, 0, 0, 0))
	{
		//	5.2翻译消息
		TranslateMessage(&msg);
		//	5.3转发到消息回调函数
		DispatchMessage(&msg);
	}
}

// 服务信息注册
void  CALLBACK RegisterWnd(HWND   hwnd, UINT   uMsg, UINT   idEvent, DWORD   dwTime)
{
	// 注册成功
	if (isRegisterWnd) {
		KillTimer(hwnd, 1);
	}
	else {
		//控制窗口
		HWND hWeChatRoot = FindWindow(NULL, L"WeChatCtl");
		if (hWeChatRoot == NULL)
		{
			LogRecord(L"未查找到WeChatCtl窗口", ofs);
			return;
		}

		COPYDATASTRUCT chatmsg;
		chatmsg.dwData = WM_RegWeChatHelper;// 保存一个数值, 可以用来作标志等
		chatmsg.cbData = sizeof(WeChatHelper);// 待发送的数据的长
		chatmsg.lpData = (LPVOID)WeChatHelper;// 待发送的数据的起始地址
		SendMessage(hWeChatRoot, WM_COPYDATA, NULL, (LPARAM)&chatmsg);


		// 尝试注册
		json o;
		o["WeChatHelperName"] = string_To_UTF8(convLPCWSTRtoString(WeChatHelper));
		HttpRequest httpReq("127.0.0.1", 6877);
		std::string res = httpReq.HttpPost("/wechatRegister", o.dump());
		std::string body = httpReq.getBody(res);
		int code = 201;
		if (body != "") {
			auto bd = json::parse(body);
			code = bd["code"].get<int>();
		}

		if (code==200) {
			isRegisterWnd = true;
			LogRecord(L"注册成功", ofs);
		}
		else {
			LogRecord(L"注册失败", ofs);
		}
	}
}

// 窗口消息回调
LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	// 仅处理WM_COPYDATA类消息
	if (Message == WM_COPYDATA)
	{
		LogRecord(L"收到WM_COPYDATA类消息", ofs);
		COPYDATASTRUCT *pCopyData = (COPYDATASTRUCT*)lParam;
		LogRecord(L"switch type", ofs);
		switch (pCopyData->dwData)
		{
		case WM_CheckIsLogin: {
			LogRecord(L"收到WM_CheckIsLogin指令", ofs);
			CheckIsLogin();
			break;
		}
		case WM_HookReciveMsg: {
			LogRecord(L"收到WM_HookReciveMsg指令", ofs);
			HOOK_ReciveMsg();
			break;
		}
		case WM_HookAntiRevoke: {
			LogRecord(L"收到WM_HookAntiRevoke指令", ofs);
			HOOK_AntiRevoke();
			break;
		}
		case WM_ShowQrCode: {
			LogRecord(L"收到WM_ShowQrCode指令", ofs);
			WX_CallShowQrCode();
			HOOK_SaveQrCode();
			break;
		}
		default:

			char ty[34] = { 0 };
			TCHAR * tchar = { 0 };
			int iLength;

			sprintf_s(ty, sizeof(ty), "%d", pCopyData->dwData);
			iLength = MultiByteToWideChar(CP_ACP, 0, ty, strlen(ty) + 1, NULL, 0);
			MultiByteToWideChar(CP_ACP, 0, ty, strlen(ty) + 1, tchar, iLength);

			LogRecord(tchar, ofs);
			break;
		}
	}
	return DefWindowProc(hWnd, Message, wParam, lParam);
}

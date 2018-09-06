#include <string>
#include <cassert>
#include <memory>
#include <vector>

#include <Windows.h>
#include <tchar.h>

#include "../../lib/RtMidi.h"





using tstring = std::basic_string<TCHAR>;

#define ARRAYCOUNT(X) (sizeof(X) / sizeof(*(X)))




/** The main app window.
Contains an overridden window proc for handling custom messages. */
HWND gWndMain = NULL;

/** The edit window in the main app window.
This is where all the output goes. */
HWND gWndEdit = NULL;

/** gWndMain's original window proc. */
WNDPROC gOldWndProc = nullptr;

/** All listening MIDI IN ports. */
std::vector<std::shared_ptr<RtMidiIn>> gPorts;

/** Messages that have been output but not yet displayed in gMainWnd.
Protected by gCSMessages against multithreaded access. */
std::vector<tstring> gQueuedMessages;

/** Protects gQueuedMessages against multithreaded access. */
CRITICAL_SECTION gCSMessages;


static const UINT MYM_START = WM_USER + 1;
static const UINT MYM_QUEUED_MESSAGE = WM_USER + 2;

static const int TIMER_DETECT = 1000;

/** The delay (in ms) before moving detection to the next port. */
static const UINT DELAY_DETECT = 500;





tstring format(LPCTSTR aFormat, va_list args)
{
	assert(aFormat != nullptr);

	TCHAR buffer[2048];
	int len;
	va_list argsCopy;
	va_copy(argsCopy, args);
	if ((len = _vsntprintf_s(buffer, ARRAYCOUNT(buffer), aFormat, argsCopy)) != -1)
	{
		// The result did fit into the static buffer
		va_end(argsCopy);
		return tstring(buffer, static_cast<size_t>(len));
	}
	va_end(argsCopy);

	// The result did not fit into the static buffer, use a dynamic buffer:
	// for MS CRT, we need to calculate the result length
	len = _vscprintf(aFormat, args);
	if (len == -1)
	{
		return {};
	}

	// Allocate a buffer and printf into it:
	va_copy(argsCopy, args);
	std::vector<char> Buffer(static_cast<size_t>(len) + 1);
	_vstprintf_s(&(Buffer.front()), Buffer.size(), aFormat, argsCopy);
	va_end(argsCopy);
	return tstring(&(Buffer.front()), Buffer.size() - 1);;
}





void output(LPCTSTR aFormat, ...)
{
	va_list args;
	va_start(args, aFormat);
	auto str = format(aFormat, args);
	str.push_back('\r');
	str.push_back('\n');
	EnterCriticalSection(&gCSMessages);
	gQueuedMessages.push_back(str);
	LeaveCriticalSection(&gCSMessages);
	PostMessage(gWndMain, MYM_QUEUED_MESSAGE, 0, 0);
}




void output()
{
	output(TEXT(" \r\n"));
}





std::string toHex(const std::vector<unsigned char> & aData)
{
	static const char HEX_CHAR[] = "0123456789abcdef";

	std::string res;
	res.reserve(aData.size() * 2);
	for (const auto c: aData)
	{
		res.push_back(HEX_CHAR[(c >> 4) & 0x0f]);
		res.push_back(HEX_CHAR[c & 0x0f]);
	}
	return res;
}





void midiInCallback(double aTimeStamp, std::vector<unsigned char> * aMessage, void * aUserData)
{
	auto portNum = reinterpret_cast<unsigned>(aUserData);
	output(TEXT("IN Port %d: 0x%hs"), portNum, toHex(*aMessage).c_str());
}





void enumDevices(HWND aWnd)
{
	// List all IN ports:
	{
		RtMidiIn dummyIn;
		auto numIn = dummyIn.getPortCount();
		output(TEXT("Number of MIDI IN ports: %d"), numIn);
		for (unsigned i = 0; i < numIn; ++i)
		{
			output(TEXT("  Port %d: %hs"), i, dummyIn.getPortName(i).c_str());
		}
	}

	// List all OUT ports:
	{
		RtMidiOut dummyOut;
		auto numOut = dummyOut.getPortCount();
		output(TEXT("Number of MIDI OUT ports: %d"), numOut);
		for (unsigned i = 0; i < numOut; ++i)
		{
			output(TEXT("  Port %d: %hs"), i, dummyOut.getPortName(i).c_str());
		}
	}

	// Display all incoming data from all IN ports:
	{
		RtMidiIn dummyIn;
		auto numIn = dummyIn.getPortCount();
		for (unsigned i = 0; i < numIn; ++i)
		{
			auto port = std::make_shared<RtMidiIn>();
			try
			{
				port->setCallback(midiInCallback, reinterpret_cast<void *>(i));
				port->ignoreTypes(false, false, false);
				port->openPort(i);
			}
			catch (const std::exception & exc)
			{
				output(TEXT("Cannot open IN port %d (%hs): %hs"), i, dummyIn.getPortName().c_str(), exc.what());
				continue;
			}
			gPorts.push_back(port);
		}
	}

	SetTimer(gWndMain, TIMER_DETECT, DELAY_DETECT, nullptr);
}





void detectAnotherController(void)
{
	static unsigned portOut = 0;
	RtMidiOut out;
	if (portOut >= out.getPortCount())
	{
		KillTimer(gWndMain, TIMER_DETECT);
		output(TEXT("Detection all done."));
		output(TEXT("Feel free to press any controller keys, move sliders and wheels to see their output here."));
		return;
	}
	output();
	output(TEXT("Sending a DeviceQuery message through OUT port %d (%hs)..."), portOut, out.getPortName(portOut).c_str());
	try
	{
		out.openPort(portOut);
		static const std::vector<unsigned char> msg{0xf0, 0x7e, 0x00, 0x06, 0x01, 0xf7};  // SysEx - MMC device query
		out.sendMessage(msg.data(), msg.size());
	}
	catch (const std::exception & exc)
	{
		output(TEXT("Failed to send device query to OUT port %d: %hs"), portOut, exc.what());
	}
	portOut += 1;
}





LRESULT CALLBACK wndProc(HWND aWnd, UINT aMsg, WPARAM wParam, LPARAM lParam)
{
	switch (aMsg)
	{
		case WM_CLOSE:
		{
			PostQuitMessage(0);
			return 0;
		}
		case WM_TIMER:
		{
			detectAnotherController();
			return 0;
		}
		case MYM_START:
		{
			enumDevices(aWnd);
			return 0;
		}
		case MYM_QUEUED_MESSAGE:
		{
			// Collect queued messages:
			tstring buffer;
			EnterCriticalSection(&gCSMessages);
			for (const auto & msg: gQueuedMessages)
			{
				buffer.append(msg);
			}
			gQueuedMessages.clear();
			LeaveCriticalSection(&gCSMessages);

			// Append to window:
			auto textLen = GetWindowTextLength(gWndEdit);
			SendMessage(gWndEdit, EM_SETSEL, textLen, textLen);
			SendMessage(gWndEdit, EM_REPLACESEL, 0, reinterpret_cast<LPARAM>(buffer.c_str()));
			return 0;
		}
		case WM_SIZE:
		{
			RECT rc;
			GetClientRect(aWnd, &rc);
			MoveWindow(gWndEdit, 0, 0, rc.right, rc.bottom, TRUE);
			break;
		}
	}
	return CallWindowProc(gOldWndProc, aWnd, aMsg, wParam, lParam);
};





int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	InitializeCriticalSection(&gCSMessages);

	// Create the main window:
	auto wid = GetSystemMetrics(SM_CXSCREEN);
	auto hei = GetSystemMetrics(SM_CYSCREEN);
	gWndMain = CreateWindowEx(
		WS_EX_APPWINDOW,
		TEXT("STATIC"), TEXT("MidiControllerDump"),
		WS_OVERLAPPEDWINDOW, 
		0, 0, wid,hei,
		NULL, NULL, hInstance, 0
	);
	gWndEdit = CreateWindow(
		TEXT("EDIT"), TEXT(""),
		WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
		0, 0, wid, hei,
		gWndMain, NULL, hInstance, 0
	);
	gOldWndProc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(gWndMain, GWLP_WNDPROC));
	SetWindowLongPtr(gWndMain, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(wndProc));
	ShowWindow(gWndMain, SW_SHOWMAXIMIZED);
	PostMessage(gWndMain, MYM_START, 0, 0);
	SendMessage(gWndEdit, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), 0);

	// Main message loop:
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return static_cast<int>(msg.wParam);
}

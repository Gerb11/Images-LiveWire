/*******************************************************************************
  Course(s):
     CS 1037, University of Western Ontario

  Author(s):
     Andrew Delong <firstname.lastname@gmail.com>
     PNG loader by Alex Pankratov and Mark Adler (see end of file).

  Description:
     Provides functions for a number of basic things students may want to try.
     See cs1037lib-window.h
         cs1037lib-image.h
         cs1037lib-time.h
         cs1037lib-random.h
         cs1037lib-controls.h
         cs1037lib-plot.h
         cs1037lib-thread.h

  History:
     Oct 30, 2010; - Made CreatePlot resize the main window to add another plot below.

     Sep 15, 2010; - LoadImage explicitly checks for transparency for faster drawing 
                     of RGBA images with redundant alpha channel.

     Jul 22, 2010; - Added PNG image support (integrated lpng and puff libraries)

     Jul 19, 2010; - Split header into cs1037lib-window.h, cs1037lib-bitmap.h, 
                     cs1037lib-controls.h, cs1037lib-time.h, cs1037lib-plot.h
                   - Changed DrawString and DrawImage to take content first
                   - Changed SetDrawColour to take unsigned char instead of double

     Oct 12, 2009; - Can now specify default state for CreateDropList,CreateSlider,CreateCheckox

     Oct 10, 2009; - Added basic multi-threading functions

     Oct  9, 2009; - Added WriteImageToFile
                   - Added palettized BMP support
                   - Fixed possible initialization-order bugs

     Feb 15, 2009; - Fixed alpha blending on some video cards via premultiplied-alpha
                   - Force all images to internally use top-down raster order for pixels
                   - Fixed manifest problem on 64-bit targets

     Jan 23, 2009; - Compiles under VC71 (2003)
                   - Plot can have axis labels

     Dec 17, 2008; - Fixed leak of screen DC handle in CreateImage.

     Nov  8, 2008; - Added plotting functions, CreatePlot() etc.
                   - Added GetMicroseconds()

     Nov  5, 2008; - Added SetDrawAxis(), DrawLine(), SetWindowResizeCallback()
                   - Added GetMilliseconds(), GetRandomInt(), GetRandomFloat()

     Nov  4, 2008; - Added TextLabel, Slider controls
                   - Control callbacks now receive arguments (functionptr_string etc)

     Nov  3, 2008; - Button, CheckBox, DropList resize to accommodate text width
                   - SetWindowSize now refers to client size of window

     Nov  2, 2008; - Added TextBox control
                   - Set up ASSERT messages to be more helpful to students
                   - Made 'handle' always the first parameter, for consistency
                   - Got rid of bold default font for controls (why Windows retarded?)

     Nov  1, 2008; - Added DropList and CheckBox controls
                   - Added basic interface for managing controls

     Nov 13, 2007; - Added assertions on image routines
                   - Removed spurious 'Sleep' in SetDrawColour()

     Nov  2, 2007; - Discard WM_MOUSE_MOVE events if there are
                     newer events waiting
                   - Cache most recent handle->bitmap used

     Nov  1, 2007; - Added DrawPolyline(), DrawPolygonFilled()
                   - Added GetImageBytesPerPixel ()

     Oct 29, 2007; - Added Image functions (create, draw, etc)

     Sep 26, 2007; - Added Pause()
                   - Compiles for both _UNICODE and _MBCS now
                   - Reorg parent/child window mechanism
                   - Added Button (no public interface yet)

     Sep 18, 2007; - GetKeyboardInput now reads from console too.
                   - Drawing now works before ShowWindow.
                   - Fixed flickering problem.

     Sep 10, 2007; - Initial window, drawing, event polling.

*******************************************************************************/

#ifndef _CPPRTTI 
#error To use cs1037util, you must enable the Run-Time Type Information (RTTI) feature \
	under Project->Properties->C++->Language settings. \
	Don't forget to do it for both Debug and Release mode.
#endif

#include <conio.h>
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#define _WIN32_WINNT 0x501
#define WINVER       0x501
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <process.h>
#include <tchar.h>
#include <time.h>
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp)   ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)   ((int)(short)HIWORD(lp))
#endif

#undef LoadImage
#undef DrawText
#ifdef UNICODE
#define LoadImageWin32  LoadImageW
#define DrawTextWin32   DrawTextW
#else
#define LoadImageWin32  LoadImageA
#define DrawTextWin32   DrawTextA
#endif // !UNICODE

// Automatically link with comctl32 to make XP-style buttons etc the default
#pragma comment(lib, "comctl32") 
#ifdef _WIN64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' \
                         version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' \
                         version='6.0.0.0' processorArchitecture='X86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#pragma comment(lib, "msimg32") // link for AlphaBlend
#include <list>
#include <map>
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>

#ifdef _WIN64 // Stupid SetWindowLongPtr doesn't have the signature that the docs claim. Pfft.
#define SETWINDOWLONGPTR(hWnd, index, newLong) SetWindowLongPtr(hWnd, index, (LONG_PTR)(newLong))
#else
#define SETWINDOWLONGPTR(hWnd, index, newLong) (size_t)SetWindowLong(hWnd, index, (LONG)(size_t)(newLong))
#endif


#ifdef _UNICODE
typedef std::wstring wstr;
static wstr char2wstr(const char* str) {
	std::wstring result;
	if (str) {
		int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, 0, 0);
		result.resize(len);
		MultiByteToWideChar(CP_UTF8, 0, str, -1, &result[0], len);
	}
	return result;
}

static std::string wchar2str(const TCHAR* str) {
	std::string result;
	if (str) {
		int len = WideCharToMultiByte(CP_UTF8, 0, str, -1, 0, 0, 0, 0);
		result.resize(len-1);
		WideCharToMultiByte(CP_UTF8, 0, str, -1, &result[0], len, 0, 0);
	}
	return result;
}
#define sprintf_TEXT swprintf_s
#else
typedef std::string wstr;
static wstr char2wstr(const char* str) { return str; }
static std::string wchar2str(const char* str) { return str; }
#define sprintf_TEXT sprintf_s
#endif

#ifdef _WIN64
#define AssertBreak __debugbreak();
#else
#define AssertBreak __asm { int 0x3 }
#endif

#if _MSC_VER < 1400  // VC7.1 (2003) or below
// _s variants not defined until VC8 (2005)
static int vsprintf_s(char* dst, size_t, const char* format, va_list args)
{
	return vsprintf(dst, format, args);
}

static int sprintf_s(char* dst, size_t, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	int result = vsprintf(dst, format, args);
	va_end(args);
	return result;
}
#endif

typedef void (*functionptr)();                    // pointer type for:  void foo()
typedef void (*functionptr_int)(int);             // pointer type for:  void foo(int index)
typedef void (*functionptr_bool)(bool);           // pointer type for:  void foo(bool state)
typedef void (*functionptr_float)(float);         // pointer type for:  void foo(float value)
typedef void (*functionptr_string)(const char*);  // pointer type for:  void foo(const char* text)
typedef void (*functionptr_intint)(int,int);      // pointer type for:  void foo(int a, int b)
typedef void (*functionptr_voidptr)(void*);       // pointer type for:  void foo(void* arg)

#define AssertMsg(expr,msg) { if (expr) { } else { sAssertDialog(msg, __FILE__, __LINE__); AssertBreak; } }
#define AssertUnreachable(msg) { sAssertDialog(msg, __FILE__, __LINE__); AssertBreak; }

void sAssertDialog(const char* msg, const char* file, int line);

HBITMAP LoadPng(const TCHAR * resName,
                const TCHAR * resType,
                HMODULE         resInst,
                BOOL   premultiplyAlpha); // LoadPng by Alex Pankratov


struct BitmapInfo {
	HANDLE osHandle;
	DIBSECTION dib;
	bool has_transparency;
};

static bool gQuit = false;

// For some alpha-blended excitement on XP/2000, uncomment this (oooooooh ghost!)
//#define WINDOW_ALPHA 70

//////////////////////////////////////////////////////////////////////
// Window
//
// Notes on window creation/deletion:
//  * To keep the implementation simple for CS1037, Window creation/deletion is NOT thread safe.
//  * The Window object does not create ('realize') the OS window (HWND etc) until
//    its handle is needed for something.
//  * OS windows can be destroyed separately from the corresponding Window object,
//    but the Window objects will still hang around (and will have a zero HWND handle).
//    The 'children()' method should help to explicitly traverse and to delete Window
//    objects that were created on the heap.
//

class Window {
public:
	Window(const char* name);

	// 'Destroys' this window (and all child windows via 'delete').
	// Any pointers to this Window object will be invalid after this call.
	//
	void destroy();

	const char*   name() const;
	virtual void  setText(const TCHAR* text);
	virtual void  setPosition(int x, int y);
	virtual void  setPositionAndSize(int x, int y, int windowSizeX, int windowSizeY);
	virtual void  setSize(int windowSizeX, int windowSizeY);
	virtual void  setVisible(bool visible);

	int   screenX() const;
	int   screenY() const;
	int   relativeX() const; // relative to parent
	int   relativeY() const; // relative to parent
	int   windowSizeX() const;
	int   windowSizeY() const;
	int   clientSizeX() const;
	int   clientSizeY() const;
	void  fitSizeToText();

	void        setParent(Window* window);
	Window*     parent();
	Window*     child(const char* name);

	static Window* sWindowFromHandle(HWND hWnd);

	// Should use delegates but to keep the implementation simple 
	// this void* nonsense will be good enough, probably.
	typedef void (*OnClickHandler)(void* userData, Window* sender);
	typedef void (*OnSizeHandler)(void* userData, Window* sender, int windowSizeX, int windowSizeY);

	void setOnSize(OnSizeHandler onSize, void* userData);

	// ATTN: it is **NOT** safe to request handle() from within subclass 
	//       constructors (expects VMT of subclasses to be loaded!)
	HWND  handle() const; 

protected:
	enum { 
		 WM_COMMAND_REFLECT = WM_USER+0x0001
	    ,WM_SCROLL_REFLECT 
	};

	// All subclasses of Window should have protected/private destructors, and
	// rely on the destroy() method above.
	//
	// The destructor is not public so that programmers will not be able to create
	// a Window object on the stack or as global data. This way, we know all child
	// windows were created on the heap and so we can clean up after ourselves.
	//
	virtual ~Window();

	virtual void    getWndClassStyle(WNDCLASS& wndClass) const;
	virtual void    getWndStyle(DWORD& dwStyle, DWORD& dwExStyle) const;
	virtual void    realize();

	// The WndProc callback is invoked whenever MS Windows sends us some kind
	// of event ("message").
	//
	virtual LRESULT wndProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

	// Each should return true if the default WNDPROC for this class should handle 
	// the event, and return false if the event has been handled internally by the method.
	//
	virtual bool    onCreate();
	virtual bool    onDestroy();
	virtual bool    onClose();
	virtual bool    onKeyDown(unsigned key);
	virtual bool    onKeyUp(unsigned key);
	virtual bool    onMouseMove(int x, int y);
	virtual bool    onMouseDown(int x, int y, int button);
	virtual bool    onMouseUp(int x, int y, int button);
	virtual bool    onPaint();
	virtual bool    onParentChanged();
	virtual bool    onSizeChanged();

	virtual void    adjustFitSizeToText(RECT& rect) const;

	static HFONT                    sDefaultFont;
	static HFONT                    sDefaultFontVertical;

private:
	void registerWndClass(WNDCLASS& wndClass);
	void rebuildChildrenList();
	static BOOL    CALLBACK         sCollectChildrenCallback(HWND hWnd, LPARAM lParam);

	typedef std::list<Window*> ChildList;
	HWND        mHWND;
	Window*     mParent;
	ChildList   mChildren;
	std::string mName;
	WNDPROC     mBaseWndProc;
	bool        mIsDestroyed;
	OnSizeHandler mOnSize;
	void*         mOnSizeData;

private:
	// Window instance registration
	static const TCHAR*             cBaseWndClassName;
	static Window*                  sWindowBeingCreated;
	static Window*                  sWindowBeingCreatedParent;
	static LRESULT CALLBACK         sGlobalWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

const TCHAR* Window::cBaseWndClassName = TEXT("cs1037window");
HFONT Window::sDefaultFont = 0;
HFONT Window::sDefaultFontVertical = 0;

Window::Window(const char* name)
: mHWND(0)
, mParent(0)
, mName(name)
, mBaseWndProc(0)
, mIsDestroyed(false)
, mOnSize(0)
, mOnSizeData(0)
{
	// don't call virtual methods in constructors -- it doesn't work the way you expect!
}

Window::~Window()
{
	while (!mChildren.empty())
		mChildren.front()->destroy(); // this will rebuild mChildren, so avoid iterators
	
	if (mHWND) {
		DestroyWindow(mHWND);
	}
	if (mParent) 
		mParent->rebuildChildrenList();
}

void Window::destroy() { delete this; }

void Window::getWndClassStyle(WNDCLASS& wndClass) const
{
	wndClass.style         = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc   = sGlobalWndProc;
	wndClass.cbClsExtra    = 0;
	wndClass.cbWndExtra    = 0;
	wndClass.hInstance     = GetModuleHandle(0);
	wndClass.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wndClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = 0;
	wndClass.lpszMenuName  = 0;
	wndClass.lpszClassName = cBaseWndClassName;
}

void Window::getWndStyle(DWORD& dwStyle, DWORD& dwExStyle) const
{
	dwExStyle |= 0;
	dwStyle   |= WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE;
}

const char* Window::name() const
{
	return mName.c_str();
}

void Window::setText(const TCHAR* title)
{
	SetWindowText(handle(), title);
}

void Window::setPosition(int x, int y)
{
	SetWindowPos(handle(), 0, x, y, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOOWNERZORDER);
}

void Window::setPositionAndSize(int x, int y, int windowSizeX, int windowSizeY)
{
	SetWindowPos(handle(), 0, x, y, windowSizeX, windowSizeY, SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void Window::setSize(int windowSizeX, int windowSizeY)
{
	SetWindowPos(handle(), 0, 0, 0, windowSizeX, windowSizeY, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void Window::setVisible(bool visible)
{
	ShowWindow(handle(), visible ? SW_SHOW : SW_HIDE);
}

int Window::screenX() const
{
	RECT r;
	GetWindowRect(handle(), &r);
	return r.left;
}

int Window::screenY() const
{
	RECT r;
	GetWindowRect(handle(), &r);
	return r.top;
}

int Window::relativeX() const
{
	int result = 0;
	RECT r;
	GetWindowRect(handle(), &r);
	result = r.left;
	HWND parentHWND = GetParent(handle());
	if (parentHWND) {
		GetWindowRect(parentHWND, &r);
		result -= r.left;
	}
	return result;
}

int Window::relativeY() const
{
	int result = 0;
	RECT r;
	GetWindowRect(handle(), &r);
	result = r.top;
	HWND parentHWND = GetParent(handle());
	if (parentHWND) {
		GetWindowRect(parentHWND, &r);
		result -= r.top;
	}
	return result;
}

int Window::windowSizeX() const
{
	RECT r;
	GetWindowRect(handle(), &r);
	return r.right - r.left;
}

int Window::windowSizeY() const
{
	RECT r;
	GetWindowRect(handle(), &r);
	return r.bottom - r.top;
}

int Window::clientSizeX() const
{
	RECT r;
	GetClientRect(handle(), &r);
	return r.right;
}

int Window::clientSizeY() const
{
	RECT r;
	GetClientRect(handle(), &r);
	return r.bottom;
}

void Window::fitSizeToText()
{
	HDC dc = GetDC(handle());
	int textLen = (int)SendMessage(handle(), WM_GETTEXTLENGTH, 0, 0);
	TCHAR* text = new TCHAR[textLen+1];
	SendMessage(handle(), WM_GETTEXT, textLen+1, (LPARAM)text);
	RECT r = { 0, 0, 0, 0 };
	DrawTextEx(dc, text, -1, &r, DT_LEFT | DT_TOP | DT_CALCRECT, 0);
	delete [] text;
	ReleaseDC(handle(), dc);
	adjustFitSizeToText(r);
	setSize(r.right, r.bottom);
}

void Window::setParent(Window* parent)
{
	if (!mHWND) {
		mParent = parent; // parent will be used in realize() later on
		mParent->mChildren.push_back(this);
		onParentChanged();
		return;
	}
	// Otherwise, reparent
	LONG currStyle = GetWindowLong(handle(), GWL_STYLE);
	if (parent) {
		SetParent(handle(), parent->handle());
		SETWINDOWLONGPTR(handle(), GWL_STYLE, currStyle | WS_CHILD);
	} else {
		SetParent(handle(), 0);
		SETWINDOWLONGPTR(handle(), GWL_STYLE, currStyle & ~WS_CHILD);
	}
	if (mParent)
		mParent->rebuildChildrenList();
	mParent = parent;
	if (mParent)
		mParent->rebuildChildrenList();
	onParentChanged();
}

Window* Window::parent()
{
	return mParent;
}

Window* Window::child(const char *name)
{
	for (ChildList::const_iterator i = mChildren.begin(); i != mChildren.end(); ++i)
		if (strcmp((*i)->name(), name) == 0)
			return *i;
	return 0;
}

void Window::setOnSize(OnSizeHandler onSize, void* userData)
{
	mOnSize = onSize;
	mOnSizeData = userData;
}

HWND  Window::handle() const
{
	if (!mHWND && !mIsDestroyed)
		const_cast<Window*>(this)->realize();
	return mHWND;
}

void Window::rebuildChildrenList()
{
	mChildren.clear();
	EnumChildWindows(mHWND, sCollectChildrenCallback, reinterpret_cast<LPARAM>(this));
}

void Window::realize()
{
	WNDCLASS wndClass;
	memset(&wndClass, 0, sizeof(WNDCLASS));
	getWndClassStyle(wndClass);
	registerWndClass(wndClass);

	DWORD dwStyle = 0, dwExStyle = 0;
	getWndStyle(dwStyle, dwExStyle);

	HWND parentHWND = 0;
	if (mParent) {
		parentHWND = mParent->handle();
		dwStyle |= WS_CHILD;
	}

	mHWND = CreateWindowEx(dwExStyle, wndClass.lpszClassName, TEXT(""), dwStyle,
							   0, 0, 0, 0,
							   parentHWND, 0, GetModuleHandle(0), 0);

	if (!mHWND)
		throw std::runtime_error("CreateWindowEx failed");

	// Redirect all messages to our WndProc dispatcher, and for this particular
	// instance remember which window class behaviour was requested so that
	// we can make it the default (i.e. call the original, mBaseWndProc, by default).
	//
	mBaseWndProc = (WNDPROC)SETWINDOWLONGPTR(mHWND, GWLP_WNDPROC, sGlobalWndProc);

	// Allow casting back and forth between window handles and Window objects
	//
	SETWINDOWLONGPTR(mHWND, GWLP_USERDATA, static_cast<Window*>(this));

	// Force the style we actually requested, since CreateWindow sometimes adds
	// unwanted styles depending on if there is an initial parent, etc.
	//
	SETWINDOWLONGPTR(mHWND, GWL_STYLE, dwStyle);

	if (!sDefaultFont) {
		sDefaultFont = CreateFont(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, \
			  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, \
			  FF_DONTCARE, 0);
		sDefaultFontVertical = CreateFont(15, 0, 900, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, \
			  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, \
			  FF_DONTCARE, 0);
	}

	SendMessage(mHWND, WM_SETFONT, (WPARAM)sDefaultFont, TRUE);
}

void Window::registerWndClass(WNDCLASS& wndClass)
{
	WNDCLASS existingClass;
	if (!GetClassInfo(GetModuleHandle(0), wndClass.lpszClassName, &existingClass))
		if (!RegisterClass(&wndClass))
			throw std::runtime_error("RegisterClass failed");
}

LRESULT Window::wndProc(UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
	switch (uMsg) {
		case WM_CREATE: { if (onCreate()) break; return 0; }
		case WM_DESTROY: { if (onDestroy()) break; return 0; }
		case WM_CLOSE: { if (onClose()) break; return 0; }
		case WM_MOUSEMOVE: { if (onMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam))) break; return 0; }
		case WM_LBUTTONDOWN: { if (onMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0)) break; return 0; }
		case WM_RBUTTONDOWN: { if (onMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 1)) break; return 0; }
		case WM_MBUTTONDOWN: { if (onMouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 2)) break; return 0; }
		case WM_LBUTTONUP: { if (onMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0)) break; return 0; }
		case WM_RBUTTONUP: { if (onMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 1)) break; return 0; }
		case WM_MBUTTONUP: { if (onMouseUp(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 2)) break; return 0; }
		case WM_PAINT: { if (onPaint()) break; return 0; }
		case WM_SIZE: { if (onSizeChanged()) break; return 0; }
		case WM_KEYDOWN: { if (onKeyDown((UINT)wParam)) break; return 0; }
		case WM_KEYUP: { if (onKeyUp((UINT)wParam)) break; return 0; }
		case WM_COMMAND: {
			if (wParam == IDOK && !lParam) {
				// If user hit enter on a control, reflect 
				// the IDOK event to that control so that it
				// can react to the ENTER key
				lParam = (LPARAM)GetFocus();
			}
			if (lParam) {
				SendMessage(reinterpret_cast<HWND>(lParam), WM_COMMAND_REFLECT, wParam, lParam);
				break;
			}
		}
		case WM_HSCROLL:
		case WM_VSCROLL: {
			if (lParam) {
				SendMessage(reinterpret_cast<HWND>(lParam), WM_SCROLL_REFLECT, wParam, lParam);
				break;
			}
		}
	}
	if (mBaseWndProc != sGlobalWndProc) {
		LRESULT r = CallWindowProc(mBaseWndProc, mHWND, uMsg, wParam, lParam);
		return r;
	} else {
		LRESULT r = DefWindowProc(mHWND, uMsg, wParam, lParam);
		return r;
	}
}

bool Window::onCreate() { return true; }
bool Window::onDestroy() { 
	mHWND = 0;
	mIsDestroyed = true;
	return true; 
}
bool Window::onClose() { return true; }
bool Window::onKeyDown(unsigned /*key*/) { return true; }
bool Window::onKeyUp(unsigned /*key*/) { return true; }
bool Window::onMouseMove(int /*x*/, int /*y*/) { return true; }
bool Window::onMouseDown(int /*x*/, int /*y*/, int /*button*/) { return true; }
bool Window::onMouseUp(int /*x*/, int /*y*/, int /*button*/) { return true; }
bool Window::onPaint() { return true; }
bool Window::onParentChanged() { return true; }

bool Window::onSizeChanged() { 
	if (mOnSize)
		mOnSize(mOnSizeData, this, windowSizeX(), windowSizeY());
	return true; 
}

void Window::adjustFitSizeToText(RECT& /*rect*/) const { }

Window* Window::sWindowFromHandle(HWND hWnd)
{
	return reinterpret_cast<Window*>((LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA));
}

LRESULT CALLBACK Window::sGlobalWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
	Window* window = sWindowFromHandle(hWnd);
	if (window)
		return window->wndProc(uMsg, wParam, lParam);

	// Called with unregistered window handle. 
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}


BOOL CALLBACK Window::sCollectChildrenCallback(HWND hWnd, LPARAM lParam)
{
	Window* parent = reinterpret_cast<Window*>(lParam);
	Window* child = reinterpret_cast<Window*>((LONG_PTR)GetWindowLongPtr(hWnd, GWLP_USERDATA));
	if (child)
		parent->mChildren.push_back(child);
	return TRUE;
}


//////////////////////////////////////////////////////////////////////
// StaticCtl

class StaticCtl: public Window {
	typedef Window Super;
public:
	StaticCtl(const char* name);
protected:
	virtual ~StaticCtl();
	virtual void    getWndClassStyle(WNDCLASS& wndClass) const;
	virtual void    getWndStyle(DWORD& dwStyle, DWORD& dwExStyle) const;
	virtual void    adjustFitSizeToText(RECT& rect) const;
};

StaticCtl::StaticCtl(const char *name)
: Super(name)
{ }

StaticCtl::~StaticCtl() { }


void StaticCtl::getWndClassStyle(WNDCLASS& wndClass) const
{
	Super::getWndClassStyle(wndClass);
	wndClass.lpszClassName = TEXT("STATIC");
}

void StaticCtl::getWndStyle(DWORD& dwStyle, DWORD& dwExStyle) const
{
	Super::getWndStyle(dwStyle, dwExStyle);
	dwStyle |= SS_CENTER | SS_CENTERIMAGE;
}

void StaticCtl::adjustFitSizeToText(RECT& rect) const
{
	rect.right  += 2;
	rect.bottom += 7;
}


//////////////////////////////////////////////////////////////////////
// ButtonCtl

class ButtonCtl: public Window {
	typedef Window Super;
public:
	ButtonCtl(const char* name);

	void setOnClick(OnClickHandler onClick, void* userData);

protected:
	virtual ~ButtonCtl();
	virtual void    getWndClassStyle(WNDCLASS& wndClass) const;
	virtual void    getWndStyle(DWORD& dwStyle, DWORD& dwExStyle) const;
	virtual LRESULT wndProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual void    adjustFitSizeToText(RECT& rect) const;

private:
	OnClickHandler mOnClick;
	void*          mOnClickData;
};

ButtonCtl::ButtonCtl(const char *name)
: Super(name)
, mOnClick(0)
, mOnClickData(0) 
{ }

ButtonCtl::~ButtonCtl() { }

void ButtonCtl::setOnClick(OnClickHandler onClick, void* userData)
{
	mOnClick = onClick;
	mOnClickData = userData;
}

void ButtonCtl::getWndClassStyle(WNDCLASS& wndClass) const
{
	Super::getWndClassStyle(wndClass);
	wndClass.lpszClassName = TEXT("BUTTON");
}

void ButtonCtl::getWndStyle(DWORD& dwStyle, DWORD& dwExStyle) const
{
	Super::getWndStyle(dwStyle, dwExStyle);
	dwStyle |= BS_PUSHBUTTON | BS_TEXT | WS_TABSTOP;
}

LRESULT ButtonCtl::wndProc(UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
	switch (uMsg) {
		case WM_COMMAND_REFLECT: { 
			if (mOnClick)
				mOnClick(mOnClickData, this);
			return 0; 
		}
	}
	return Super::wndProc(uMsg, wParam, lParam);
}

void ButtonCtl::adjustFitSizeToText(RECT& rect) const
{
	if (rect.right < 50)
		rect.right = 50;
	rect.right  += 10;
	rect.bottom += 7;
}

//////////////////////////////////////////////////////////////////////
// ComboBoxCtl

class ComboBoxCtl: public Window {
	typedef Window Super;
public:
	ComboBoxCtl(const char* name);

	typedef void (*OnSelectionChangedHandler)(void* userData, Window* sender);
	void setOnSelectionChanged(OnSelectionChangedHandler onSelectionChanged, void* userData);
	void insertItem(int pos, const char* text);
	void setSelectedItem(int index);
	int  getSelectedItem() const;
	int  itemCount() const;
	void fitSizeToItems();

protected:
	virtual ~ComboBoxCtl();
	virtual void    getWndClassStyle(WNDCLASS& wndClass) const;
	virtual void    getWndStyle(DWORD& dwStyle, DWORD& dwExStyle) const;
	virtual LRESULT wndProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	int                       mPrevSelectedItem;
	OnSelectionChangedHandler mOnSelectionChanged;
	void*                     mOnSelectionChangedData;
};

ComboBoxCtl::ComboBoxCtl(const char *name)
: Super(name)
, mOnSelectionChanged(0)
, mOnSelectionChangedData(0) 
, mPrevSelectedItem(0)
{ }

ComboBoxCtl::~ComboBoxCtl() { }

void ComboBoxCtl::setOnSelectionChanged(OnSelectionChangedHandler onSelectionChanged, void* userData)
{
	mOnSelectionChanged = onSelectionChanged;
	mOnSelectionChangedData = userData;
}

void ComboBoxCtl::insertItem(int pos, const char* text)
{
	SendMessage(handle(), CB_INSERTSTRING, (WPARAM)pos, (LPARAM)char2wstr(text).c_str());
}

int  ComboBoxCtl::getSelectedItem() const
{
	return (int)SendMessage(handle(), CB_GETCURSEL, 0, 0);
}

void ComboBoxCtl::setSelectedItem(int index)
{
	if (index != getSelectedItem()) {
		SendMessage(handle(), CB_SETCURSEL, (WPARAM)index, 0);
		if (mPrevSelectedItem != index) {
			mPrevSelectedItem = index;
			if (mOnSelectionChanged)
				mOnSelectionChanged(mOnSelectionChangedData, this);
		}
	}
}

int  ComboBoxCtl::itemCount() const
{
	return (int)SendMessage(handle(), CB_GETCOUNT, 0, 0);
}

void ComboBoxCtl::fitSizeToItems()
{
	int maxItemSizeX = 100;	
	int count = itemCount();
	HDC dc = GetDC(handle());
	for (int i = 0; i < count; ++i) {
		int itemLen = (int)SendMessage(handle(), CB_GETLBTEXTLEN, i, 0);
		TCHAR* item = new TCHAR[itemLen+1];
		SendMessage(handle(), CB_GETLBTEXT, i, (LPARAM)item);
		RECT r = { 0, 0, 0, 0 };
		DrawTextEx(dc, item, -1, &r, DT_LEFT | DT_TOP | DT_CALCRECT, 0);
		delete [] item;
		if (r.right > maxItemSizeX)
			maxItemSizeX = r.right;
	}
	ReleaseDC(handle(), dc);
	setSize(maxItemSizeX+10, windowSizeY());
}

void ComboBoxCtl::getWndClassStyle(WNDCLASS& wndClass) const
{
	Super::getWndClassStyle(wndClass);
	wndClass.lpszClassName = TEXT("COMBOBOX");
}

void ComboBoxCtl::getWndStyle(DWORD& dwStyle, DWORD& dwExStyle) const
{
	Super::getWndStyle(dwStyle, dwExStyle);
	dwStyle |= CBS_DROPDOWNLIST | WS_TABSTOP;
}

LRESULT ComboBoxCtl::wndProc(UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
	switch (uMsg) {
		case WM_COMMAND_REFLECT: {
			if (HIWORD(wParam) == CBN_SELENDOK && mOnSelectionChanged) {
				if (getSelectedItem() != mPrevSelectedItem) { // keep silent if user re-selects already selected item
					mPrevSelectedItem = getSelectedItem();
					mOnSelectionChanged(mOnSelectionChangedData, this);
				}
			}
			return 0;
		}
	}
	return Super::wndProc(uMsg, wParam, lParam);
}


//////////////////////////////////////////////////////////////////////
// CheckBoxCtl

class CheckBoxCtl: public Window {
	typedef Window Super;
public:
	CheckBoxCtl(const char* name);

	typedef void (*OnStateChangedHandler)(void* userData, Window* sender);
	void setOnStateChanged(OnStateChangedHandler onStateChanged, void* userData);
	void setState(bool selected);
	bool getState() const;

protected:
	virtual ~CheckBoxCtl();
	virtual void    getWndClassStyle(WNDCLASS& wndClass) const;
	virtual void    getWndStyle(DWORD& dwStyle, DWORD& dwExStyle) const;
	virtual LRESULT wndProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
	virtual void    adjustFitSizeToText(RECT& rect) const;

private:
	OnStateChangedHandler mOnStateChanged;
	void*                 mOnStateChangedData;
};

CheckBoxCtl::CheckBoxCtl(const char *name)
: Super(name)
, mOnStateChanged(0)
, mOnStateChangedData(0) 
{ }

CheckBoxCtl::~CheckBoxCtl() { }

void CheckBoxCtl::setOnStateChanged(OnStateChangedHandler onStateChanged, void* userData)
{
	mOnStateChanged = onStateChanged;
	mOnStateChangedData = userData;
}

bool CheckBoxCtl::getState() const
{
	return SendMessage(handle(), BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void CheckBoxCtl::setState(bool selected)
{
	if (selected != getState()) {
		SendMessage(handle(), BM_SETCHECK, (WPARAM)selected ? BST_CHECKED : BST_UNCHECKED, 0);
		if (mOnStateChanged)
			mOnStateChanged(mOnStateChangedData, this);
	}
}

void CheckBoxCtl::getWndClassStyle(WNDCLASS& wndClass) const
{
	Super::getWndClassStyle(wndClass);
	wndClass.lpszClassName = TEXT("BUTTON");
}

void CheckBoxCtl::getWndStyle(DWORD& dwStyle, DWORD& dwExStyle) const
{
	Super::getWndStyle(dwStyle, dwExStyle);
	dwStyle |= BS_AUTOCHECKBOX | WS_TABSTOP;
}

LRESULT CheckBoxCtl::wndProc(UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
	switch (uMsg) {
		case WM_COMMAND_REFLECT: {
			if (HIWORD(wParam) == BN_CLICKED && mOnStateChanged) {
				mOnStateChanged(mOnStateChangedData, this);
			}
			return 0;
		}
	}
	return Super::wndProc(uMsg, wParam, lParam);
}

void CheckBoxCtl::adjustFitSizeToText(RECT& rect) const
{
	rect.right  += 25;
	rect.bottom += 7;
}


//////////////////////////////////////////////////////////////////////
// EditCtl

class EditCtl: public Window {
	typedef Window Super;
public:
	EditCtl(const char* name);

	typedef void (*OnStringChangedHandler)(void* userData, Window* sender);
	void setOnStringChanged(OnStringChangedHandler onStringChanged, void* userData);
	void setString(const char* value);
	const char* getString() const;

protected:
	virtual ~EditCtl();
	virtual void    getWndClassStyle(WNDCLASS& wndClass) const;
	virtual void    getWndStyle(DWORD& dwStyle, DWORD& dwExStyle) const;
	virtual LRESULT wndProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	OnStringChangedHandler mOnStringChanged;
	void*                  mOnStringChangedData;
	std::string            mString;
};

EditCtl::EditCtl(const char *name)
: Super(name)
, mOnStringChanged(0)
, mOnStringChangedData(0) 
{ }

EditCtl::~EditCtl() { }

void EditCtl::setOnStringChanged(OnStringChangedHandler onStringChanged, void* userData)
{
	mOnStringChanged = onStringChanged;
	mOnStringChangedData = userData;
}

void EditCtl::setString(const char* str)
{
	if (mString != str) {
		mString = str;
		SendMessage(handle(), WM_SETTEXT, (WPARAM)0, (LPARAM)char2wstr(str).c_str());
		if (mOnStringChanged)
			mOnStringChanged(mOnStringChangedData, this);
	}
}

const char* EditCtl::getString() const
{
	return mString.c_str();
}

void EditCtl::getWndClassStyle(WNDCLASS& wndClass) const
{
	Super::getWndClassStyle(wndClass);
	wndClass.lpszClassName = TEXT("EDIT");
}

void EditCtl::getWndStyle(DWORD& dwStyle, DWORD& dwExStyle) const
{
	Super::getWndStyle(dwStyle, dwExStyle);
	dwStyle |= WS_TABSTOP | ES_AUTOHSCROLL;
	dwExStyle |= WS_EX_CLIENTEDGE;
}

LRESULT EditCtl::wndProc(UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
	if (uMsg == WM_COMMAND_REFLECT && (HIWORD(wParam) == EN_KILLFOCUS || wParam == IDOK)) {
		int textLen = (int)SendMessage(handle(), WM_GETTEXTLENGTH, 0, 0);
		TCHAR* buffer = new TCHAR[textLen+1];
		SendMessage(handle(), WM_GETTEXT, (WPARAM)textLen+1, (LPARAM)buffer);
		std::string newStr = wchar2str(buffer);
		if (mString != newStr) {
			mString = newStr;
			if (mOnStringChanged)
				mOnStringChanged(mOnStringChangedData, this);
		}
		delete [] buffer;
		return 0;
	}
	return Super::wndProc(uMsg, wParam, lParam);
}


//////////////////////////////////////////////////////////////////////
// TrackbarCtl

class TrackbarCtl: public Window {
	typedef Window Super;
public:
	TrackbarCtl(const char* name, bool isHoriz);

	typedef void (*OnValueChangedHandler)(void* userData, Window* sender);
	void setOnValueChanged(OnValueChangedHandler onValueChanged, void* userData);
	void setValue(float value);
	float getValue() const;

protected:
	virtual ~TrackbarCtl();
	virtual void    getWndClassStyle(WNDCLASS& wndClass) const;
	virtual void    getWndStyle(DWORD& dwStyle, DWORD& dwExStyle) const;
	virtual LRESULT wndProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	OnValueChangedHandler  mOnValueChanged;
	void*                  mOnValueChangedData;
	float                  mPrevValue;
	bool                   mIsHoriz;
};

TrackbarCtl::TrackbarCtl(const char *name, bool isHoriz)
: Super(name)
, mOnValueChanged(0)
, mOnValueChangedData(0) 
, mPrevValue(0.0f)
, mIsHoriz(isHoriz)
{ }

TrackbarCtl::~TrackbarCtl() { }

void TrackbarCtl::setOnValueChanged(OnValueChangedHandler onValueChanged, void* userData)
{
	mOnValueChanged = onValueChanged;
	mOnValueChangedData = userData;
}

void TrackbarCtl::setValue(float value)
{
	if (mPrevValue != value) {
		int rmin = (int)SendMessage(handle(), TBM_GETRANGEMIN, 0, 0);
		int rmax = (int)SendMessage(handle(), TBM_GETRANGEMAX, 0, 0);
		int pos = rmin + (int)(value * (rmax - rmin));
		SendMessage(handle(), TBM_SETPOS, (WPARAM)TRUE, (LPARAM)pos);
		if (mPrevValue != getValue()) {
			mPrevValue = getValue();
			if (mOnValueChanged)
				mOnValueChanged(mOnValueChangedData, this);
		}
	}
}

float TrackbarCtl::getValue() const
{
	int pos = (int)SendMessage(handle(), TBM_GETPOS, 0, 0);
	int rmin = (int)SendMessage(handle(), TBM_GETRANGEMIN, 0, 0);
	int rmax = (int)SendMessage(handle(), TBM_GETRANGEMAX, 0, 0);
	if (!mIsHoriz)
		pos = rmax - (pos-rmin); // flip so that [bottom,top] map to [0,1]
	if (rmin < rmax)
		return (float)(pos-rmin) / (float)(rmax-rmin);
	return 0;
}

void TrackbarCtl::getWndClassStyle(WNDCLASS& wndClass) const
{
	Super::getWndClassStyle(wndClass);
	wndClass.lpszClassName = TRACKBAR_CLASS;
}

void TrackbarCtl::getWndStyle(DWORD& dwStyle, DWORD& dwExStyle) const
{
	Super::getWndStyle(dwStyle, dwExStyle);
	dwStyle |= WS_TABSTOP | (mIsHoriz ? TBS_HORZ : TBS_VERT);
}

LRESULT TrackbarCtl::wndProc(UINT uMsg, WPARAM wParam, LPARAM lParam) 
{
	switch (uMsg) {
		case WM_SCROLL_REFLECT: {
			if (lParam && (LOWORD(wParam) >= TB_LINEUP && LOWORD(wParam) <= TB_ENDTRACK)) {
				float value = getValue();
				if (mPrevValue != value) {
					mPrevValue = value;
					if (mOnValueChanged)
						mOnValueChanged(mOnValueChangedData, this);
				}
				return 0;
			}
			break;
		}
		case WM_ERASEBKGND: { return 1; }
	}		
	return Super::wndProc(uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////
// SimpleCanvas

class SimpleCanvas: public Window {
	typedef Window Super;
public:
	SimpleCanvas(const char* name);

	// The off screen canvas is, by default, the same size as the client area of the
	// window when it is first drawn. This method resizes the canvas area, preserving as 
	// much of the existing pixels as possible.
	void setBitmapSize(int sizeX, int sizeY);

	void setAxisOrigin(int x0, int y0);
	void setAxisFlipY(bool flip);
	int  axisOriginX() const;
	int  axisOriginY() const;
	bool axisFlipY() const;

	// Simple drawing methods. Relevant client area is automatically invalidated.
	void setColour(unsigned char r, unsigned char g, unsigned char b);
	POINT measureString(const TCHAR* str, bool vertical=false);
	void drawString(int x0, int y0, const TCHAR* str, bool centered=false, bool vertical=false);
	void drawRect(int x0, int y0, int x1, int y1);
	void fillRect(int x0, int y0, int x1, int y1);
	void drawEllipse(int x0, int y0, int x1, int y1);
	void fillEllipse(int x0, int y0, int x1, int y1);
	void drawLine(int x0, int y0, int x1, int y1);
	void drawPolyline(int* pts, int count);
	void fillPoly(int* pts, int count);
	void drawBitmap(int x0, int y0, const BitmapInfo& bmp);
	void stretchBitmap(int x0, int y0, int x1, int y1, const BitmapInfo& bmp);

	HDC             DC();
	HDC             offScreenDC();

protected:
	virtual ~SimpleCanvas();

	virtual void    getWndClassStyle(WNDCLASS& wndClass) const;

	virtual bool    onMouseDown(int x, int y, int button);
	virtual bool    onPaint();

	void transformRect(RECT* r);

private:
	HDC     mOwnDC;
	HDC     mOffScreenDC;
	HBITMAP mOffScreenBM;
	HBRUSH  mActiveBrush; // must be explicitly selected when needed
	HPEN    mActivePen;   // must be explicitly selected when needed
	int     mX0, mY0;
	int     mYDir;
};


SimpleCanvas::SimpleCanvas(const char* name)
: Super(name)
, mOwnDC(0)
, mOffScreenDC(0)
, mOffScreenBM(0)
, mActiveBrush(0)
, mActivePen(0)
, mX0(0)
, mY0(0)
, mYDir(1)
{
}

SimpleCanvas::~SimpleCanvas()
{
	// mOwnDC is destructed with the corresponding window handle
	if (mOffScreenDC) DeleteDC(mOffScreenDC);
	if (mOffScreenBM) DeleteObject(mOffScreenBM);
	if (mActiveBrush) DeleteObject(mActiveBrush);
	if (mActivePen) DeleteObject(mActivePen);
}

void SimpleCanvas::setAxisOrigin(int x0, int y0) { mX0 = x0; mY0 = y0; }
void SimpleCanvas::setAxisFlipY(bool flip) { mYDir = flip ? -1 : 1; }
int  SimpleCanvas::axisOriginX() const { return mX0; }
int  SimpleCanvas::axisOriginY() const { return mY0; } 
bool SimpleCanvas::axisFlipY() const { return mYDir < 0; }

void SimpleCanvas::setBitmapSize(int sizeX, int sizeY)
{
	HBITMAP newBM = CreateCompatibleBitmap(DC(), sizeX, sizeY);
	if (!newBM)
		throw std::runtime_error("CreateCompatibleBitmap failed.");
	SetBitmapDimensionEx(newBM, sizeX, sizeY, 0);
	if (mOffScreenBM) {
		// Copy as much of the old contents as possible into the new bitmap
		HDC tempDC = CreateCompatibleDC(DC());
		HGDIOBJ defaultBM = SelectObject(tempDC, newBM);

		// Perform the copy from old to new
		SIZE oldBMSize;
		GetBitmapDimensionEx(mOffScreenBM, &oldBMSize);
		BitBlt(tempDC, 0, 0, oldBMSize.cx, oldBMSize.cy, 
			   offScreenDC(), 0, 0, SRCCOPY);

		// Select clean up and select new bitmap
		SelectObject(tempDC, defaultBM);
		DeleteDC(tempDC);
		HGDIOBJ oldBM = SelectObject(offScreenDC(), newBM);
		if (oldBM != mOffScreenBM)
			throw std::runtime_error("Unexpected HGDIOBJ was selected for off-screen bitmap.");
		DeleteObject(mOffScreenBM);
	} else {
		SelectObject(offScreenDC(), newBM);
		HGDIOBJ oldBM = SelectObject(offScreenDC(), newBM);
		if (oldBM != newBM)
			throw std::runtime_error("SelectObject failed to select new off-screen bitmap.");
	}
	mOffScreenBM = newBM;
}

void SimpleCanvas::setColour(unsigned char r, unsigned char g, unsigned char b)
{
	DeleteObject(mActivePen);
	DeleteObject(mActiveBrush);
	mActiveBrush = CreateSolidBrush(RGB(r,g,b));
	mActivePen = CreatePen(PS_SOLID, 1, RGB(r,g,b));
	SetTextColor(offScreenDC(), RGB(r,g,b));
}

POINT SimpleCanvas::measureString(const TCHAR* str, bool vertical)
{
	RECT r = { 0, 0, 0, 0 };
	HGDIOBJ oldFont = 0;
	if (vertical)
		oldFont = SelectObject(offScreenDC(), sDefaultFontVertical);
	DrawTextEx(offScreenDC(), const_cast<TCHAR*>(str), -1, &r, DT_LEFT | DT_TOP | DT_CALCRECT, 0);
	POINT p = { r.right-r.left, r.bottom-r.top };
	if (vertical) {
		SelectObject(offScreenDC(), oldFont);
		std::swap(p.x, p.y);
	}
	return p;
}

void SimpleCanvas::drawString(int x0, int y0, const TCHAR* str, bool centered, bool vertical)
{
	int bkMode = SetBkMode(offScreenDC(), TRANSPARENT);
	RECT r = { x0, y0, x0, y0 };
	transformRect(&r);
	HGDIOBJ oldFont = 0;
	if (vertical)
		oldFont = SelectObject(offScreenDC(), sDefaultFontVertical);
	DrawTextEx(offScreenDC(), const_cast<TCHAR*>(str), -1, &r, DT_LEFT | DT_TOP | DT_CALCRECT, 0);
	int w = r.right-r.left, h = r.bottom-r.top;
	if (vertical) {
		// Rotate the rectangle.
		r.right = r.left + h;
		r.bottom = r.top + w;
		std::swap(r.top, r.bottom);
		if (centered)
			OffsetRect(&r, -h/2, mYDir*w/2);
	} else {
		if (centered)
			OffsetRect(&r, -w/2, mYDir*h/2);
	}
	DrawTextEx(offScreenDC(), const_cast<TCHAR*>(str), -1, &r, DT_LEFT | DT_TOP, 0);
	if (vertical)
		SelectObject(offScreenDC(), oldFont);

	SetBkMode(offScreenDC(), bkMode);
	InvalidateRect(handle(), &r, FALSE);
}

void SimpleCanvas::fillRect(int x0, int y0, int x1, int y1)
{
	RECT r = { x0, y0, x1, y1 };
	transformRect(&r);
	FillRect(offScreenDC(), &r, mActiveBrush);
	InvalidateRect(handle(), &r, FALSE);
}

void SimpleCanvas::drawRect(int x0, int y0, int x1, int y1)
{
	RECT r = { x0, y0, x1, y1 };
	transformRect(&r);
	FrameRect(offScreenDC(), &r, mActiveBrush);
	InvalidateRect(handle(), &r, FALSE);
}

void SimpleCanvas::fillEllipse(int x0, int y0, int x1, int y1)
{
	RECT r = { x0, y0, x1, y1 };
	transformRect(&r);
	if (abs(x1-x0) == 1 || abs(y1-y0) == 1) {      // special case: for single-pixel, Ellipse() 
		FillRect(offScreenDC(), &r, mActiveBrush); // doesn't draw anything, so use FillRect()
	} else {
		HGDIOBJ oldBrush = SelectObject(offScreenDC(), mActiveBrush);
		HGDIOBJ oldPen = SelectObject(offScreenDC(), mActivePen);
		Ellipse(offScreenDC(), r.left, r.top, r.right, r.bottom);
		SelectObject(offScreenDC(), oldBrush);
		SelectObject(offScreenDC(), oldPen);
	}
	InvalidateRect(handle(), &r, FALSE);
}

void SimpleCanvas::drawEllipse(int x0, int y0, int x1, int y1)
{
	RECT r = { x0, y0, x1, y1 };
	transformRect(&r);
	if (abs(x1-x0) == 1 || abs(y1-y0) == 1) {      // special case: for single-pixel, Ellipse() 
		FillRect(offScreenDC(), &r, mActiveBrush); // doesn't draw anything, so use FillRect()
	} else {
		HGDIOBJ oldBrush = SelectObject(offScreenDC(), GetStockObject(NULL_BRUSH));
		HGDIOBJ oldPen = SelectObject(offScreenDC(), mActivePen);
		Ellipse(offScreenDC(), r.left, r.top, r.right, r.bottom);
		SelectObject(offScreenDC(), oldBrush);
		SelectObject(offScreenDC(), oldPen);
	}
	InvalidateRect(handle(), &r, FALSE);
}

void SimpleCanvas::drawLine(int x0, int y0, int x1, int y1)
{
	int pts[4] = { x0, y0, x1, y1 };
	drawPolyline(pts, 2);
}

void SimpleCanvas::fillPoly(int* pts, int count)
{
	HGDIOBJ oldBrush = SelectObject(offScreenDC(), mActiveBrush);
	HGDIOBJ oldPen = SelectObject(offScreenDC(), mActivePen);
	POINT* ptsXform = new POINT[count];
	for (int i = 0; i < count; ++i) {
		ptsXform[i].x =       pts[i*2  ] + mX0;
		ptsXform[i].y = mYDir*pts[i*2+1] + mY0;
	}
	Polygon(offScreenDC(), ptsXform, count);
	delete [] ptsXform;
	SelectObject(offScreenDC(), oldBrush);
	SelectObject(offScreenDC(), oldPen);
	InvalidateRect(handle(), NULL, FALSE);
}

void SimpleCanvas::drawPolyline(int* pts, int count)
{
	HGDIOBJ oldBrush = SelectObject(offScreenDC(), GetStockObject(NULL_BRUSH));
	HGDIOBJ oldPen = SelectObject(offScreenDC(), mActivePen);
	POINT* ptsXform = new POINT[count];
	for (int i = 0; i < count; ++i) {
		ptsXform[i].x =       pts[i*2  ] + mX0;
		ptsXform[i].y = mYDir*pts[i*2+1] + mY0;
	}
	Polyline(offScreenDC(), ptsXform, count);
	delete [] ptsXform;
	SelectObject(offScreenDC(), oldBrush);
	SelectObject(offScreenDC(), oldPen);
	InvalidateRect(handle(), NULL, FALSE);
}


void SimpleCanvas::drawBitmap(int x0, int y0, const BitmapInfo& bmp)
{
	stretchBitmap(x0, y0, x0 + mYDir*bmp.dib.dsBmih.biWidth, y0 + mYDir*bmp.dib.dsBmih.biHeight, bmp);
}

void SimpleCanvas::stretchBitmap(int x0, int y0, int x1, int y1, const BitmapInfo& bmp)
{
	RECT r = { x0, y0, x1, y1 };
	transformRect(&r);
	if (mYDir < 0) {
		OffsetRect(&r, 0, r.bottom-r.top);
	}
	if (bmp.dib.dsBm.bmBitsPixel == 32 && bmp.has_transparency) {
		// alpha blending enabled ... but can be sloowwww because on windows the image must be pre-multiplied by alpha
		HBITMAP bm = (HBITMAP)CopyImage(bmp.osHandle, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
		if (!bm)
			throw std::runtime_error("CopyImage failed");
		DIBSECTION dib;
		GetObject(bm, sizeof(DIBSECTION), &dib);
		for (int y = 0; y < dib.dsBm.bmHeight; ++y) {
			unsigned char* p = (unsigned char*)dib.dsBm.bmBits + dib.dsBm.bmWidthBytes*y;
			unsigned char* p_end = p + dib.dsBm.bmWidth*4;
			while (p < p_end) {
				p[0] = p[0]*p[3]/255;
				p[1] = p[1]*p[3]/255;
				p[2] = p[2]*p[3]/255;
				p += 4;
			}
		}

		HDC dc = CreateCompatibleDC(offScreenDC());
		SelectObject(dc, bm);
		BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, (dib.dsBm.bmBitsPixel == 32) ? AC_SRC_ALPHA : 0};
		AlphaBlend(offScreenDC(), r.left, r.top, r.right-r.left, r.bottom-r.top, dc, 0, 0, 
			dib.dsBmih.biWidth, dib.dsBmih.biHeight, blend); 
		DeleteDC(dc);
		DeleteObject(bm);
	} else {
		struct {
			BITMAPINFOHEADER bmih;
			RGBQUAD pal[256];
		} bmi;
		bmi.bmih = bmp.dib.dsBmih;
		if (bmi.bmih.biHeight > 0)
			bmi.bmih.biHeight = -bmi.bmih.biHeight; // signify top-down bitmap
		if (bmi.bmih.biBitCount == 8) {
			// Build a grayscale palette since I can't figure out how to get freaking Windows to do this automatically.
			bmi.bmih.biClrUsed = 256;
			bmi.bmih.biClrImportant = 256;
			for (int i = 0; i < 256; ++i) { // grayscale palette
				bmi.pal[i].rgbBlue = bmi.pal[i].rgbGreen = bmi.pal[i].rgbRed = (unsigned char)i;
				bmi.pal[i].rgbReserved = 255;
			}
		}
		StretchDIBits(offScreenDC(), r.left, r.top, r.right-r.left, r.bottom-r.top, 
			0, 0, bmp.dib.dsBmih.biWidth, bmp.dib.dsBmih.biHeight,
			bmp.dib.dsBm.bmBits, (const BITMAPINFO*)&bmi, DIB_RGB_COLORS, SRCCOPY);
	}
	InvalidateRect(handle(), &r, FALSE);
}

HDC SimpleCanvas::DC()
{
	if (!mOwnDC) {
		mOwnDC = GetDC(handle());
		if (!mOwnDC)
			throw std::runtime_error("GetDC failed");
	}
	return mOwnDC;
}

HDC SimpleCanvas::offScreenDC()
{
	if (!mOffScreenDC) {
		mOffScreenDC = CreateCompatibleDC(DC());
		if (!mOffScreenDC)
			throw std::runtime_error("CreateCompatibleDC failed");
		if (!mOffScreenBM)
			setBitmapSize(clientSizeX(), clientSizeY());
		SelectObject(mOffScreenDC, sDefaultFont);
	}
	return mOffScreenDC;
}

void SimpleCanvas::getWndClassStyle(WNDCLASS& wndClass) const
{
	Super::getWndClassStyle(wndClass);
	wndClass.style |= CS_OWNDC; // lets us keep our mOwnDC without releasing it
}

bool SimpleCanvas::onMouseDown(int x, int y, int button)
{
	SetFocus(handle());
	return Super::onMouseDown(x, y, button);
}

bool SimpleCanvas::onPaint()
{
	// Copy the off-screen bitmap onto the screen
	SIZE sizeOfBM;
	GetBitmapDimensionEx(mOffScreenBM, &sizeOfBM);
	BitBlt(DC(), 0, 0, sizeOfBM.cx, sizeOfBM.cy,
	       offScreenDC(), 0, 0, SRCCOPY);

	// Fill any unused area with some default colour
	RECT fillX = { sizeOfBM.cx, 0, std::max((int)sizeOfBM.cx, clientSizeX()), clientSizeY() };
	RECT fillY = { 0, sizeOfBM.cy, clientSizeX(), std::max((int)sizeOfBM.cy, clientSizeY()) };
	FillRect(DC(), &fillX, 0);
	FillRect(DC(), &fillY, 0);
	return true;
}

void SimpleCanvas::transformRect(RECT* r)
{
	if (mYDir < 0) {
		int temp = r->bottom;
		r->bottom = -r->top+1;
		r->top = -temp+1;
	}
	OffsetRect(r, mX0, mY0);
}


//////////////////////////////////////////////////////////////////////
// SimpleCanvasApp

class SimpleCanvasApp: public SimpleCanvas {
	typedef SimpleCanvas Super;
public:
	SimpleCanvasApp(const char* name);
	virtual void  setPositionAndSize(int x, int y, int windowSizeX, int windowSizeY);
	virtual void  setSize(int windowSizeX, int windowSizeY);
protected:
	virtual ~SimpleCanvasApp();
	virtual void    getWndStyle(DWORD& dwStyle, DWORD& dwExStyle) const;
	virtual void    realize();
	virtual bool    onClose();
};

SimpleCanvasApp::SimpleCanvasApp(const char* name)
: Super(name)
{
}

SimpleCanvasApp::~SimpleCanvasApp()
{
}

void SimpleCanvasApp::setPositionAndSize(int x, int y, int windowSizeX, int windowSizeY)
{
	RECT clientRect, windowRect;
	GetClientRect(handle(), &clientRect);
	GetWindowRect(handle(), &windowRect);  // setSize should set the size of the *client* area
	int marginX = (windowRect.right-windowRect.left)-(clientRect.right-clientRect.left);
	int marginY = (windowRect.bottom-windowRect.top)-(clientRect.bottom-clientRect.top);
	Super::setPositionAndSize(x, y, windowSizeX + marginX, windowSizeY + marginY);
}

void SimpleCanvasApp::setSize(int windowSizeX, int windowSizeY)
{
	RECT clientRect, windowRect;
	GetClientRect(handle(), &clientRect);
	GetWindowRect(handle(), &windowRect);  // setSize should set the size of the *client* area
	int marginX = (windowRect.right-windowRect.left)-(clientRect.right-clientRect.left);
	int marginY = (windowRect.bottom-windowRect.top)-(clientRect.bottom-clientRect.top);
	Super::setSize(windowSizeX + marginX, windowSizeY + marginY);
}

void SimpleCanvasApp::realize()
{
	Super::realize();
#ifdef WINDOW_ALPHA
	SetLayeredWindowAttributes(handle(), 0, (255 * WINDOW_ALPHA) / 100, LWA_ALPHA);
#endif
}

void SimpleCanvasApp::getWndStyle(DWORD& dwStyle, DWORD& dwExStyle) const
{
	Super::getWndStyle(dwStyle, dwExStyle);
	dwExStyle |= WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
#ifdef WINDOW_ALPHA
	dwExStyle |= WS_EX_LAYERED;
#endif
	dwStyle   |= WS_OVERLAPPEDWINDOW;
	dwStyle   &= ~(WS_CHILD | WS_CHILDWINDOW | WS_VISIBLE);
}

bool SimpleCanvasApp::onClose()
{
	gQuit = true;
	return true; // allow window to be destroyed
}


//////////////////////////////////////////////////////////////////////
// PlotCtl


class PlotCtl: public SimpleCanvas {
	typedef SimpleCanvas Super;
public:
	PlotCtl(const char* name);

	void setTitle(const char* title);
	void setAxisLabels(const char* labelX, const char* labelY);
	int  addSeries(double* x, double* y, int count, const char* title, const char* style);
	void addSeriesPoint(int seriesID, double x, double y);
	int  seriesCount() const;

protected:
	virtual ~PlotCtl();
	virtual void    getWndStyle(DWORD& dwStyle, DWORD& dwExStyle) const;
	virtual bool    onPaint();

	void redrawPlot();
	void setDirty();
	void formatAxisLabel(TCHAR* dst, int size, double value);

	enum Shape { cNoShape, cStar, cPlus, cCross, cPoint, cNumSeriesShapes };
	enum Line  { cNoLine, cSolid, cDotted, cNumSeriesLines };

	struct Series {
		std::vector<double> x;
		std::vector<double> y;
		wstr title;
		unsigned char colour[3];
		Shape shape;
		Line line;
	};

	int mNextSeriesStyle;
	std::map<int,Series> mSeries;
	wstr mTitle;
	wstr mAxisLabelX, mAxisLabelY;
	double mAxisMinX, mAxisMinY;
	double mAxisMaxX, mAxisMaxY;
	bool mAutoAxis;
	bool mDrawAxisLabels;
	bool mDrawLegend;
	bool mDirty;

	static int sNextSeriesID;
	static const int cNumSeriesColours = 6;
	static unsigned char sSeriesColours[cNumSeriesColours][3];
};

int PlotCtl::sNextSeriesID = 1;
unsigned char PlotCtl::sSeriesColours[][3] = {
	 {   0,   0, 220 }
	,{ 220,   0,   0 }
	,{   0, 220,   0 }
	,{ 220, 128,   0 }
	,{ 190,   0, 200 }
	,{ 128, 128, 128 }
};

PlotCtl::PlotCtl(const char* name)
: Super(name)
, mNextSeriesStyle(0)
, mDirty(true)
, mAxisMinX(0.0), mAxisMinY(0.0)
, mAxisMaxX(1.0),  mAxisMaxY(1.0)
, mAutoAxis(true)
, mDrawAxisLabels(true)
, mDrawLegend(true)
{
}

PlotCtl::~PlotCtl()
{
}

void PlotCtl::setTitle(const char* title)
{
	mTitle = char2wstr(title);
	setDirty();
}

void PlotCtl::setAxisLabels(const char* labelX, const char* labelY)
{
	mAxisLabelX = char2wstr(labelX);
	mAxisLabelY = char2wstr(labelY);
	setDirty();
}

int PlotCtl::addSeries(double* x, double* y, int count, const char* title, const char* style)
{
	setDirty();

	int series = sNextSeriesID++;
	Series& s = mSeries[series];
	if (title) {
		s.title = char2wstr(title);
	} else {
		char str[128];
		sprintf_s(str, 128, "Series %u", series);
		s.title = char2wstr(str);
	}

	// Copy x/y data, if any
	if (count > 0) {
		if (x) {
			s.x.assign(x, x+count);
		} else {
			s.x.resize(count);
			for (int i = 0; i < count; ++i)
				s.x[i] = i;
		}
		s.y.assign(y, y+count);
	}

	// Set up either a custom line style, or generate a default.
	if (style && strstr(style, "*"))
		s.shape = cStar;
	else if (style && strstr(style, "."))
		s.shape = cPoint;
	else if (style && strstr(style, "+"))
		s.shape = cPlus;
	else if (style && strstr(style, "x"))
		s.shape = cCross;
	else
		s.shape = (Shape)(1+(mNextSeriesStyle / cNumSeriesColours) % (cNumSeriesShapes-1));

	// Set up either custom colour, or generate a default.
	memset(s.colour, 0, 3);
	if (style && strstr(style, "r"))
		s.colour[0] = 255;
	else if (style && strstr(style, "b"))
		s.colour[2] = 255;
	else if (style && strstr(style, "g"))
		s.colour[1] = 192;
	else if (style && strstr(style, "y"))
		s.colour[0] = s.colour[1] = 192;
	else if (style && strstr(style, "c"))
		s.colour[2] = s.colour[1] = 192;
	else if (style && strstr(style, "m"))
		s.colour[2] = s.colour[0] = 192;
	else
		memcpy(s.colour, sSeriesColours[mNextSeriesStyle], 3);

	
	// Line style
	if (style && strstr(style, "-"))
		s.line = cSolid;
	else if (style && strstr(style, ":"))
		s.line = cDotted;
	else 
		s.line = cNoLine;

	mNextSeriesStyle++;
	return series;
}

void PlotCtl::addSeriesPoint(int seriesID, double x, double y)
{
	std::map<int,Series>::iterator si = mSeries.find(seriesID);
	if (si == mSeries.end())
		throw std::runtime_error("Invalid series ID passed to addSeriesPoint.");
	Series& s = si->second;
	s.x.push_back(x);
	s.y.push_back(y);
	setDirty();
}

int  PlotCtl::seriesCount() const { return (int)mSeries.size(); }

void PlotCtl::getWndStyle(DWORD& dwStyle, DWORD& dwExStyle) const
{
	Super::getWndStyle(dwStyle, dwExStyle);
#ifdef WINDOW_ALPHA
	dwExStyle |= WS_EX_LAYERED;
#endif
}

bool PlotCtl::onPaint()
{
	if (mDirty)
		redrawPlot();
	return Super::onPaint();
}

void PlotCtl::redrawPlot()
{
	mDirty = false;
	int sizeX = clientSizeX(), sizeY = clientSizeY();
	setAxisOrigin(0, sizeY-1);
	setAxisFlipY(true);

	// Compute scale of data when plotting
	int plotMinX = 0, plotMaxX = sizeX-1;
	int plotMinY = 0, plotMaxY = sizeY-1;
	double minX = mAxisMinX, minY = mAxisMinY;
	double maxX = mAxisMaxX, maxY = mAxisMaxY;
	if (mAutoAxis) {
		minX = minY =  1e10;
		maxX = maxY = -1e10;
		// Calculate min/max data points
		for (std::map<int,Series>::const_iterator si = mSeries.begin(); si != mSeries.end(); ++si) {
			const Series& s = si->second;
			if (s.x.empty())
				continue;
			minX = std::min(minX, *std::min_element(s.x.begin(), s.x.end()));
			maxX = std::max(maxX, *std::max_element(s.x.begin(), s.x.end()));
			minY = std::min(minY, *std::min_element(s.y.begin(), s.y.end()));
			maxY = std::max(maxY, *std::max_element(s.y.begin(), s.y.end()));
		}
		if (minX > maxX) { // if all series were empty, default to [0,1] square
			minX = minY = 0;
			maxX = maxY = 1;
		} else if (minX == maxX) { // if all data points were the same, plot around them
			maxX += 1;
			maxY += 1;
		}
	}

	POINT ptitle = measureString(mTitle.c_str());

	// Clear drawing area
	setColour(255, 255, 255);
	fillRect(0, 0, sizeX, sizeY);

	// Frame and axis tickmarks
	TCHAR minXstr[32], maxXstr[32], minYstr[32], maxYstr[32];
	POINT pminX, pmaxX, pminY, pmaxY;
	if (mDrawAxisLabels) {
		formatAxisLabel(minXstr, 32, minX);    // Get string for each axis label.
		formatAxisLabel(maxXstr, 32, maxX);     
		formatAxisLabel(minYstr, 32, minY);    
		formatAxisLabel(maxYstr, 32, maxY);    
		pminX = measureString(minXstr);         // Measure the size that each label
		pmaxX = measureString(maxXstr);         // will take when we draw them.
		pminY = measureString(minYstr);
		pmaxY = measureString(maxYstr);
		plotMinX += std::max(pminX.x/2,std::max(pminY.x, pmaxY.x))+6;     // Add enough margin to fit axis labes
		plotMaxX -= pmaxX.x/2+2;                      // outside theplot area.
		plotMinY += pminX.y+2;
		plotMaxY -= ptitle.y+8;
	}

	POINT plabelX, plabelY;
	if (!mAxisLabelY.empty()) {
		plabelY = measureString(mAxisLabelY.c_str(), true);
		plotMinX = std::max(plotMinX, (int)plabelY.x+6);
	}
	if (!mAxisLabelX.empty()) {
		plabelX = measureString(mAxisLabelX.c_str());
		plotMinY = std::max(plotMinY, (int)plabelX.y+6);
	}

	int plotSizeX = plotMaxX-plotMinX; // finished computing position of 
	int plotSizeY = plotMaxY-plotMinY; // plot area, so start drawing

	if (mDrawAxisLabels) {
		setColour(192, 192, 192);
		drawRect(plotMinX, plotMinY, plotMaxX+1, plotMaxY+1);
		setColour(0, 0, 0);
		drawString(plotMinX, plotMinY-pminX.y/2-2, minXstr, true);
		drawString(plotMaxX, plotMinY-pmaxX.y/2-2, maxXstr, true);
		drawString(plotMinX-pminY.x/2-4, plotMinY+1, minYstr, true);
		drawString(plotMinX-pmaxY.x/2-4, plotMaxY+1, maxYstr, true);
	}

	if (!mAxisLabelY.empty())
		drawString(plotMinX-plabelY.x/2-4, plotMinY+plotSizeY/2, mAxisLabelY.c_str(), true, true);
	if (!mAxisLabelX.empty()) 
		drawString(plotMinX+plotSizeX/2, plotMinY-plabelX.y/2-4, mAxisLabelX.c_str(), true);

	// Origin axes
	setColour(0, 0, 0);
	if (minY <= 0 && maxY >= 0) {   // draw X axis if it's in view
		int y0 = plotMinY + (int)(-minY / (maxY-minY) * plotSizeY);
		drawLine(plotMinX, y0, plotMaxX+1, y0);
	}
	if (minX <= 0 && maxX >= 0) {   // draw Y axis if it's in view
		int x0 = plotMinX + (int)(-minX / (maxX-minX) * plotSizeX);
		drawLine(x0, plotMinY, x0, plotMaxY+1);
	}

	// Title
	setColour(0, 0, 0);
	drawString(plotMinX + plotSizeX/2, plotMaxY+ptitle.y/2+4, mTitle.c_str(), true);

	// Series points
	for (std::map<int,Series>::const_iterator si = mSeries.begin(); si != mSeries.end(); ++si) {
		const Series& s = si->second;
		int count = (int)s.x.size();
		if (count == 0)
			continue;

		// Convert from data coordinates to plot coordinates
		const double* xs = &s.x[0];
		const double* ys = &s.y[0];
		POINT* pts = new POINT[count];
		for (int i = 0; i < count; ++i) {
			pts[i].x = plotMinX + (int)((xs[i]-minX) / (maxX-minX) * plotSizeX + .4999f);
			pts[i].y = plotMinY + (int)((ys[i]-minY) / (maxY-minY) * plotSizeY + .4999f);
		}

		setColour(s.colour[0], s.colour[1], s.colour[2]);

		// Draw connected lines if style is set.
		if (s.line != cNoLine) { 
			drawPolyline((int*)pts, count);
		}

		// Draw shapes at each point in the series.
		if (s.shape == cNoShape)
			continue;
		for (int i = 0; i < count; ++i) {
			int x = pts[i].x;
			int y = pts[i].y;
			switch (s.shape) {
				case cPoint: { fillEllipse(x-1, y-1, x+2, y+2); break; }
				case cPlus:  { drawLine(x-3, y, x+4, y);     drawLine(x, y-3, x, y+4); break; }
				case cCross: { drawLine(x-3, y-3, x+4, y+4); drawLine(x-3, y+3, x+4, y-4); break; }
				case cStar: { drawLine(x-3, y, x+4, y);     drawLine(x, y-3, x, y+4);
				              drawLine(x-2, y-2, x+3, y+3); drawLine(x-2, y+2, x+3, y-3); break; }
			}
		}
		delete [] pts;
	}

	// Legend
	if (mDrawLegend && !mSeries.empty()) {
		int legendX = plotMinX+std::max(plotSizeX/10, 5),  legendY = plotMaxY-10;
		int legendSizeX = 0, legendSizeY = 0;

		int itemHeight = 0;
		// First calculate size of legend based on series titles.
		for (std::map<int,Series>::const_iterator si = mSeries.begin(); si != mSeries.end(); ++si) {
			const Series& s = si->second;
			POINT p = measureString(s.title.c_str());
			itemHeight = p.y;
			if (legendSizeX < p.x)
				legendSizeX = p.x;
			legendSizeY += p.y;
		}

		legendSizeX += 25;
		legendSizeY += 6;

		// Draw frame around legend
		setColour(255, 255, 255);
		fillRect(legendX, legendY-legendSizeY, legendX+legendSizeX, legendY);
		setColour(192, 192, 192);
		drawRect(legendX, legendY-legendSizeY, legendX+legendSizeX, legendY);

		// Draw each series title into the legend area
		int itemY = legendY-3;
		for (std::map<int,Series>::const_iterator si = mSeries.begin(); si != mSeries.end(); ++si) {
			const Series& s = si->second;
			setColour(s.colour[0], s.colour[1], s.colour[2]);

			// Draw title text
			drawString(legendX+22, itemY, s.title.c_str());

			// Draw representation of the data shapes/lines, if any
			int x = legendX+11, y = itemY-itemHeight/2-1;
			if (s.line != cNoLine)
				drawLine(x-8, y, x+9, y);

			// Draw shapes at each point in the series.
			switch (s.shape) {
				case cPoint: { fillEllipse(x-1, y-1, x+2, y+2); break; }
				case cPlus:  { drawLine(x-3, y, x+4, y);     drawLine(x, y-3, x, y+4); break; }
				case cCross: { drawLine(x-3, y-3, x+4, y+4); drawLine(x-3, y+3, x+4, y-4); break; }
				case cStar:  { drawLine(x-3, y, x+4, y);     drawLine(x, y-3, x, y+4);
				               drawLine(x-2, y-2, x+3, y+3); drawLine(x-2, y+2, x+3, y-3); break; }
			}

			itemY -= itemHeight;
		}
	}
}

void PlotCtl::setDirty()
{
	mDirty = true;
	InvalidateRect(handle(), NULL, FALSE);
}

void PlotCtl::formatAxisLabel(TCHAR *dst, int size, double value)
{
	int ival = (int)(value + .000000001);
	if (value > 4096 && (ival & (~ival+1)) == ival) { // if ival is a power of two
		int e = -1;
		while (ival) {  // count the trailing zero bits 
			ival >>=  1;
			++e; 
		}
		sprintf_TEXT(dst, size, TEXT("2^%u"), e); 
	} else {
		sprintf_TEXT(dst, size, TEXT("%g"), value);
	}
}

//////////////////////////////////////////////////////////////////////
// Simple 1037 API redirects all calls to a default global Window object

static unsigned gMainThreadID = 0;
#define AssertIsMainThread() AssertMsg(gMainThreadID == 0 || _threadid == gMainThreadID, "Only the main() thread can call this function.")

static std::map<int, BitmapInfo>& sBitmapRegistry() {
	AssertIsMainThread();
	static std::map<int, BitmapInfo> registry;
	return registry;
}
static int gNextBitmapID = 1;

// These are global functions with static data so that students can initialize 
// global variables with calls to CreateButton etc:
//    int checkBox = CreateCheckBox("text", handler);
// and not run into any global data initialization-order problems 
// (std::map constructor for example).

#define DECL_REGISTRY(name,type)                 \
	static std::map<int, type*>& name() {        \
		AssertIsMainThread();                    \
		static std::map<int, type*> registry;    \
		return registry;                         \
	}

DECL_REGISTRY(sTextLabelRegistry, StaticCtl)
DECL_REGISTRY(sButtonRegistry,    ButtonCtl)
DECL_REGISTRY(sDropListRegistry,  ComboBoxCtl)
DECL_REGISTRY(sCheckBoxRegistry,  CheckBoxCtl)
DECL_REGISTRY(sTextBoxRegistry,   EditCtl)
DECL_REGISTRY(sSliderRegistry,    TrackbarCtl)
DECL_REGISTRY(sPlotRegistry,      PlotCtl)
DECL_REGISTRY(sPlotSeriesRegistry,PlotCtl)

static int gNextCtrlID = 1;

static std::list<MSG> gKeyboardInputQueue;
static std::list<MSG> gMouseInputQueue;

static SimpleCanvasApp* theMainWindow()
{
	AssertIsMainThread();
	// NOT thread safe, obviously
	static SimpleCanvasApp* sMainWindow = 0;
	if (!sMainWindow) {
		INITCOMMONCONTROLSEX initcc;
		initcc.dwSize = sizeof(initcc);
		initcc.dwICC = ICC_STANDARD_CLASSES;
		InitCommonControlsEx(&initcc);
		sMainWindow = new SimpleCanvasApp("main");
		sMainWindow->setPositionAndSize(200, 100, 640, 480);
		sMainWindow->setBitmapSize(2048, 2048);
		sMainWindow->setColour(255,255,255);
		sMainWindow->fillRect(0, 0, 1280, 1024);
		sMainWindow->setColour(0,0,0);
	}
	return sMainWindow;
}

static void sShowDialog_(const char* title, const char* format, va_list args)
{
	int textLen = _vscprintf(format, args);
	char* message = new char[textLen+1];
	vsprintf_s(message, textLen+1, format, args);
	theMainWindow()->setVisible(true);
	MessageBoxA(theMainWindow()->handle(), message, title, MB_OK);
	delete [] message;
}

static void sShowDialog(const char* title, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	sShowDialog_(title, format, args);
	va_end(args);
}

void sAssertDialog(const char* msg, const char* file, int line)
{
	const char* nameptr = strrchr(file, '\\');
	if (!nameptr) nameptr = strrchr(file, '/');
	if (!nameptr) nameptr = file;
	sShowDialog("ASSERT failed!", "%s\n\n%s line %d", msg, nameptr, line);
}

static Window* sGetControl(int handle, const char* callerName)
{
	AssertIsMainThread();
	Window* ctl = NULL;
	if (sTextLabelRegistry().find(handle) != sTextLabelRegistry().end())
		ctl = sTextLabelRegistry()[handle];
	else if (sButtonRegistry().find(handle) != sButtonRegistry().end())
		ctl = sButtonRegistry()[handle];
	else if (sDropListRegistry().find(handle) != sDropListRegistry().end())
		ctl = sDropListRegistry()[handle];
	else if (sCheckBoxRegistry().find(handle) != sCheckBoxRegistry().end())
		ctl = sCheckBoxRegistry()[handle];
	else if (sTextBoxRegistry().find(handle) != sTextBoxRegistry().end())
		ctl = sTextBoxRegistry()[handle];
	else if (sSliderRegistry().find(handle) != sSliderRegistry().end())
		ctl = sSliderRegistry()[handle];
	else if (sPlotRegistry().find(handle) != sPlotRegistry().end())
		ctl = sPlotRegistry()[handle];
	if (!ctl) {
		char msg[512];
		sprintf_s(msg, 512, "You called %s with an invalid control handle.", callerName);
		AssertUnreachable(msg);
	}
	return ctl;
}

inline static BitmapInfo* sGetBitmapInfo(int handle, const char* callerName)
{
	AssertIsMainThread();
	// Do a bit of caching so that subsequent calls on the same
	// image will avoid lookups in sBitmapRegistry().
	static BitmapInfo* bmp = 0;
	static int bmpHandle = 0;
	if (!bmp || handle != bmpHandle) {
		if (sBitmapRegistry().find(handle) != sBitmapRegistry().end()) {
			bmp = &sBitmapRegistry()[handle];
			bmpHandle = handle;
		} else {
			bmp = 0;
			bmpHandle = 0;
		}
	}
	if (!bmp) {
		char msg[512];
		if (handle == 0)
			sprintf_s(msg, 512, "You called %s with an invalid image handle.\nMaybe CreateImageFromFile failed?", callerName);
		else
			sprintf_s(msg, 512, "You called %s with an invalid image handle.\nMaybe you already deleted this image?", callerName);
		AssertUnreachable(msg);
	}
	return bmp;
}

static void sPumpMessageQueue()
{
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT)
			gQuit = true;

		if (!IsDialogMessage(theMainWindow()->handle(), &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST)
			gKeyboardInputQueue.push_back(msg);
		else if (msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST)
			gMouseInputQueue.push_back(msg);
	}
}

static bool sWantKeyboardEvent(MSG& msg, char c)
{
	// Do not report non-ASCII keys, and do not report tabs
	if (!c || c == '\t') 
		return false;

	// Report any key event received directly by main window 
	Window* w = Window::sWindowFromHandle(msg.hwnd);
	if (w == static_cast<Window*>(theMainWindow()))
		return true;

	// Only report events for children of the main window
	if (!IsChild(theMainWindow()->handle(), msg.hwnd))
		return false;

	// ButtonCtls and CheckBoxCtls eat the spacebar
	if (dynamic_cast<ButtonCtl*>(w) || dynamic_cast<CheckBoxCtl*>(w))
		return c != ' ';

	// Text controls eat everything
	if (dynamic_cast<EditCtl*>(w))
		return false;

	return true;
}

bool GetKeyboardInput(char* key)
{
	AssertIsMainThread();
	sPumpMessageQueue();
	while (!gKeyboardInputQueue.empty()) {
		MSG msg = gKeyboardInputQueue.front(); gKeyboardInputQueue.pop_front();
		if (msg.message == WM_KEYDOWN) {
			char c = (char)MapVirtualKey((int)msg.wParam, 2); // 2 = MAPVK_VK_TO_CHAR
			c = (char)tolower(c);
			if (sWantKeyboardEvent(msg, c)) {
				if (key)
					*key = c;
				return true;
			}
		}
	}
	if (_kbhit()) { // don't let students get confused, poll both window and console
		if (key)
			*key = (char)_getch();
		return true;
	}
	Sleep(1); // hack to avoid saturating CPU
	return false;
}

bool GetMouseInput(int* mouseX, int* mouseY, int* mouseButton)
{
	AssertIsMainThread();
	int ydir = theMainWindow()->axisFlipY() ? -1 : 1;
	sPumpMessageQueue();
	while (!gMouseInputQueue.empty()) {
		MSG msg = gMouseInputQueue.front(); gMouseInputQueue.pop_front();
		if (Window::sWindowFromHandle(msg.hwnd) != static_cast<Window*>(theMainWindow()))
			continue;
		int x =  GET_X_LPARAM(msg.lParam) - theMainWindow()->axisOriginX();
		int y = (GET_Y_LPARAM(msg.lParam) - theMainWindow()->axisOriginY())*ydir;
		switch (msg.message) {
			case WM_MOUSEMOVE:   { if (!gMouseInputQueue.empty()) break;
			                       *mouseX = x; *mouseY = y; *mouseButton =  0; return true; }
			case WM_LBUTTONDOWN: { *mouseX = x; *mouseY = y; *mouseButton =  1; return true; }
			case WM_RBUTTONDOWN: { *mouseX = x; *mouseY = y; *mouseButton =  2; return true; }
			case WM_MBUTTONDOWN: { *mouseX = x; *mouseY = y; *mouseButton =  3; return true; }
			case WM_LBUTTONUP:   { *mouseX = x; *mouseY = y; *mouseButton = -1; return true; }
			case WM_RBUTTONUP:   { *mouseX = x; *mouseY = y; *mouseButton = -2; return true; }
			case WM_MBUTTONUP:   { *mouseX = x; *mouseY = y; *mouseButton = -3; return true; }
		}
	}
	Sleep(1); // hack to avoid saturating CPU
	return false;
}

void Pause(int milliseconds)
{
	if (_threadid == gMainThreadID)
		sPumpMessageQueue();  // give the window a chance to redraw before sleep.
	Sleep(milliseconds);
	if (_threadid == gMainThreadID)
		sPumpMessageQueue();
}

bool WasWindowClosed()
{
	Sleep(1); // hack to avoid saturating CPU
	sPumpMessageQueue();
	return gQuit;
}

void SetWindowVisible(bool visible)
{
	theMainWindow()->setVisible(visible);
	sPumpMessageQueue();
}

void SetWindowTitle(const char* title)
{
	theMainWindow()->setText(char2wstr(title).c_str());
	sPumpMessageQueue();
}

void SetWindowPosition(int x, int y)
{
	theMainWindow()->setPosition(x, y);
	sPumpMessageQueue();
}

void SetWindowSize(int width, int height)
{
	theMainWindow()->setSize(width, height);
	sPumpMessageQueue();
}

void DrawText(int x0, int y0, const char* str)
{
	theMainWindow()->drawString(x0, y0, char2wstr(str).c_str());
}

void SetDrawColour(unsigned char r, unsigned char g, unsigned char b)
{
	r = std::min((unsigned char)255, r);
	g = std::min((unsigned char)255, g);
	b = std::min((unsigned char)255, b);
	theMainWindow()->setColour(r, g, b);
}

void DrawRectangleOutline(int x0, int y0, int x1, int y1)
{
	theMainWindow()->drawRect(x0, y0, x1, y1);
}

void DrawRectangleFilled(int x0, int y0, int x1, int y1)
{
	theMainWindow()->fillRect(x0, y0, x1, y1);
}

void DrawEllipseOutline(int x, int y, int radiusX, int radiusY)
{
	theMainWindow()->drawEllipse(x-radiusX+1, y-radiusY+1, x+radiusX, y+radiusY);
}

void DrawEllipseFilled(int x, int y, int radiusX, int radiusY)
{
	theMainWindow()->fillEllipse(x-radiusX+1, y-radiusY+1, x+radiusX, y+radiusY);
}

void DrawLine(int x0, int y0, int x1, int y1)
{
	theMainWindow()->drawLine(x0, y0, x1, y1);
}

void DrawPolygonOutline(int* xy, int count)
{
	theMainWindow()->drawPolyline(xy, count);
}

void DrawPolygonFilled(int* xy, int count)
{
	theMainWindow()->fillPoly(xy, count);
}

void SetDrawAxis(int originX, int originY, bool flipY)
{
	theMainWindow()->setAxisOrigin(originX, originY);
	theMainWindow()->setAxisFlipY(flipY);
}

void DrawImage(int handle, int x0, int y0)
{
	BitmapInfo* bmp = sGetBitmapInfo(handle, "DrawImage");
	theMainWindow()->drawBitmap(x0, y0, *bmp);
}

void DrawImageStretched(int handle, int x0, int y0, int x1, int y1)
{
	BitmapInfo* bmp = sGetBitmapInfo(handle, "DrawImageStretched");
	theMainWindow()->stretchBitmap(x0, y0, x1, y1, *bmp);
}

int CreateImage(int sizeX, int sizeY, const char* options)
{
	AssertIsMainThread();
	int bpp = strstr(options, "bgra") ? 4 : 
	          strstr(options, "bgr")  ? 3 : 
	          strstr(options, "grey") ? 1 : 
	          strstr(options, "gray") ? 1 : 0;

	if (!bpp)
		return 0; // invalid options

	// Set BITMAPINFOHEADER fields
	struct {
		BITMAPINFOHEADER bmiHeader;
		RGBQUAD pal[256];
	} bmi;
	memset(&bmi.bmiHeader, 0, sizeof(BITMAPINFOHEADER));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = sizeX;
	bmi.bmiHeader.biHeight = -sizeY;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = (WORD)(bpp*8);
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biClrUsed = (bpp == 1) ? 256 : 0;
	bmi.bmiHeader.biClrImportant = bmi.bmiHeader.biClrUsed;
	for (int i = 0; i < 256; ++i) { // grayscale palette
		bmi.pal[i].rgbBlue = bmi.pal[i].rgbGreen = bmi.pal[i].rgbRed = (unsigned char)i;
		bmi.pal[i].rgbReserved = 255;
	}

	// Tell windows to allocate a DIB from the parameters we've set up
	BitmapInfo info;
	info.osHandle = CreateDIBSection(theMainWindow()->offScreenDC(), (BITMAPINFO*)&bmi, DIB_RGB_COLORS, 0, 0, 0); 
	info.has_transparency = bpp == 4; // assume it could be transparent
	
	AssertMsg(info.osHandle, "Attempt to allocate bitmap failed. Probably ran out of OS handles.");
	GetObject(info.osHandle, sizeof(DIBSECTION), &info.dib);

	int handle = gNextBitmapID++;
	sBitmapRegistry()[handle] = info;
	return handle;
}

int GetImageSizeX(int handle)
{
	BitmapInfo* bmp = sGetBitmapInfo(handle, "GetImageSizeX");
	return bmp->dib.dsBm.bmWidth;
}

int GetImageSizeY(int handle)
{
	BitmapInfo* bmp = sGetBitmapInfo(handle, "GetImageSizeY");
	return bmp->dib.dsBm.bmHeight;
}

int GetImageBytesPerPixel(int handle)
{
	BitmapInfo* bmp = sGetBitmapInfo(handle, "GetImageBytesPerPixel");
	return bmp->dib.dsBm.bmBitsPixel >> 3;
}

unsigned char* GetPixelPtr(int handle, int x, int y)
{
	BitmapInfo* bmp = sGetBitmapInfo(handle, "GetImagePixelPtr");
	int bpp = bmp->dib.dsBm.bmBitsPixel>>3;
	AssertMsg(x >= 0 && y >= 0 && x < bmp->dib.dsBm.bmWidth && y < bmp->dib.dsBm.bmHeight,
	          "Requested pixel is outside image boundary");
	return (unsigned char*)bmp->dib.dsBm.bmBits 
			+ bmp->dib.dsBm.bmWidthBytes*y + x*bpp;
}

int LoadImage(const char* fileName)
{
	AssertIsMainThread();

	bool isPNG = strstr(fileName,".png") || strstr(fileName,".PNG");
	bool isBMP = strstr(fileName,".bmp") || strstr(fileName,".BMP");
	AssertMsg(isBMP || isPNG,"CreateImageFromFile can only load .bmp or .png images");

	HANDLE bmp = 0;

	if (isPNG) {
		bmp = LoadPng(char2wstr(fileName).c_str(), NULL, NULL, TRUE);
	} else {
		// Ask Windows to do the dirty work
		bmp = LoadImageWin32(0, char2wstr(fileName).c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	}

	if (!bmp) {
		char msg[512];
		sprintf_s(msg,512,"Failed to load image file %s (wrong name? file not in path? internal format unsupported?",fileName);
		AssertMsg(bmp,msg);
	}

	// Ask for the bitmap details and remember them in the registry for easy access
	BitmapInfo info;
	info.osHandle = bmp;
	info.has_transparency = false;
	if (GetObject(bmp, sizeof(info.dib), &info.dib) != sizeof(info.dib)) {
		DeleteObject(bmp);
		return 0;
	}

	if (info.dib.dsBm.bmBitsPixel == 32) {
		// Check if there are any pixels that need to be alpha-blended
		for (int y = 0; y < info.dib.dsBm.bmHeight && !info.has_transparency; ++y) {
			unsigned char* p = (unsigned char*)info.dib.dsBm.bmBits + info.dib.dsBm.bmWidthBytes*y;
			unsigned char* p_end = p + info.dib.dsBm.bmWidth*4;
			for (; p < p_end; p += 4) {
				if (p[3] != 255) {
					info.has_transparency = true;
					break;
				}
			}
		}
	}

	int handle = 0;
	if (info.dib.dsBm.bmBitsPixel <= 8) {
		// For CreateImage and DrawImage functions, 8-bit images treated as grayscale. 
		// Any palettized image must therefore either be grayscale to begin with, OR 
		// be converted to BGR internally at load time.
		RGBQUAD palette[256];
		HDC tempDC = CreateCompatibleDC(0);
		HBITMAP oldBMP = (HBITMAP)SelectObject(tempDC, bmp);
		GetDIBColorTable(tempDC, 0, info.dib.dsBmih.biClrUsed, palette);
		int bpp = 1;
		for (unsigned i = 0; i < info.dib.dsBmih.biClrUsed; ++i) {
			if (palette[i].rgbBlue != palette[i].rgbGreen || palette[i].rgbBlue != palette[i].rgbRed) {
				bpp = 3; // palette is not greyscale
				break;
			}
		}
		SelectObject(tempDC, oldBMP);
		DeleteDC(tempDC);

		handle = CreateImage(info.dib.dsBm.bmWidth, info.dib.dsBm.bmHeight, bpp == 1 ? "gray" : "bgr");
		for (int y = 0; y < info.dib.dsBm.bmHeight; ++y) {
			unsigned char* src = (unsigned char*)info.dib.dsBm.bmBits + info.dib.dsBm.bmWidthBytes*y;
			unsigned char* dst = GetPixelPtr(handle, 0, info.dib.dsBmih.biHeight < 0 ? y : info.dib.dsBmih.biHeight-y-1); // potentially flip row order
			for (int x = 0; x < info.dib.dsBm.bmWidth; ++x) {
				unsigned char srcByte = src[x*info.dib.dsBmih.biBitCount/8];
				unsigned pindex = (srcByte >> (8-info.dib.dsBmih.biBitCount-(info.dib.dsBmih.biBitCount*x)%8)) & ((1 << info.dib.dsBmih.biBitCount)-1);
				memcpy(dst+x*bpp, palette+pindex, bpp);
			}
		}

	} else {
		// The image is not palettized, so load it directly but flip row order if necessary
		handle = CreateImage(info.dib.dsBm.bmWidth, info.dib.dsBm.bmHeight, info.dib.dsBm.bmBitsPixel == 24 ? "bgr" : "bgra");
		for (int y = 0; y < info.dib.dsBm.bmHeight; ++y) {
			unsigned char* src = (unsigned char*)info.dib.dsBm.bmBits + info.dib.dsBm.bmWidthBytes*y;
			unsigned char* dst = GetPixelPtr(handle, 0, info.dib.dsBmih.biHeight < 0 ? y : info.dib.dsBmih.biHeight-y-1); // potentially flip row order
			memcpy(dst, src, info.dib.dsBm.bmWidthBytes);
		}
		BitmapInfo* handle_bmp = sGetBitmapInfo(handle, "LoadImage");
		handle_bmp->has_transparency = info.has_transparency;
	}

	DeleteObject(bmp);
	return handle;
}

void SaveImage(int handle, const char* fileName)
{
	AssertIsMainThread();
	bool isBMP = strstr(fileName, ".bmp") == fileName+strlen(fileName)-4 
		      || strstr(fileName, ".BMP") == fileName+strlen(fileName)-4;
	AssertMsg(isBMP,"Only BMP images are supported; file name must end in .bmp");

	BitmapInfo* bmp = sGetBitmapInfo(handle, "SaveImage");

	BITMAPFILEHEADER bmfh;
	bmfh.bfType = 'B' | ('M' << 8);
	bmfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + (bmp->dib.dsBmih.biBitCount == 8 ? 256*sizeof(RGBQUAD) : 0);
	bmfh.bfSize = bmfh.bfOffBits + bmp->dib.dsBmih.biSizeImage;
	bmfh.bfReserved1 = bmfh.bfReserved2 = 0;

	FILE* fh;
	fopen_s(&fh, fileName, "wb");
	AssertMsg(fh, "Could not open file for writing");
	fwrite(&bmfh, sizeof(BITMAPFILEHEADER), 1, fh);
	bmp->dib.dsBmih.biHeight = -bmp->dib.dsBmih.biHeight; // Standard for on-disk BMP is to have bottom-first row order
	fwrite(&bmp->dib.dsBmih, sizeof(BITMAPINFOHEADER), 1, fh);
	bmp->dib.dsBmih.biHeight = -bmp->dib.dsBmih.biHeight;
	if (bmp->dib.dsBmih.biBitCount == 8) {
		RGBQUAD palette[256];
		for (int i = 0; i < 256; ++i) { // grayscale palette
			palette[i].rgbBlue = palette[i].rgbGreen = palette[i].rgbRed = (unsigned char)i;
			palette[i].rgbReserved = 255;
		}
		fwrite(palette, 256*sizeof(RGBQUAD), 1, fh);
	}
	fwrite(bmp->dib.dsBm.bmBits, bmp->dib.dsBmih.biSizeImage, 1, fh);
	fclose(fh);
}

void DeleteImage(int handle)
{
	AssertMsg(sBitmapRegistry().find(handle) != sBitmapRegistry().end(),
			"You called DeleteImage with an invalid image handle.");
	BitmapInfo& info = sBitmapRegistry()[handle];
	BOOL deleted = DeleteObject(info.osHandle);
	AssertMsg(deleted, "Failed to delete image.");
	sBitmapRegistry().erase(handle); // redundant searching but who cares
}

// These "Ctrl" variables provide a reasonable default position for 
// controls in the order that they were created. The basic idea is to 
// place the controls in left-right order on a row, then wrap to the next 
// row once a control no longer fits in the *current* main window size.
// 
// Not intended to be robust. Makes typical apps work without any
// SetControlPosition nonsense.
//
static const int sCtrlSpacingX = 5, sCtrlSpacingY = 5;
static int sCreateCtrlX = sCtrlSpacingX, sCreateCtrlY = sCtrlSpacingY;
static int sDefaultRowCtrlSize = 23, sMaxRowCtrlSizeY = 23;
static int sPrevCreateCtrlX = sCreateCtrlX, sPrevCreateCtrlY = sCreateCtrlY;
static bool sAutoPlotPosition = true;

static void sSetDefaultCtrlPos(Window* ctrl, bool paddingX=true)
{
	sPrevCreateCtrlX = sCreateCtrlX;
	sPrevCreateCtrlY = sCreateCtrlY;
	int dx = ctrl->windowSizeX();
	int dy = ctrl->windowSizeY();
	if (dy > sMaxRowCtrlSizeY)
		sMaxRowCtrlSizeY = dy;
	if (sCreateCtrlX + dx >= theMainWindow()->clientSizeX()) {
		// No more space on current row. Wrap new control to next row.
		sCreateCtrlX = sCtrlSpacingX;
		sCreateCtrlY += sMaxRowCtrlSizeY + sCtrlSpacingY;
		sMaxRowCtrlSizeY = sDefaultRowCtrlSize;
	}
	ctrl->setPosition(sCreateCtrlX, sCreateCtrlY);
	sCreateCtrlX += dx;
	if (paddingX)
		sCreateCtrlX += sCtrlSpacingX;
}


int CreateTextLabel(const char* text)
{
	int handle = gNextCtrlID++;
	char name[64];
	sprintf_s(name, 64, "TextLabel%d", handle);
	StaticCtl* s = new StaticCtl(name);
	s->setParent(theMainWindow());
	s->setText(char2wstr(text).c_str());
	s->fitSizeToText();
	sSetDefaultCtrlPos(s, false);
	sTextLabelRegistry()[handle] = s;
	return handle;
}

void SetTextLabelString(int handle, const char* text)
{
	AssertMsg(sTextLabelRegistry().find(handle) != sTextLabelRegistry().end(),
			"You called SetTextLabelString with an invalid handle.");
	StaticCtl* s = sTextLabelRegistry()[handle];
	s->setText(char2wstr(text).c_str());
}

static void sButtonCallback(void* userData, Window* /*sender*/)
{
	if (userData)
		((functionptr)userData)();
}

int CreateButton(const char* text, functionptr onClick)
{
	int handle = gNextCtrlID++;
	char name[64];
	sprintf_s(name, 64, "Button%d", handle);
	ButtonCtl* b = new ButtonCtl(name);
	b->setParent(theMainWindow());
	b->setText(char2wstr(text).c_str());
	b->fitSizeToText();
	sSetDefaultCtrlPos(b);
	b->setOnClick(sButtonCallback, onClick);
	sButtonRegistry()[handle] = b;
	return handle;
}

static void sCheckBoxCallback(void* userData, Window* sender)
{
	bool state = static_cast<CheckBoxCtl*>(sender)->getState();
	if (userData)
		((functionptr_bool)userData)(state);
}

int CreateCheckBox(const char* text, bool checked, functionptr_bool onStateChanged)
{
	int handle = gNextCtrlID++;
	char name[64];
	sprintf_s(name, 64, "CheckBox%d", handle);
	CheckBoxCtl* cb = new CheckBoxCtl(name);
	cb->setParent(theMainWindow());
	cb->setText(char2wstr(text).c_str());
	cb->fitSizeToText();
	sSetDefaultCtrlPos(cb);
	cb->setState(checked);
	cb->setOnStateChanged(sCheckBoxCallback, onStateChanged);
	sCheckBoxRegistry()[handle] = cb;
	return handle;
}

bool GetCheckBoxState(int handle)
{
	AssertMsg(sCheckBoxRegistry().find(handle) != sCheckBoxRegistry().end(),
			"You called GetCheckBoxState with an invalid handle.");
	CheckBoxCtl* cb = sCheckBoxRegistry()[handle];
	return cb->getState();
}

void SetCheckBoxState(int handle, bool checked)
{
	AssertMsg(sCheckBoxRegistry().find(handle) != sCheckBoxRegistry().end(),
			"You called SetCheckBoxState with an invalid handle.");
	CheckBoxCtl* cb = sCheckBoxRegistry()[handle];
	cb->setState(checked);
}


static void sTextBoxCallback(void* userData, Window* sender)
{
	const char* text = static_cast<EditCtl*>(sender)->getString();
	if (userData)
		((functionptr_string)userData)(text);
}

int CreateTextBox(const char* text, functionptr_string onStringChanged)
{
	const int cDefaultSizeX = 75, cDefaultSizeY = sDefaultRowCtrlSize;
	int handle = gNextCtrlID++;
	char name[64];
	sprintf_s(name, 64, "TextBox%d", handle);
	EditCtl* tb = new EditCtl(name);
	tb->setParent(theMainWindow());
	tb->setString(text);
	tb->setSize(cDefaultSizeX, cDefaultSizeY);
	sSetDefaultCtrlPos(tb);
	tb->setOnStringChanged(sTextBoxCallback, onStringChanged);
	sTextBoxRegistry()[handle] = tb;
	return handle;
}

const char* GetTextBoxString(int handle)
{
	AssertMsg(sTextBoxRegistry().find(handle) != sTextBoxRegistry().end(),
			"You called GetTextBoxState with an invalid handle.");
	EditCtl* tb = sTextBoxRegistry()[handle];
	return tb->getString();
}

void SetTextBoxString(int handle, const char* text)
{
	AssertMsg(sTextBoxRegistry().find(handle) != sTextBoxRegistry().end(),
			"You called SetTextBoxState with an invalid handle.");
	EditCtl* tb = sTextBoxRegistry()[handle];
	tb->setString(text);
}


static void sDropListCallback(void* userData, Window* sender)
{
	int index = static_cast<ComboBoxCtl*>(sender)->getSelectedItem();
	if (userData)
		((functionptr_int)userData)(index);
}

int CreateDropList(int numItems, const char* const items[], int selected, functionptr_int onSelectionChanged)
{
	int handle = gNextCtrlID++;
	char name[64];
	sprintf_s(name, 64, "DropList%d", handle);
	ComboBoxCtl* cb = new ComboBoxCtl(name);
	cb->setParent(theMainWindow());
	for (int i = 0; i < numItems; ++i)
		cb->insertItem(i, items[i]);
	cb->setSelectedItem(selected);
	cb->setOnSelectionChanged(sDropListCallback, onSelectionChanged);
	cb->fitSizeToItems();
	sSetDefaultCtrlPos(cb);
	sDropListRegistry()[handle] = cb;
	return handle;
}

int GetDropListSelection(int handle)
{
	AssertMsg(sDropListRegistry().find(handle) != sDropListRegistry().end(),
			"You called GetDropListSelection with an invalid handle.");
	ComboBoxCtl* cb = sDropListRegistry()[handle];
	return cb->getSelectedItem();
}

void SetDropListSelection(int handle, int selected)
{
	AssertMsg(sDropListRegistry().find(handle) != sDropListRegistry().end(),
			"You called SetDropListSelection with an invalid handle.");
	ComboBoxCtl* cb = sDropListRegistry()[handle];
	AssertMsg(selected >= 0 && selected < cb->itemCount(),
			"You called SetDropListSelection with an invalid index.");
	cb->setSelectedItem(selected);
}


static void sSliderCallback(void* userData, Window* sender)
{
	float value = static_cast<TrackbarCtl*>(sender)->getValue();
	if (userData)
		((functionptr_float)userData)(value);
}

int CreateSlider(float value, functionptr_float onValueChanged, bool isHorizontal)
{
	const int cDefaultSizeX = 100, cDefaultSizeY = sDefaultRowCtrlSize;
	int handle = gNextCtrlID++;
	char name[64];
	sprintf_s(name, 64, "Slider%d", handle);
	TrackbarCtl* tb = new TrackbarCtl(name, isHorizontal);
	tb->setParent(theMainWindow());
	if (isHorizontal)
		tb->setSize(cDefaultSizeX, cDefaultSizeY);
	else
		tb->setSize(cDefaultSizeY, cDefaultSizeX);
	tb->setValue(value);
	tb->setOnValueChanged(sSliderCallback, onValueChanged);
	sSetDefaultCtrlPos(tb);
	sSliderRegistry()[handle] = tb;
	return handle;
}

float GetSliderValue(int handle)
{
	AssertMsg(sSliderRegistry().find(handle) != sSliderRegistry().end(),
	      "You called GetSliderValue with an invalid handle.");
	TrackbarCtl* tb = sSliderRegistry()[handle];
	return tb->getValue();
}

void SetSliderValue(int handle, float value)
{
	AssertMsg(sSliderRegistry().find(handle) != sSliderRegistry().end(),
	      "You called SetSliderValue with an invalid handle.");
	TrackbarCtl* tb = sSliderRegistry()[handle];
	tb->setValue(value);
}

void SetControlPosition(int handle, int x, int y)
{
	if (sPrevCreateCtrlX != sCreateCtrlX || sPrevCreateCtrlY != sCreateCtrlY) {
		sMaxRowCtrlSizeY = sDefaultRowCtrlSize;
		sCreateCtrlX = sPrevCreateCtrlX; // HEURISTIC: if the student created a control, then immediately moved it,
		sCreateCtrlY = sPrevCreateCtrlY; // assume that the next control can go in the blank spot left for this one.
	}
	sGetControl(handle,"SetControlPosition")->setPosition(x, y);
	if (sPlotRegistry().find(handle) != sPlotRegistry().end())
		sAutoPlotPosition = false;
}

void SetControlSize(int handle, int sizeX, int sizeY)
{
	Window* ctrl = sGetControl(handle,"SetControlSize");
	if (sPrevCreateCtrlX != sCreateCtrlX || sPrevCreateCtrlY != sCreateCtrlY) {
		sCreateCtrlX += sizeX - ctrl->windowSizeX(); // HEURISTIC: if the student creates a control, then immediately resizes it,
		sCreateCtrlY += sizeY - ctrl->windowSizeY(); // then make sure the next control is pushed over when it gets created.
		if (sizeY > sMaxRowCtrlSizeY)
			sMaxRowCtrlSizeY = sizeY;
	}
	ctrl->setSize(sizeX, sizeY);
}

void GetControlPosition(int handle, int* x, int* y)
{
	Window* ctrl = sGetControl(handle,"GetControlPosition");
	*x = ctrl->relativeX();
	*y = ctrl->relativeY();
}

void GetControlSize(int handle, int* sizeX, int* sizeY)
{
	Window* ctrl = sGetControl(handle,"GetControlSize");
	*sizeX = ctrl->windowSizeX();
	*sizeY = ctrl->windowSizeY();
}

void DeleteControl(int handle)
{
	if (sTextLabelRegistry().find(handle) != sTextLabelRegistry().end()) {
		sTextLabelRegistry()[handle]->destroy();
		sTextLabelRegistry().erase(handle);
	} else if (sButtonRegistry().find(handle) != sButtonRegistry().end()) {
		sButtonRegistry()[handle]->destroy();
		sButtonRegistry().erase(handle);
	} else if (sDropListRegistry().find(handle) != sDropListRegistry().end()) {
		sDropListRegistry()[handle]->destroy();
		sDropListRegistry().erase(handle);
	} else if (sCheckBoxRegistry().find(handle) != sCheckBoxRegistry().end()) {
		sCheckBoxRegistry()[handle]->destroy();
		sCheckBoxRegistry().erase(handle);
	} else if (sTextBoxRegistry().find(handle) != sTextBoxRegistry().end()) {
		sTextBoxRegistry()[handle]->destroy();
		sTextBoxRegistry().erase(handle);
	} else if (sSliderRegistry().find(handle) != sSliderRegistry().end()) {
		sSliderRegistry()[handle]->destroy();
		sSliderRegistry().erase(handle);
	} else if (sPlotRegistry().find(handle) != sPlotRegistry().end()) {
		sPlotRegistry()[handle]->destroy();
		sPlotRegistry().erase(handle);
	} else {
		AssertUnreachable("You called DeleteControl with an invalid handle. (control was already deleted?)");
	}
}

static void sWindowResizeCallback(void* userData, Window* sender, int, int)
{
	((functionptr_intint)userData)(sender->clientSizeX(), sender->clientSizeY());
}

void SetWindowResizeCallback(functionptr_intint onWindowResize)
{
	theMainWindow()->setOnSize(sWindowResizeCallback, onWindowResize);
}

struct QPCInitializer {
	QPCInitializer() 
	{
		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&ticks0);
	}
	LARGE_INTEGER ticks0;
	LARGE_INTEGER frequency;
};

static QPCInitializer sQPC; // constructor is called before main(), to record the tick counter at program startup.

double GetMilliseconds()
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	double microseconds = (double)(10000000i64*(now.QuadPart-sQPC.ticks0.QuadPart) / sQPC.frequency.QuadPart)/10.0;
	return microseconds / 1000;
}

static bool sRandInitialized = false;

int GetRandomInt()
{
	if (!sRandInitialized) {
		srand((unsigned)time(0));
		sRandInitialized = true;
	}
	// randomize all 31 bits to make an unsigned random integer
	return ((rand() & 0x7fff) | (rand() << 16)) & ~(1 << 31);
}

double GetRandomReal()
{
	return (double)GetRandomInt() / ((double)~(1 << 31)+1);
}

////////////////////////////////////////////////////

static void sThreadStart(void* arg)
{
	((functionptr)arg)();
}

void StartParallelThread(functionptr func)
{
	_beginthread(sThreadStart, 0, func);
}

void StartParallelThreadWithArg(functionptr_voidptr func, void* arg)
{
	_beginthread(func, 0, arg);
}

static const int cMaxCriticalSections = 100;
static CRITICAL_SECTION sCriticalSections[cMaxCriticalSections];

// Initialize critical sections before main() gets a chance to start
struct InitCriticalSections {
	InitCriticalSections() {
		for (int i = 0; i < cMaxCriticalSections; ++i)
			InitializeCriticalSectionAndSpinCount(&sCriticalSections[i], 50);
		gMainThreadID = _threadid;
	}
};
InitCriticalSections gInitCriticalSections;

void BeginCriticalSection(int id) {
	AssertMsg(id >= 0 && id < cMaxCriticalSections, "Invalid critical section id; valid ids are 0..99");
	EnterCriticalSection(&sCriticalSections[id]);
}

void EndCriticalSection(int id) {
	AssertMsg(id >= 0 && id < cMaxCriticalSections, "Invalid critical section id; valid ids are 0..99");
	AssertMsg(sCriticalSections[id].LockCount >= 0, "EndCriticalSection was called outside of a critical section");
	LeaveCriticalSection(&sCriticalSections[id]);
}


////////////////////////////////////////////////////


static PlotCtl* sActivePlot = 0;   // These variables introduce the convenient notion of a "default," or "active" plot
static int sActivePlotID = 0;      // much like how Matlab has a current figure/axes to which commands apply by default.
static int sActivePlotSeries = 0;

int CreatePlot(const char* title, const char* axisLabelX, const char* axisLabelY)
{
	int handle = gNextCtrlID++;
	char name[64];
	sprintf_s(name, 64, "Plot%d", handle);
	PlotCtl* p = new PlotCtl(name);
	p->setParent(theMainWindow());
	if (sAutoPlotPosition && !sPlotRegistry().empty()) {
		int windowSizeY = theMainWindow()->clientSizeY();
		int plotSizeY = windowSizeY / sPlotRegistry().size();
		theMainWindow()->setSize(theMainWindow()->clientSizeX(), windowSizeY*(sPlotRegistry().size()+1));
		p->setPositionAndSize(0, windowSizeY, theMainWindow()->clientSizeX(), plotSizeY);
	} else {
		p->setPositionAndSize(0, 0, 
							  theMainWindow()->clientSizeX(), 
							  theMainWindow()->clientSizeY());
	}
	if (title)
		p->setTitle(title);
	if (axisLabelX || axisLabelY)
		p->setAxisLabels(axisLabelX, axisLabelY);
	sPlotRegistry()[handle] = p;
	sActivePlot = p;
	sActivePlotID = handle;
	return handle;
}

int CreatePlotSeries(const char* title, const char* style, int plot)
{
	PlotCtl* p = 0;
	if (!plot) {
		if (!sActivePlot)
			CreatePlot(0,0,0); // create a default plot if no active plot yet
		p = sActivePlot;
	} else {
		AssertMsg(sPlotRegistry().find(plot) != sPlotRegistry().end(),
			"You called CreatePlotSeries with an invalid plot handle.");
		p = sPlotRegistry()[plot];
	}
	int series = p->addSeries(0, 0, 0, title, style);
	sPlotSeriesRegistry()[series] = p; // remember which plot owns 'series'
	if (p == sActivePlot)
		sActivePlotSeries = series; // make the new series the 'default' series
	return series;
}

void AddSeriesPoint(double x, double y, int series)
{
	PlotCtl* p = 0;
	if (!series) {
		if (!sActivePlotSeries)
			CreatePlotSeries(0, 0, sActivePlotID); // create default series if no active series yet
		p = sActivePlot;
		series = sActivePlotSeries;
	} else {
		AssertMsg(sPlotSeriesRegistry().find(series) != sPlotSeriesRegistry().end(),
			"You called AddSeriesPoint with an invalid series handle.");
		p = sPlotSeriesRegistry()[series];
	}
	p->addSeriesPoint(series, x, y);
}

















































///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
//
//  The code below was taken directly from the lpng library by Alex Pankratov
//  and the puff library by Mark Adler. 
//
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////


/* puff.h
  Copyright (C) 2002, 2003 Mark Adler, all rights reserved
  version 1.7, 3 Mar 2002

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Mark Adler    madler@alumni.caltech.edu
 */

/*
 * puff.c
 * Copyright (C) 2002-2004 Mark Adler
 * For conditions of distribution and use, see copyright notice in puff.h
 * version 1.8, 9 Jan 2004
 *
 * puff.c is a simple inflate written to be an unambiguous way to specify the
 * deflate format.  It is not written for speed but rather simplicity.  As a
 * side benefit, this code might actually be useful when small code is more
 * important than speed, such as bootstrap applications.  For typical deflate
 * data, zlib's inflate() is about four times as fast as puff().  zlib's
 * inflate compiles to around 20K on my machine, whereas puff.c compiles to
 * around 4K on my machine (a PowerPC using GNU cc).  If the faster decode()
 * function here is used, then puff() is only twice as slow as zlib's
 * inflate().
 *
 * All dynamically allocated memory comes from the stack.  The stack required
 * is less than 2K bytes.  This code is compatible with 16-bit int's and
 * assumes that long's are at least 32 bits.  puff.c uses the short data type,
 * assumed to be 16 bits, for arrays in order to to conserve memory.  The code
 * works whether integers are stored big endian or little endian.
 *
 * In the comments below are "Format notes" that describe the inflate process
 * and document some of the less obvious aspects of the format.  This source
 * code is meant to supplement RFC 1951, which formally describes the deflate
 * format:
 *
 *    http://www.zlib.org/rfc-deflate.html
 */

/*
 * Change history:
 *
 * 1.0  10 Feb 2002     - First version
 * 1.1  17 Feb 2002     - Clarifications of some comments and notes
 *                      - Update puff() dest and source pointers on negative
 *                        errors to facilitate debugging deflators
 *                      - Remove longest from struct huffman -- not needed
 *                      - Simplify offs[] index in construct()
 *                      - Add input size and checking, using longjmp() to
 *                        maintain easy readability
 *                      - Use short data type for large arrays
 *                      - Use pointers instead of long to specify source and
 *                        destination sizes to avoid arbitrary 4 GB limits
 * 1.2  17 Mar 2002     - Add faster version of decode(), doubles speed (!),
 *                        but leave simple version for readabilty
 *                      - Make sure invalid distances detected if pointers
 *                        are 16 bits
 *                      - Fix fixed codes table error
 *                      - Provide a scanning mode for determining size of
 *                        uncompressed data
 * 1.3  20 Mar 2002     - Go back to lengths for puff() parameters [Jean-loup]
 *                      - Add a puff.h file for the interface
 *                      - Add braces in puff() for else do [Jean-loup]
 *                      - Use indexes instead of pointers for readability
 * 1.4  31 Mar 2002     - Simplify construct() code set check
 *                      - Fix some comments
 *                      - Add FIXLCODES #define
 * 1.5   6 Apr 2002     - Minor comment fixes
 * 1.6   7 Aug 2002     - Minor format changes
 * 1.7   3 Mar 2003     - Added test code for distribution
 *                      - Added zlib-like license
 * 1.8   9 Jan 2004     - Added some comments on no distance codes case
 */

#include <setjmp.h>             /* for setjmp(), longjmp(), and jmp_buf */

#define local static            /* for local function definitions */
#define NIL ((unsigned char *)0)        /* for no output option */

/*
 * Maximums for allocations and loops.  It is not useful to change these --
 * they are fixed by the deflate format.
 */
#define MAXBITS 15              /* maximum bits in a code */
#define MAXLCODES 286           /* maximum number of literal/length codes */
#define MAXDCODES 30            /* maximum number of distance codes */
#define MAXCODES (MAXLCODES+MAXDCODES)  /* maximum codes lengths to read */
#define FIXLCODES 288           /* number of fixed literal/length codes */

/* input and output state */
struct state {
    /* output state */
    unsigned char *out;         /* output buffer */
    unsigned long outlen;       /* available space at out */
    unsigned long outcnt;       /* bytes written to out so far */

    /* input state */
    unsigned char *in;          /* input buffer */
    unsigned long inlen;        /* available input at in */
    unsigned long incnt;        /* bytes read so far */
    int bitbuf;                 /* bit buffer */
    int bitcnt;                 /* number of bits in bit buffer */

    /* input limit error return state for bits() and decode() */
    jmp_buf env;
};

/*
 * Return need bits from the input stream.  This always leaves less than
 * eight bits in the buffer.  bits() works properly for need == 0.
 *
 * Format notes:
 *
 * - Bits are stored in bytes from the least significant bit to the most
 *   significant bit.  Therefore bits are dropped from the bottom of the bit
 *   buffer, using shift right, and new bytes are appended to the top of the
 *   bit buffer, using shift left.
 */
local int bits(struct state *s, int need)
{
    long val;           /* bit accumulator (can use up to 20 bits) */

    /* load at least need bits into val */
    val = s->bitbuf;
    while (s->bitcnt < need) {
        if (s->incnt == s->inlen) longjmp(s->env, 1);   /* out of input */
        val |= (long)(s->in[s->incnt++]) << s->bitcnt;  /* load eight bits */
        s->bitcnt += 8;
    }

    /* drop need bits and update buffer, always zero to seven bits left */
    s->bitbuf = (int)(val >> need);
    s->bitcnt -= need;

    /* return need bits, zeroing the bits above that */
    return (int)(val & ((1L << need) - 1));
}

/*
 * Process a stored block.
 *
 * Format notes:
 *
 * - After the two-bit stored block type (00), the stored block length and
 *   stored bytes are byte-aligned for fast copying.  Therefore any leftover
 *   bits in the byte that has the last bit of the type, as many as seven, are
 *   discarded.  The value of the discarded bits are not defined and should not
 *   be checked against any expectation.
 *
 * - The second inverted copy of the stored block length does not have to be
 *   checked, but it's probably a good idea to do so anyway.
 *
 * - A stored block can have zero length.  This is sometimes used to byte-align
 *   subsets of the compressed data for random access or partial recovery.
 */
local int stored(struct state *s)
{
    unsigned len;       /* length of stored block */

    /* discard leftover bits from current byte (assumes s->bitcnt < 8) */
    s->bitbuf = 0;
    s->bitcnt = 0;

    /* get length and check against its one's complement */
    if (s->incnt + 4 > s->inlen) return 2;      /* not enough input */
    len = s->in[s->incnt++];
    len |= s->in[s->incnt++] << 8;
    if (s->in[s->incnt++] != (~len & 0xff) ||
        s->in[s->incnt++] != ((~len >> 8) & 0xff))
        return -2;                              /* didn't match complement! */

    /* copy len bytes from in to out */
    if (s->incnt + len > s->inlen) return 2;    /* not enough input */
    if (s->out != NIL) {
        if (s->outcnt + len > s->outlen)
            return 1;                           /* not enough output space */
        while (len--)
            s->out[s->outcnt++] = s->in[s->incnt++];
    }
    else {                                      /* just scanning */
        s->outcnt += len;
        s->incnt += len;
    }

    /* done with a valid stored block */
    return 0;
}

/*
 * Huffman code decoding tables.  count[1..MAXBITS] is the number of symbols of
 * each length, which for a canonical code are stepped through in order.
 * symbol[] are the symbol values in canonical order, where the number of
 * entries is the sum of the counts in count[].  The decoding process can be
 * seen in the function decode() below.
 */
struct huffman {
    short *count;       /* number of symbols of each length */
    short *symbol;      /* canonically ordered symbols */
};

/*
 * Decode a code from the stream s using huffman table h.  Return the symbol or
 * a negative value if there is an error.  If all of the lengths are zero, i.e.
 * an empty code, or if the code is incomplete and an invalid code is received,
 * then -9 is returned after reading MAXBITS bits.
 *
 * Format notes:
 *
 * - The codes as stored in the compressed data are bit-reversed relative to
 *   a simple integer ordering of codes of the same lengths.  Hence below the
 *   bits are pulled from the compressed data one at a time and used to
 *   build the code value reversed from what is in the stream in order to
 *   permit simple integer comparisons for decoding.  A table-based decoding
 *   scheme (as used in zlib) does not need to do this reversal.
 *
 * - The first code for the shortest length is all zeros.  Subsequent codes of
 *   the same length are simply integer increments of the previous code.  When
 *   moving up a length, a zero bit is appended to the code.  For a complete
 *   code, the last code of the longest length will be all ones.
 *
 * - Incomplete codes are handled by this decoder, since they are permitted
 *   in the deflate format.  See the format notes for fixed() and dynamic().
 */
#ifdef SLOW
local int decode(struct state *s, struct huffman *h)
{
    int len;            /* current number of bits in code */
    int code;           /* len bits being decoded */
    int first;          /* first code of length len */
    int count;          /* number of codes of length len */
    int index;          /* index of first code of length len in symbol table */

    code = first = index = 0;
    for (len = 1; len <= MAXBITS; len++) {
        code |= bits(s, 1);             /* get next bit */
        count = h->count[len];
        if (code < first + count)       /* if length len, return symbol */
            return h->symbol[index + (code - first)];
        index += count;                 /* else update for next length */
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -9;                          /* ran out of codes */
}

/*
 * A faster version of decode() for real applications of this code.   It's not
 * as readable, but it makes puff() twice as fast.  And it only makes the code
 * a few percent larger.
 */
#else /* !SLOW */
local int decode(struct state *s, struct huffman *h)
{
    int len;            /* current number of bits in code */
    int code;           /* len bits being decoded */
    int first;          /* first code of length len */
    int count;          /* number of codes of length len */
    int index;          /* index of first code of length len in symbol table */
    int bitbuf;         /* bits from stream */
    int left;           /* bits left in next or left to process */
    short *next;        /* next number of codes */

    bitbuf = s->bitbuf;
    left = s->bitcnt;
    code = first = index = 0;
    len = 1;
    next = h->count + 1;
    while (1) {
        while (left--) {
            code |= bitbuf & 1;
            bitbuf >>= 1;
            count = *next++;
            if (code < first + count) { /* if length len, return symbol */
                s->bitbuf = bitbuf;
                s->bitcnt = (s->bitcnt - len) & 7;
                return h->symbol[index + (code - first)];
            }
            index += count;             /* else update for next length */
            first += count;
            first <<= 1;
            code <<= 1;
            len++;
        }
        left = (MAXBITS+1) - len;
        if (left == 0) break;
        if (s->incnt == s->inlen) longjmp(s->env, 1);   /* out of input */
        bitbuf = s->in[s->incnt++];
        if (left > 8) left = 8;
    }
    return -9;                          /* ran out of codes */
}
#endif /* SLOW */

/*
 * Given the list of code lengths length[0..n-1] representing a canonical
 * Huffman code for n symbols, construct the tables required to decode those
 * codes.  Those tables are the number of codes of each length, and the symbols
 * sorted by length, retaining their original order within each length.  The
 * return value is zero for a complete code set, negative for an over-
 * subscribed code set, and positive for an incomplete code set.  The tables
 * can be used if the return value is zero or positive, but they cannot be used
 * if the return value is negative.  If the return value is zero, it is not
 * possible for decode() using that table to return an error--any stream of
 * enough bits will resolve to a symbol.  If the return value is positive, then
 * it is possible for decode() using that table to return an error for received
 * codes past the end of the incomplete lengths.
 *
 * Not used by decode(), but used for error checking, h->count[0] is the number
 * of the n symbols not in the code.  So n - h->count[0] is the number of
 * codes.  This is useful for checking for incomplete codes that have more than
 * one symbol, which is an error in a dynamic block.
 *
 * Assumption: for all i in 0..n-1, 0 <= length[i] <= MAXBITS
 * This is assured by the construction of the length arrays in dynamic() and
 * fixed() and is not verified by construct().
 *
 * Format notes:
 *
 * - Permitted and expected examples of incomplete codes are one of the fixed
 *   codes and any code with a single symbol which in deflate is coded as one
 *   bit instead of zero bits.  See the format notes for fixed() and dynamic().
 *
 * - Within a given code length, the symbols are kept in ascending order for
 *   the code bits definition.
 */
local int construct(struct huffman *h, short *length, int n)
{
    int symbol;         /* current symbol when stepping through length[] */
    int len;            /* current length when stepping through h->count[] */
    int left;           /* number of possible codes left of current length */
    short offs[MAXBITS+1];      /* offsets in symbol table for each length */

    /* count number of codes of each length */
    for (len = 0; len <= MAXBITS; len++)
        h->count[len] = 0;
    for (symbol = 0; symbol < n; symbol++)
        (h->count[length[symbol]])++;   /* assumes lengths are within bounds */
    if (h->count[0] == n)               /* no codes! */
        return 0;                       /* complete, but decode() will fail */

    /* check for an over-subscribed or incomplete set of lengths */
    left = 1;                           /* one possible code of zero length */
    for (len = 1; len <= MAXBITS; len++) {
        left <<= 1;                     /* one more bit, double codes left */
        left -= h->count[len];          /* deduct count from possible codes */
        if (left < 0) return left;      /* over-subscribed--return negative */
    }                                   /* left > 0 means incomplete */

    /* generate offsets into symbol table for each length for sorting */
    offs[1] = 0;
    for (len = 1; len < MAXBITS; len++)
        offs[len + 1] = offs[len] + h->count[len];

    /*
     * put symbols in table sorted by length, by symbol order within each
     * length
     */
    for (symbol = 0; symbol < n; symbol++)
        if (length[symbol] != 0)
            h->symbol[offs[length[symbol]]++] = symbol;

    /* return zero for complete set, positive for incomplete set */
    return left;
}

/*
 * Decode literal/length and distance codes until an end-of-block code.
 *
 * Format notes:
 *
 * - Compressed data that is after the block type if fixed or after the code
 *   description if dynamic is a combination of literals and length/distance
 *   pairs terminated by and end-of-block code.  Literals are simply Huffman
 *   coded bytes.  A length/distance pair is a coded length followed by a
 *   coded distance to represent a string that occurs earlier in the
 *   uncompressed data that occurs again at the current location.
 *
 * - Literals, lengths, and the end-of-block code are combined into a single
 *   code of up to 286 symbols.  They are 256 literals (0..255), 29 length
 *   symbols (257..285), and the end-of-block symbol (256).
 *
 * - There are 256 possible lengths (3..258), and so 29 symbols are not enough
 *   to represent all of those.  Lengths 3..10 and 258 are in fact represented
 *   by just a length symbol.  Lengths 11..257 are represented as a symbol and
 *   some number of extra bits that are added as an integer to the base length
 *   of the length symbol.  The number of extra bits is determined by the base
 *   length symbol.  These are in the static arrays below, lens[] for the base
 *   lengths and lext[] for the corresponding number of extra bits.
 *
 * - The reason that 258 gets its own symbol is that the longest length is used
 *   often in highly redundant files.  Note that 258 can also be coded as the
 *   base value 227 plus the maximum extra value of 31.  While a good deflate
 *   should never do this, it is not an error, and should be decoded properly.
 *
 * - If a length is decoded, including its extra bits if any, then it is
 *   followed a distance code.  There are up to 30 distance symbols.  Again
 *   there are many more possible distances (1..32768), so extra bits are added
 *   to a base value represented by the symbol.  The distances 1..4 get their
 *   own symbol, but the rest require extra bits.  The base distances and
 *   corresponding number of extra bits are below in the static arrays dist[]
 *   and dext[].
 *
 * - Literal bytes are simply written to the output.  A length/distance pair is
 *   an instruction to copy previously uncompressed bytes to the output.  The
 *   copy is from distance bytes back in the output stream, copying for length
 *   bytes.
 *
 * - Distances pointing before the beginning of the output data are not
 *   permitted.
 *
 * - Overlapped copies, where the length is greater than the distance, are
 *   allowed and common.  For example, a distance of one and a length of 258
 *   simply copies the last byte 258 times.  A distance of four and a length of
 *   twelve copies the last four bytes three times.  A simple forward copy
 *   ignoring whether the length is greater than the distance or not implements
 *   this correctly.  You should not use memcpy() since its behavior is not
 *   defined for overlapped arrays.  You should not use memmove() or bcopy()
 *   since though their behavior -is- defined for overlapping arrays, it is
 *   defined to do the wrong thing in this case.
 */
local int codes(struct state *s,
                struct huffman *lencode,
                struct huffman *distcode)
{
    int symbol;         /* decoded symbol */
    int len;            /* length for copy */
    unsigned dist;      /* distance for copy */
    static const short lens[29] = { /* Size base for length codes 257..285 */
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const short lext[29] = { /* Extra bits for length codes 257..285 */
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const short dists[30] = { /* Offset base for distance codes 0..29 */
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
        8193, 12289, 16385, 24577};
    static const short dext[30] = { /* Extra bits for distance codes 0..29 */
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
        12, 12, 13, 13};

    /* decode literals and length/distance pairs */
    do {
        symbol = decode(s, lencode);
        if (symbol < 0) return symbol;  /* invalid symbol */
        if (symbol < 256) {             /* literal: symbol is the byte */
            /* write out the literal */
            if (s->out != NIL) {
                if (s->outcnt == s->outlen) return 1;
                s->out[s->outcnt] = symbol;
            }
            s->outcnt++;
        }
        else if (symbol > 256) {        /* length */
            /* get and compute length */
            symbol -= 257;
            if (symbol >= 29) return -9;        /* invalid fixed code */
            len = lens[symbol] + bits(s, lext[symbol]);

            /* get and check distance */
            symbol = decode(s, distcode);
            if (symbol < 0) return symbol;      /* invalid symbol */
            dist = dists[symbol] + bits(s, dext[symbol]);
            if (dist > s->outcnt)
                return -10;     /* distance too far back */

            /* copy length bytes from distance bytes back */
            if (s->out != NIL) {
                if (s->outcnt + len > s->outlen) return 1;
                while (len--) {
                    s->out[s->outcnt] = s->out[s->outcnt - dist];
                    s->outcnt++;
                }
            }
            else
                s->outcnt += len;
        }
    } while (symbol != 256);            /* end of block symbol */

    /* done with a valid fixed or dynamic block */
    return 0;
}

/*
 * Process a fixed codes block.
 *
 * Format notes:
 *
 * - This block type can be useful for compressing small amounts of data for
 *   which the size of the code descriptions in a dynamic block exceeds the
 *   benefit of custom codes for that block.  For fixed codes, no bits are
 *   spent on code descriptions.  Instead the code lengths for literal/length
 *   codes and distance codes are fixed.  The specific lengths for each symbol
 *   can be seen in the "for" loops below.
 *
 * - The literal/length code is complete, but has two symbols that are invalid
 *   and should result in an error if received.  This cannot be implemented
 *   simply as an incomplete code since those two symbols are in the "middle"
 *   of the code.  They are eight bits long and the longest literal/length\
 *   code is nine bits.  Therefore the code must be constructed with those
 *   symbols, and the invalid symbols must be detected after decoding.
 *
 * - The fixed distance codes also have two invalid symbols that should result
 *   in an error if received.  Since all of the distance codes are the same
 *   length, this can be implemented as an incomplete code.  Then the invalid
 *   codes are detected while decoding.
 */
local int fixed(struct state *s)
{
    static int virgin = 1;
    static short lencnt[MAXBITS+1], lensym[FIXLCODES];
    static short distcnt[MAXBITS+1], distsym[MAXDCODES];
    static struct huffman lencode = {lencnt, lensym};
    static struct huffman distcode = {distcnt, distsym};

    /* build fixed huffman tables if first call (may not be thread safe) */
    if (virgin) {
        int symbol;
        short lengths[FIXLCODES];

        /* literal/length table */
        for (symbol = 0; symbol < 144; symbol++)
            lengths[symbol] = 8;
        for (; symbol < 256; symbol++)
            lengths[symbol] = 9;
        for (; symbol < 280; symbol++)
            lengths[symbol] = 7;
        for (; symbol < FIXLCODES; symbol++)
            lengths[symbol] = 8;
        construct(&lencode, lengths, FIXLCODES);

        /* distance table */
        for (symbol = 0; symbol < MAXDCODES; symbol++)
            lengths[symbol] = 5;
        construct(&distcode, lengths, MAXDCODES);

        /* do this just once */
        virgin = 0;
    }

    /* decode data until end-of-block code */
    return codes(s, &lencode, &distcode);
}

/*
 * Process a dynamic codes block.
 *
 * Format notes:
 *
 * - A dynamic block starts with a description of the literal/length and
 *   distance codes for that block.  New dynamic blocks allow the compressor to
 *   rapidly adapt to changing data with new codes optimized for that data.
 *
 * - The codes used by the deflate format are "canonical", which means that
 *   the actual bits of the codes are generated in an unambiguous way simply
 *   from the number of bits in each code.  Therefore the code descriptions
 *   are simply a list of code lengths for each symbol.
 *
 * - The code lengths are stored in order for the symbols, so lengths are
 *   provided for each of the literal/length symbols, and for each of the
 *   distance symbols.
 *
 * - If a symbol is not used in the block, this is represented by a zero as
 *   as the code length.  This does not mean a zero-length code, but rather
 *   that no code should be created for this symbol.  There is no way in the
 *   deflate format to represent a zero-length code.
 *
 * - The maximum number of bits in a code is 15, so the possible lengths for
 *   any code are 1..15.
 *
 * - The fact that a length of zero is not permitted for a code has an
 *   interesting consequence.  Normally if only one symbol is used for a given
 *   code, then in fact that code could be represented with zero bits.  However
 *   in deflate, that code has to be at least one bit.  So for example, if
 *   only a single distance base symbol appears in a block, then it will be
 *   represented by a single code of length one, in particular one 0 bit.  This
 *   is an incomplete code, since if a 1 bit is received, it has no meaning,
 *   and should result in an error.  So incomplete distance codes of one symbol
 *   should be permitted, and the receipt of invalid codes should be handled.
 *
 * - It is also possible to have a single literal/length code, but that code
 *   must be the end-of-block code, since every dynamic block has one.  This
 *   is not the most efficient way to create an empty block (an empty fixed
 *   block is fewer bits), but it is allowed by the format.  So incomplete
 *   literal/length codes of one symbol should also be permitted.
 *
 * - If there are only literal codes and no lengths, then there are no distance
 *   codes.  This is represented by one distance code with zero bits.
 *
 * - The list of up to 286 length/literal lengths and up to 30 distance lengths
 *   are themselves compressed using Huffman codes and run-length encoding.  In
 *   the list of code lengths, a 0 symbol means no code, a 1..15 symbol means
 *   that length, and the symbols 16, 17, and 18 are run-length instructions.
 *   Each of 16, 17, and 18 are follwed by extra bits to define the length of
 *   the run.  16 copies the last length 3 to 6 times.  17 represents 3 to 10
 *   zero lengths, and 18 represents 11 to 138 zero lengths.  Unused symbols
 *   are common, hence the special coding for zero lengths.
 *
 * - The symbols for 0..18 are Huffman coded, and so that code must be
 *   described first.  This is simply a sequence of up to 19 three-bit values
 *   representing no code (0) or the code length for that symbol (1..7).
 *
 * - A dynamic block starts with three fixed-size counts from which is computed
 *   the number of literal/length code lengths, the number of distance code
 *   lengths, and the number of code length code lengths (ok, you come up with
 *   a better name!) in the code descriptions.  For the literal/length and
 *   distance codes, lengths after those provided are considered zero, i.e. no
 *   code.  The code length code lengths are received in a permuted order (see
 *   the order[] array below) to make a short code length code length list more
 *   likely.  As it turns out, very short and very long codes are less likely
 *   to be seen in a dynamic code description, hence what may appear initially
 *   to be a peculiar ordering.
 *
 * - Given the number of literal/length code lengths (nlen) and distance code
 *   lengths (ndist), then they are treated as one long list of nlen + ndist
 *   code lengths.  Therefore run-length coding can and often does cross the
 *   boundary between the two sets of lengths.
 *
 * - So to summarize, the code description at the start of a dynamic block is
 *   three counts for the number of code lengths for the literal/length codes,
 *   the distance codes, and the code length codes.  This is followed by the
 *   code length code lengths, three bits each.  This is used to construct the
 *   code length code which is used to read the remainder of the lengths.  Then
 *   the literal/length code lengths and distance lengths are read as a single
 *   set of lengths using the code length codes.  Codes are constructed from
 *   the resulting two sets of lengths, and then finally you can start
 *   decoding actual compressed data in the block.
 *
 * - For reference, a "typical" size for the code description in a dynamic
 *   block is around 80 bytes.
 */
local int dynamic(struct state *s)
{
    int nlen, ndist, ncode;             /* number of lengths in descriptor */
    int index;                          /* index of lengths[] */
    int err;                            /* construct() return value */
    short lengths[MAXCODES];            /* descriptor code lengths */
    short lencnt[MAXBITS+1], lensym[MAXLCODES];         /* lencode memory */
    short distcnt[MAXBITS+1], distsym[MAXDCODES];       /* distcode memory */
    struct huffman lencode = {lencnt, lensym};          /* length code */
    struct huffman distcode = {distcnt, distsym};       /* distance code */
    static const short order[19] =      /* permutation of code length codes */
        {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

    /* get number of lengths in each table, check lengths */
    nlen = bits(s, 5) + 257;
    ndist = bits(s, 5) + 1;
    ncode = bits(s, 4) + 4;
    if (nlen > MAXLCODES || ndist > MAXDCODES)
        return -3;                      /* bad counts */

    /* read code length code lengths (really), missing lengths are zero */
    for (index = 0; index < ncode; index++)
        lengths[order[index]] = bits(s, 3);
    for (; index < 19; index++)
        lengths[order[index]] = 0;

    /* build huffman table for code lengths codes (use lencode temporarily) */
    err = construct(&lencode, lengths, 19);
    if (err != 0) return -4;            /* require complete code set here */

    /* read length/literal and distance code length tables */
    index = 0;
    while (index < nlen + ndist) {
        int symbol;             /* decoded value */
        int len;                /* last length to repeat */

        symbol = decode(s, &lencode);
        if (symbol < 16)                /* length in 0..15 */
            lengths[index++] = symbol;
        else {                          /* repeat instruction */
            len = 0;                    /* assume repeating zeros */
            if (symbol == 16) {         /* repeat last length 3..6 times */
                if (index == 0) return -5;      /* no last length! */
                len = lengths[index - 1];       /* last length */
                symbol = 3 + bits(s, 2);
            }
            else if (symbol == 17)      /* repeat zero 3..10 times */
                symbol = 3 + bits(s, 3);
            else                        /* == 18, repeat zero 11..138 times */
                symbol = 11 + bits(s, 7);
            if (index + symbol > nlen + ndist)
                return -6;              /* too many lengths! */
            while (symbol--)            /* repeat last or zero symbol times */
                lengths[index++] = len;
        }
    }

    /* build huffman table for literal/length codes */
    err = construct(&lencode, lengths, nlen);
    if (err < 0 || (err > 0 && nlen - lencode.count[0] != 1))
        return -7;      /* only allow incomplete codes if just one code */

    /* build huffman table for distance codes */
    err = construct(&distcode, lengths + nlen, ndist);
    if (err < 0 || (err > 0 && ndist - distcode.count[0] != 1))
        return -8;      /* only allow incomplete codes if just one code */

    /* decode data until end-of-block code */
    return codes(s, &lencode, &distcode);
}

/*
 * Inflate source to dest.  On return, destlen and sourcelen are updated to the
 * size of the uncompressed data and the size of the deflate data respectively.
 * On success, the return value of puff() is zero.  If there is an error in the
 * source data, i.e. it is not in the deflate format, then a negative value is
 * returned.  If there is not enough input available or there is not enough
 * output space, then a positive error is returned.  In that case, destlen and
 * sourcelen are not updated to facilitate retrying from the beginning with the
 * provision of more input data or more output space.  In the case of invalid
 * inflate data (a negative error), the dest and source pointers are updated to
 * facilitate the debugging of deflators.
 *
 * puff() also has a mode to determine the size of the uncompressed output with
 * no output written.  For this dest must be (unsigned char *)0.  In this case,
 * the input value of *destlen is ignored, and on return *destlen is set to the
 * size of the uncompressed output.
 *
 * The return codes are:
 *
 *   2:  available inflate data did not terminate
 *   1:  output space exhausted before completing inflate
 *   0:  successful inflate
 *  -1:  invalid block type (type == 3)
 *  -2:  stored block length did not match one's complement
 *  -3:  dynamic block code description: too many length or distance codes
 *  -4:  dynamic block code description: code lengths codes incomplete
 *  -5:  dynamic block code description: repeat lengths with no first length
 *  -6:  dynamic block code description: repeat more than specified lengths
 *  -7:  dynamic block code description: invalid literal/length code lengths
 *  -8:  dynamic block code description: invalid distance code lengths
 *  -9:  invalid literal/length or distance code in fixed or dynamic block
 * -10:  distance is too far back in fixed or dynamic block
 *
 * Format notes:
 *
 * - Three bits are read for each block to determine the kind of block and
 *   whether or not it is the last block.  Then the block is decoded and the
 *   process repeated if it was not the last block.
 *
 * - The leftover bits in the last byte of the deflate data after the last
 *   block (if it was a fixed or dynamic block) are undefined and have no
 *   expected values to check.
 */
int puff(unsigned char *dest,           /* pointer to destination pointer */
         unsigned long *destlen,        /* amount of output space */
         unsigned char *source,         /* pointer to source data pointer */
         unsigned long *sourcelen)      /* amount of input available */
{
    struct state s;             /* input/output state */
    int last, type;             /* block information */
    int err;                    /* return value */

    /* initialize output state */
    s.out = dest;
    s.outlen = *destlen;                /* ignored if dest is NIL */
    s.outcnt = 0;

    /* initialize input state */
    s.in = source;
    s.inlen = *sourcelen;
    s.incnt = 0;
    s.bitbuf = 0;
    s.bitcnt = 0;

    /* return if bits() or decode() tries to read past available input */
    if (setjmp(s.env) != 0)             /* if came back here via longjmp() */
        err = 2;                        /* then skip do-loop, return error */
    else {
        /* process blocks until last block or error */
        do {
            last = bits(&s, 1);         /* one if last block */
            type = bits(&s, 2);         /* block type 0..3 */
            err = type == 0 ? stored(&s) :
                  (type == 1 ? fixed(&s) :
                   (type == 2 ? dynamic(&s) :
                    -1));               /* type == 3, invalid */
            if (err != 0) break;        /* return with error */
        } while (!last);
    }

    /* update the lengths and return */
    if (err <= 0) {
        *destlen = s.outcnt;
        *sourcelen = s.incnt;
    }
    return err;
}





































/* 
 *  lpng.h, version 16-01-2009
 *
 *  Copyright (c) 2009, Alex Pankratov, ap-at-swapped-dot-cc
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use,
 *  copy, modify, merge, publish, distribute, sublicense, and/or 
 *  sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following
 *  conditions:
 *  
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *  OTHER DEALINGS IN THE SOFTWARE.
 */


/* 
 *  lpng.c, version 16-01-2009
 *
 *  Copyright (c) 2009, Alex Pankratov, ap-at-swapped-dot-cc
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use,
 *  copy, modify, merge, publish, distribute, sublicense, and/or 
 *  sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following
 *  conditions:
 *  
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *  OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 *	The following constant limits the maximum size of a PNG 
 *	image. Larger images are not loaded. 
 *
 *	The limit is arbitrary and can be increased as needed.
 *	It exists solely to prevent the accidental loading of 
 *	very large images.
 */
#define MAX_IDAT_SIZE (16*1024*1024)

/*
 *
 */
#pragma warning (disable: 4996)

typedef unsigned char  uchar;
typedef unsigned long  ulong;
typedef struct _png    png_t;
typedef struct _buf    buf_t;

struct _png
{
	ulong w;
	ulong h;
	uchar bpp; /* 3 - truecolor, 4 - truecolor + alpha */
	uchar pix[1];
};

struct _buf
{
	uchar * ptr;
	ulong   len;
};

typedef int (* read_cb)(uchar * buf, ulong len, void * arg);

//
static __inline ulong get_ulong(uchar * v)
{
	ulong r = v[0];
	r = (r << 8) | v[1];
	r = (r << 8) | v[2];
	return (r << 8) | v[3];
}

static __inline uchar paeth(uchar a, uchar b, uchar c)
{
	int p = a + b - c;
        int pa = p > a ? p-a : a-p;
	int pb = p > b ? p-b : b-p;
	int pc = p > c ? p-c : c-p;
        return (pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c;
}

static const uchar png_sig[] = { 137, 80, 78, 71, 13, 10, 26, 10 };

/*
 *
 */
static png_t * LoadPngEx(read_cb read, void * read_arg)
{
	png_t * png = NULL;

	uchar buf[64];
	ulong w, h, i, j;
	uchar bpp;
	ulong len;
	uchar * tmp, * dat = 0;
	ulong dat_max, dat_len;
	ulong png_max, png_len;
	ulong out_len;
	int   r;
	uchar * line, * prev;

	/* sig and IHDR */
	if (! read(buf, 8 + 8 + 13 + 4, read_arg))
		return NULL;

	if (memcmp(buf, png_sig, 8) != 0)
		return NULL;

	len = get_ulong(buf+8);
	if (len != 13)
		return NULL;
	
	if (memcmp(buf+12, "IHDR", 4) != 0)
		return NULL;

	w = get_ulong(buf+16);
	h = get_ulong(buf+20);

	if (buf[24] != 8)
		return NULL;
	
	/* truecolor pngs only please */
	if ((buf[25] & 0x03) != 0x02)
		return NULL;

	/* check for the alpha channel */
	bpp = (buf[25] & 0x04) ? 4 : 3;

	if (buf[26] != 0 || buf[27] != 0 || buf[28] != 0)
		return NULL;

	/* IDAT */
	dat_max = 0;
	for (;;)
	{
		if (! read(buf, 8, read_arg))
			goto err;

		len = get_ulong(buf);
		if (memcmp(buf+4, "IDAT", 4) != 0)
		{
			if (! read(0, len+4, read_arg))
				goto err;

			if (memcmp(buf+4, "IEND", 4) == 0)
				break;
		
			continue;
		}

		/*
		 *	see comment at the top of this file
		 */
		if (len > MAX_IDAT_SIZE)
			goto err;

		if (len+4 > dat_max)
		{
			dat_max = len+4;
			tmp = (uchar*)realloc(dat, dat_max);
			if (! tmp)
				goto err;
			dat = tmp;
		}

		if (! read(dat, len+4, read_arg))
			goto err;

		if ((dat[0] & 0x0f) != 0x08 ||  /* compression method (rfc 1950) */
		    (dat[0] & 0xf0) > 0x70)     /* window size */
			goto err;

		if ((dat[1] & 0x20) != 0)       /* preset dictionary present */
			goto err;

		if (! png)
		{
			png_max = (w * bpp + 1) * h;
			png_len = 0;
			png = (png_t*)malloc(sizeof(*png) - 1 + png_max);
			if (! png)
				goto err;
			png->w = w;
			png->h = h;
			png->bpp = bpp;
		}

		out_len = png_max - png_len;
		dat_len = len - 2;
		r = puff(png->pix + png_len, &out_len, dat+2, &dat_len);
		if (r != 0)
			goto err;
		if (2+dat_len+4 != len)
			goto err;
		png_len += out_len;
	}
	free(dat);

	/* unfilter */
	len = w * bpp + 1;
	line = png->pix;
	prev = 0;
	for (i=0; i<h; i++, prev = line, line += len)
	{
		switch (line[0])
		{
		case 0: /* none */
			break;
		case 1: /* sub */
			for (j=1+bpp; j<len; j++)
				line[j] += line[j-bpp];
			break;
		case 2: /* up */
			if (! prev)
				break;
			for (j=1; j<len; j++)
				line[j] += prev[j];
			break;
		case 3: /* avg */
			if (! prev)
				goto err; /* $todo */
			for (j=1; j<=bpp; j++)
				line[j] += prev[j]/2;
			for (   ; j<len; j++)
				line[j] += (line[j-bpp] + prev[j])/2;
			break;
		case 4: /* paeth */
			if (! prev)
				goto err; /* $todo */
			for (j=1; j<=bpp; j++)
				line[j] += prev[j];
			for (   ; j<len; j++)
				line[j] += paeth(line[j-bpp], prev[j], prev[j-bpp]);			
			break;
		default:
			goto err;
		}
	}
	
	return png;
err:
	free(png);
	free(dat);
	return NULL;
}

/*
 *
 */
static int file_reader(uchar * buf, ulong len, void * arg)
{
	FILE * fh = (FILE*)arg;
	return buf ? 
		fread(buf, 1, len, fh) == len :
		fseek(fh, len, SEEK_CUR) == 0;
}

static png_t * LoadPngFile(const TCHAR * name)
{
	png_t * png = NULL;
	FILE  * fh;
#ifdef _UNICODE
	fh = _wfopen(name, L"rb");
#else
	fh = fopen(name,"rb");
#endif
	if (fh)
	{
		png = LoadPngEx(file_reader, fh);
		fclose(fh); 
	}

	return png;
}

/*
 *
 */
static int data_reader(uchar * buf, ulong len, void * arg)
{
	buf_t * m = (buf_t*)arg;
	
	if (len > m->len)
		return 0;

	if (buf)
		memcpy(buf, m->ptr, len);

	m->len -= len;
	m->ptr += len;
	return 1;
}

static png_t * LoadPngResource(const TCHAR* name, const TCHAR * type, HMODULE module)
{
	HRSRC   hRes;
	HGLOBAL hResData;
	buf_t   buf;

	hRes = FindResource(module, name, type);
	if (! hRes)
		return NULL;

	if (! SizeofResource(0, hRes))
		return NULL;

	if (! (hResData = LoadResource(0, hRes)))
		return NULL;

	if (! (buf.ptr = (uchar*)LockResource(hResData)))
		return NULL;

	buf.len = SizeofResource(0, hRes);

	return LoadPngEx(data_reader, &buf);
}

/*
 *
 */
static HBITMAP PngToDib(png_t * png, BOOL premultiply)
{
	BITMAPINFO bmi = { sizeof(bmi) };
	HBITMAP dib;
	uchar * dst;
	uchar * src, * tmp;
	uchar  alpha;
	int    bpl;
	ulong  x, y;

	if (png->bpp != 3 && png->bpp != 4)
		return NULL;

	//
	bmi.bmiHeader.biWidth = png->w;
	bmi.bmiHeader.biHeight = png->h;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	bmi.bmiHeader.biSizeImage = 0;
	bmi.bmiHeader.biXPelsPerMeter = 0;
	bmi.bmiHeader.biYPelsPerMeter = 0;
	bmi.bmiHeader.biClrUsed = 0;
	bmi.bmiHeader.biClrImportant = 0;
	
	dib = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void**)&dst, NULL, 0);
	if (! dib)
		return NULL;

	bpl = png->bpp * png->w + 1;              // bytes per line
	src = png->pix + (png->h - 1) * bpl + 1;  // start of the last line

	for (y = 0; y < png->h; y++, src -= bpl)
	{
		tmp = src;

		for (x = 0; x < png->w; x++)
		{
			alpha = (png->bpp == 4) ? tmp[3] : 0xff; // $fixme - optimize

			/*
			 *	R, G and B need to be 'pre-multiplied' to alpha as 
			 *	this is the format that AlphaBlend() expects from
			 *	an alpha-transparent DIB
			 */
			if (premultiply)
			{
				dst[0] = tmp[2] * alpha / 255;
				dst[1] = tmp[1] * alpha / 255;
				dst[2] = tmp[0] * alpha / 255;
			}
			else
			{
				dst[0] = tmp[2];
				dst[1] = tmp[1];
				dst[2] = tmp[0];
			}
			dst[3] = alpha;

			dst += 4;
			tmp += png->bpp;
		}
	}

	return dib;
}

/*
 *
 */
HBITMAP LoadPng(const TCHAR * res_name, 
		const TCHAR * res_type, 
		HMODULE         res_inst,
		BOOL            premultiply)
{
	HBITMAP  bmp;
	png_t  * png;

	if (res_type)
		png = LoadPngResource(res_name, res_type, res_inst);
	else
		png = LoadPngFile(res_name);

	if (! png)
		return NULL;

	bmp = PngToDib(png, premultiply);
	
	free(png);
	if (! bmp)
		return NULL;

	return bmp;
}

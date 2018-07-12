#pragma once

//***************************************************************************************
// d3dApp.h by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

#pragma once

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "RenderUtils.h"
#include "GameTimer.h"


class D3DApp
{
protected:

	D3DApp( HINSTANCE hInstance );
	D3DApp( const D3DApp& rhs ) = delete;
	D3DApp& operator=( const D3DApp& rhs ) = delete;
	virtual ~D3DApp();

public:

	static D3DApp* GetApp();

	HINSTANCE AppInst()const;
	HWND      MainWnd()const;
	float     AspectRatio()const;

	bool Get4xMsaaState()const;
	void Set4xMsaaState( bool value );

	int Run();

	virtual bool Initialize();
	virtual LRESULT MsgProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );

protected:
	virtual void OnResize();
	virtual void Update( const GameTimer& gt ) = 0;
	virtual void Draw( const GameTimer& gt ) = 0;

	// Convenience overrides for handling mouse input.
	virtual void OnMouseDown( WPARAM btnState, int x, int y ) { }
	virtual void OnMouseUp( WPARAM btnState, int x, int y ) { }
	virtual void OnMouseMove( WPARAM btnState, int x, int y ) { }

protected:

	bool InitMainWindow();
	
	void CalculateFrameStats();

protected:

	static D3DApp* mApp;

	HINSTANCE mhAppInst = nullptr; // application instance handle
	HWND      mhMainWnd = nullptr; // main window handle
	bool      mAppPaused = false;  // is the application paused?
	bool      mMinimized = false;  // is the application minimized?
	bool      mMaximized = false;  // is the application maximized?
	bool      mResizing = false;   // are the resize bars being dragged?
	bool      mFullscreenState = false;// fullscreen enabled

									   // Set true to use 4X MSAA (§4.1.8).  The default is false.
									   // Used to keep track of the “delta-time” and game time (§4.4).
	GameTimer mTimer;

	// Derived class should set these in derived constructor to customize starting values.
	std::wstring mMainWndCaption = L"d3d App";

	int mClientWidth = 800;
	int mClientHeight = 600;
};


#include "stdafx.h"

#include "RenderApp.h"

RenderApp::RenderApp( HINSTANCE hinstance ): D3DApp( hinstance )
{
	mMainWndCaption = L"Shapes Demo";
}

bool RenderApp::Initialize()
{
	if ( ! D3DApp::Initialize() )
		return false;

	return true;
}

void RenderApp::OnResize()
{
	D3DApp::OnResize();
}

void RenderApp::Update( const GameTimer& gt )
{
}


void RenderApp::Draw( const GameTimer& gt )
{
}

void RenderApp::OnMouseDown( WPARAM btn_state, int x, int y )
{
}

void RenderApp::OnMouseUp( WPARAM btn_state, int x, int y )
{
}

void RenderApp::OnMouseMove( WPARAM btnState, int x, int y )
{

}
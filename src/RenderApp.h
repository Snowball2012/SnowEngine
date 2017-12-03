#pragma once

#include <Luna/d3dApp.h>

class RenderApp: public D3DApp
{
public:
	RenderApp( HINSTANCE hinstance );
	RenderApp( const RenderApp& rhs ) = delete;
	RenderApp& operator=( const RenderApp& rhs ) = delete;

	// Inherited via D3DApp
	virtual bool Initialize() override;

private:
	virtual void OnResize() override;

	virtual void Update( const GameTimer& gt ) override;
	virtual void Draw( const GameTimer& gt ) override;

	virtual void OnMouseDown( WPARAM btnState, int x, int y ) override;
	virtual void OnMouseUp( WPARAM btnState, int x, int y ) override;
	virtual void OnMouseMove( WPARAM btnState, int x, int y ) override;
};
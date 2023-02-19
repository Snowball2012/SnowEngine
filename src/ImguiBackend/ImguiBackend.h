#pragma once

class ImguiBackend
{
public:
	ImguiBackend( struct SDL_Window* window );
	~ImguiBackend();

private:
	void InitRHI();
};
#include "Hud.h"

void Hud::loadHud(rapidjson::Document* in, SDL_Renderer* rendererIn)
{
	//load the renderer
	renderer = rendererIn;
}

void Hud::drawHud(TextDB* textManager, float zoom)
{
	//set render scale to 1
	SDL_RenderSetScale(renderer, 1, 1);



	//set render scale back to zoom
	SDL_RenderSetScale(renderer, zoom, zoom);
	
}

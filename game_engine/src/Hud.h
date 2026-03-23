#ifndef HUD_H
#define HUD_H


#include <string>
#include "SDL2/SDL.h"
#include "SDL2_image/SDL_image.h"
#include "rapidjson/document.h"
#include "Helper.h"
#include <filesystem>
#include "actorDB.h"
#include "TextDB.h"



class Hud
{
public:
	void loadHud(rapidjson::Document* in, SDL_Renderer* renderer);
	void drawHud(TextDB* textManager, float zoom);
private:
	SDL_Renderer* renderer = nullptr;
};

#endif
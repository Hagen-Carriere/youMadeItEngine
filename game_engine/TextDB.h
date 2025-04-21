#ifndef TEXTDB_H
#define TEXTDB_H


#include "SDL2/SDL.h"
#include <string>
#include <vector>
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include <filesystem>
#include <iostream>
#include "SDL2_ttf/SDL_ttf.h"
#include "Helper.h"

class TextDB
{
public:
	static TextDB* instance;

	struct TextDrawRequest {
		std::string content;
		std::string fontName;
		int fontSize;
		int x, y;
		SDL_Color color;
	};

	std::unordered_map<std::string, std::unordered_map<int, TTF_Font*>> fontCache;
	std::queue<TextDrawRequest> drawQueue;

	void init(rapidjson::Document* in);
	void QueueTextDraw(const std::string& content, int x, int y, const std::string& fontName, int fontSize, SDL_Color color);
	void FlushTextDraws(SDL_Renderer* renderer);
	TTF_Font* LoadFont(const std::string& fontName, int fontSize);

	static void Lua_DrawText(const std::string& str, float x, float y,
		const std::string& font, float size,
		float r, float g, float b, float a);

	void DrawText(SDL_Renderer* renderer, const std::string& text, TTF_Font* fontIn, SDL_Color font_color, int x, int y);
	TTF_Font* getFont();
	TTF_Font* font = nullptr; 
	

private:

	std::unordered_map<std::string, SDL_Texture*> textureMap = {};

};

#endif
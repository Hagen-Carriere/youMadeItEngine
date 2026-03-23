#include "TextDB.h"
TextDB* TextDB::instance = nullptr;

void TextDB::init(rapidjson::Document* in)
{
	instance = this;
	TTF_Init();

	//get font if it exists
	if ((*in).HasMember("font")) {
		//get the font 
		std::string temp;
		temp = (*in)["font"].GetString();


		//check if font exists
		if (!std::filesystem::exists("resources/fonts/" + temp + ".ttf")) {
			std::cout << "error: font " + temp + " missing";
			std::exit(0);
		}
		//load the font
		font = TTF_OpenFont(("resources/fonts/" + temp + ".ttf").c_str(), 16);
		if (!font) {
			std::cout << "error: font failed to load";
			std::exit(0);
		}
	}


}

void TextDB::QueueTextDraw(const std::string& content, int x, int y, const std::string& fontName, int fontSize, SDL_Color color) {
	TextDrawRequest req{ content, fontName, fontSize, x, y, color };
	drawQueue.push(req);
}

void TextDB::FlushTextDraws(SDL_Renderer* renderer) {
	while (!drawQueue.empty()) {
		TextDrawRequest req = drawQueue.front(); drawQueue.pop();
		TTF_Font* font = LoadFont(req.fontName, req.fontSize);
		if (font) {
			DrawText(renderer, req.content, font, req.color, req.x, req.y);
		}
	}
}

TTF_Font* TextDB::LoadFont(const std::string& fontName, int fontSize) {
	auto& sizeMap = fontCache[fontName];
	if (sizeMap.find(fontSize) != sizeMap.end()) return sizeMap[fontSize];

	std::string path = "resources/fonts/" + fontName + ".ttf";
	if (!std::filesystem::exists(path)) return nullptr;

	TTF_Font* font = TTF_OpenFont(path.c_str(), fontSize);
	if (!font) return nullptr;

	sizeMap[fontSize] = font;
	return font;
}

void TextDB::Lua_DrawText(const std::string& str, float x, float y, const std::string& font, float size, float r, float g, float b, float a)
{
	int xi = static_cast<int>(x), yi = static_cast<int>(y);
	int fontSize = static_cast<int>(size);
	SDL_Color color = {
		static_cast<Uint8>(r),
		static_cast<Uint8>(g),
		static_cast<Uint8>(b),
		static_cast<Uint8>(a)
	};

	// Assuming you can access `textManager` statically or via a global pointer
	instance->QueueTextDraw(str, xi, yi, font, fontSize, color);

}



void TextDB::DrawText(SDL_Renderer* renderer, const std::string& text, TTF_Font* fontIn, SDL_Color font_color, int x, int y)
{	
	if (text == "") {
		return;
	}

	if(!fontIn){
		return;
	}

	SDL_Texture* texture = textureMap[text];

	if (texture == nullptr) {
		// Create a surface from the string
		SDL_Surface* surface = TTF_RenderText_Solid(fontIn, text.c_str(), font_color);
		if (!surface) {
			std::cout << "error: could not create surface from text";
			return;
		}

		// Create texture from surface
		texture = SDL_CreateTextureFromSurface(renderer, surface);
		if (!texture) {
			std::cout << "error: could not create texture from surface: " << SDL_GetError();
			SDL_FreeSurface(surface);
			return;
		}
		SDL_FreeSurface(surface);
		textureMap[text] = texture;

	}


	// Convert to SDL_FRect
	SDL_FRect textFRect;
	textFRect.x = static_cast<float>(x);
	textFRect.y = static_cast<float>(y);

	// Get the width and height of the text from the texture
	Helper::SDL_QueryTexture(texture, &textFRect.w, &textFRect.h);



	// Copy the texture to the renderer
	Helper::SDL_RenderCopy(renderer, texture, NULL, &textFRect);

	// Free the surface and texture
	//SDL_DestroyTexture(texture);
}


TTF_Font* TextDB::getFont()
{
	return font;
}

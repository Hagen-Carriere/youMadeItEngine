#ifndef IMAGEDB_H
#define IMAGEDB_H


#include <string>
#include <vector>
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include <filesystem>
#include <iostream>
#include "SDL2/SDL.h"
#include "SDL2_image/SDL_image.h"
#include "Helper.h"
#include "glm/glm.hpp"
#include "SceneDB.h"




class ImageDB
{
public:
    enum class DrawType { Scene, UI };

    struct ImageDrawRequest {
		std::string imageName;
        DrawType drawType;
        float x;
        float y;
        int rotation_degrees;
		float scaleX;
		float scaleY;
		float pivotX;
		float pivotY;
		int r, g, b, a;
        int sortingOrder;
        int callOrder;
    };

    struct PixelDrawRequest {
        int x, y;
        Uint8 r, g, b, a;
    };


	void init(SDL_Renderer* rendererIn, SceneDB::camera* camIn);
	void DrawImage(SDL_Texture* image);
    void RenderAndClearAllImages();
    static void CreateDefaultParticleTextureWithName(const std::string& name);
	static SDL_Texture* getTexture(const std::string& name);



    static void Lua_Draw(const std::string& imageName, float x, float y);
    static void Lua_DrawEx(const std::string& imageName, float x, float y, float rotation,
        float scaleX, float scaleY, float pivotX, float pivotY,
        float r, float g, float b, float a, float sortingOrder);

    static void Lua_DrawUI(const std::string& imageName, float x, float y);
    static void Lua_DrawUIEx(const std::string& imageName, float x, float y,
        float r, float g, float b, float a, float sortingOrder);

    static void Lua_DrawPixel(float x, float y, float r, float g, float b, float a);

	static SDL_Texture* LoadImage(const std::string& name);

	static ImageDB* instance;
	std::unordered_map<std::string, SDL_Texture*> images;
private:
	SDL_Renderer* renderer = nullptr;
    std::vector<ImageDrawRequest> drawQueue;
    std::vector<PixelDrawRequest> pixelQueue;
    SceneDB::camera* cam;

    static inline int drawCallCounter = 0;
    static bool compareIMG(const ImageDrawRequest& a, const ImageDrawRequest& b);

};

#endif
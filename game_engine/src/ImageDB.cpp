#include "ImageDB.h"
ImageDB* ImageDB::instance = nullptr;

void ImageDB::init(SDL_Renderer* rendererIn, SceneDB::camera* camIn)
{
	renderer = rendererIn;
	instance = this;
	cam = camIn;
}


void ImageDB::DrawImage(SDL_Texture* image)
{
	//load the texture to the renderer
	Helper::SDL_RenderCopy(renderer, image, NULL, NULL);
}

void ImageDB::RenderAndClearAllImages()
{
    const float pixels_per_meter = 100.0f;
    const float zoom_factor = cam->zoomFactor;
    const glm::vec2 camPos = cam->getCameraPos();
    const glm::vec2 camSize = glm::vec2(cam->dimensions) * (1.0f / zoom_factor);

 //   std::cout << "[Camera] Pos: " << cam->getCameraPos().x << ", " << cam->getCameraPos().y
 //       << " | Zoom: " << zoom_factor
 //       << " | Window: " << cam->dimensions.x << "x" << cam->dimensions.y << std::endl;

	//std::cout << "[Camera] Size: " << camSize.x << ", " << camSize.y << std::endl
	//	<< "draws x range: " << camPos.x - camSize.x * 0.5f << ", " << camPos.x + camSize.x * 0.5f << std::endl
	//	<< "draws y range: " << camPos.y - camSize.y * 0.5f << ", " << camPos.y + camSize.y * 0.5f << std::endl;


    // Cull off-screen scene-space images (UI always rendered)
    drawQueue.erase(std::remove_if(drawQueue.begin(), drawQueue.end(),
        [&](const ImageDrawRequest& req) {
            if (req.drawType != DrawType::Scene) return false;

            glm::vec2 worldPos = glm::vec2(req.x, req.y);
            glm::vec2 rel = worldPos - camPos;

            float imageHalfWidth = 0.5f * std::abs(req.scaleX);  // conservative 1m size
            float imageHalfHeight = 0.5f * std::abs(req.scaleY);

            return (
                rel.x + imageHalfWidth < -0.5f * camSize.x / pixels_per_meter ||
                rel.x - imageHalfWidth >  0.5f * camSize.x / pixels_per_meter ||
                rel.y + imageHalfHeight < -0.5f * camSize.y / pixels_per_meter ||
                rel.y - imageHalfHeight >  0.5f * camSize.y / pixels_per_meter
                );
        }),
        drawQueue.end());

    // Sort remaining draw calls
    std::stable_sort(drawQueue.begin(), drawQueue.end(), ImageDB::compareIMG);

    for (const auto& request : drawQueue)
    {
        SDL_RenderSetScale(renderer, request.drawType == DrawType::Scene ? zoom_factor : 1.0f, request.drawType == DrawType::Scene ? zoom_factor : 1.0f);

        glm::vec2 final_rendering_position = request.drawType == DrawType::Scene
            ? glm::vec2(request.x, request.y) - camPos
            : glm::vec2(request.x, request.y);

        SDL_Texture* tex = LoadImage(request.imageName);
        if (!tex) {
            //try to load with Tiled
            std::cout << "Error: Image not found: " << request.imageName << std::endl;
            continue;
        }

        SDL_FRect tex_rect;
        Helper::SDL_QueryTexture(tex, &tex_rect.w, &tex_rect.h);

        int flip_mode = SDL_FLIP_NONE;
        if (request.scaleX < 0) flip_mode = SDL_FLIP_HORIZONTAL;
        if (request.scaleY < 0) flip_mode = SDL_FLIP_VERTICAL;

        float x_scale = glm::abs(request.scaleX);
        float y_scale = glm::abs(request.scaleY);
        tex_rect.w *= x_scale;
        tex_rect.h *= y_scale;

        SDL_FPoint pivot_point;
        pivot_point.x = request.pivotX * tex_rect.w;
        pivot_point.y = request.pivotY * tex_rect.h;

        if (request.drawType == DrawType::Scene) {
            tex_rect.x = final_rendering_position.x * pixels_per_meter + cam->dimensions.x * 0.5f * (1.0f / zoom_factor) - pivot_point.x;
            tex_rect.y = final_rendering_position.y * pixels_per_meter + cam->dimensions.y * 0.5f * (1.0f / zoom_factor) - pivot_point.y;
        }
        else {
            tex_rect.x = final_rendering_position.x - pivot_point.x;
            tex_rect.y = final_rendering_position.y - pivot_point.y;
        }

        SDL_SetTextureColorMod(tex, request.r, request.g, request.b);
        SDL_SetTextureAlphaMod(tex, request.a);

        Helper::SDL_RenderCopyEx(request.sortingOrder, request.imageName, renderer, tex, NULL, &tex_rect, request.rotation_degrees, &pivot_point, static_cast<SDL_RendererFlip>(flip_mode));

        SDL_SetTextureColorMod(tex, 255, 255, 255);
        SDL_SetTextureAlphaMod(tex, 255);
    }

    // Render queued pixels
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (const auto& px : pixelQueue) {
        SDL_SetRenderDrawColor(renderer, px.r, px.g, px.b, px.a);
        SDL_RenderDrawPoint(renderer, px.x, px.y);
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // Cleanup
    drawQueue.clear();
    pixelQueue.clear();
}


void ImageDB::CreateDefaultParticleTextureWithName(const std::string& name)
{
    // Have we already cached this default texture?
    if (instance->images.find(name) != instance->images.end())
        return;

    // Create an SDL_Surface (a cpu-side texture) with no special flags, 8 width, 8 height, 32 bits of color depth (RGBA) and no masking.
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 8, 8, 32, SDL_PIXELFORMAT_RGBA8888);

    // Ensure color set to white (255, 255, 255, 255)
    Uint32 white_color = SDL_MapRGBA(surface->format, 255, 255, 255, 255);
    SDL_FillRect(surface, NULL, white_color);

    // Create a gpu-side texture from the cpu-side surface now that we're done editing it.
    SDL_Texture* texture = SDL_CreateTextureFromSurface(instance->renderer, surface);

    // Clean up the surface and cache this default texture for future use (we'll probably spawn many particles with it).
    SDL_FreeSurface(surface);
    instance->images[name] = texture;
}

SDL_Texture* ImageDB::getTexture(const std::string& name)
{
	if (instance->images.find(name) != instance->images.end())
	{
		return instance->images[name];
	}
	else
	{
		std::cout << "Error: Texture not found: " << name << std::endl;
		return nullptr;
	}
}

void ImageDB::Lua_Draw(const std::string& imageName, float x, float y) {
    ImageDrawRequest req;
    req.imageName = imageName;
    req.x = x;
    req.y = y;
    req.rotation_degrees = 0;
    req.scaleX = 1.0f;
    req.scaleY = 1.0f;
    req.pivotX = 0.5f;
    req.pivotY = 0.5f;
    req.r = 255;
    req.g = 255;
    req.b = 255;
    req.a = 255;
    req.sortingOrder = 0;
    req.drawType = DrawType::Scene;
    req.callOrder = drawCallCounter++;
    instance->drawQueue.push_back(req);
}

void ImageDB::Lua_DrawEx(const std::string& imageName, float x, float y, float rotation,
    float scaleX, float scaleY, float pivotX, float pivotY,
    float r, float g, float b, float a, float sortingOrder) {
    ImageDrawRequest req;
    req.imageName = imageName;
    req.x = x;
    req.y = y;
    req.rotation_degrees = static_cast<int>(rotation);
    req.scaleX = scaleX;
    req.scaleY = scaleY;
    req.pivotX = pivotX;
    req.pivotY = pivotY;
    req.r = static_cast<int>(r);
    req.g = static_cast<int>(g);
    req.b = static_cast<int>(b);
    req.a = static_cast<int>(a);
    req.sortingOrder = static_cast<int>(sortingOrder);
    req.drawType = DrawType::Scene;
    instance->drawQueue.push_back(req);
}

void ImageDB::Lua_DrawUI(const std::string& imageName, float x, float y) {
    ImageDrawRequest req;
    req.imageName = imageName;
    req.x = static_cast<int>(x);
    req.y = static_cast<int>(y);
    req.r = 255;
    req.g = 255;
    req.b = 255;
    req.a = 255;
    req.rotation_degrees = 0;
    req.scaleX = 1.0f;
    req.scaleY = 1.0f;
    req.pivotX = 0;
    req.pivotY = 0;

    req.sortingOrder = 0;
    req.drawType = DrawType::UI;
    req.callOrder = drawCallCounter++;
    instance->drawQueue.push_back(req);
}

void ImageDB::Lua_DrawUIEx(const std::string& imageName, float x, float y,
    float r, float g, float b, float a, float sortingOrder) {
    ImageDrawRequest req;
    req.imageName = imageName;
    req.x = static_cast<int>(x);
    req.y = static_cast<int>(y);
    req.r = static_cast<int>(r);
    req.g = static_cast<int>(g);
    req.b = static_cast<int>(b);
    req.a = static_cast<int>(a);
    req.rotation_degrees = 0;
    req.scaleX = 1.0f;
    req.scaleY = 1.0f;
    req.pivotX = 0;
    req.pivotY = 0;

    req.sortingOrder = static_cast<int>(sortingOrder);
	req.drawType = DrawType::UI;
    req.callOrder = drawCallCounter++;
    instance->drawQueue.push_back(req);
}

void ImageDB::Lua_DrawPixel(float x, float y, float r, float g, float b, float a) {
    PixelDrawRequest req;
    req.x = static_cast<int>(x);
    req.y = static_cast<int>(y);
    req.r = static_cast<int>(r);
    req.g = static_cast<int>(g);
    req.b = static_cast<int>(b);
    req.a = static_cast<int>(a);
    instance->pixelQueue.push_back(req);
}

SDL_Texture* ImageDB::LoadImage(const std::string& name)
{
	//check if image is already loaded
	if (instance->images.find(name) != instance->images.end())
	{
		return instance->images[name];
	}

	//load the image
	std::string path = "resources/images/" + name + ".png";

	SDL_Texture* texture = IMG_LoadTexture(instance->renderer, path.c_str());

    instance->images.emplace(name, texture);
	return texture;
}

bool ImageDB::compareIMG(const ImageDrawRequest& a, const ImageDrawRequest& b)
{
	if (a.drawType == DrawType::UI && b.drawType == DrawType::Scene)
	{
		return false;
	}
	else if (a.drawType == DrawType::Scene && b.drawType == DrawType::UI)
	{
		return true;
	}
	else if (a.sortingOrder > b.sortingOrder)
	{
		return false;
	}
	else if (a.sortingOrder < b.sortingOrder)
	{
		return true;
    }
    else {
		return a.callOrder < b.callOrder;
    }
}


#ifndef SCENEDB_H
#define SCENEDB_H


#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <sstream>
#include "glm/glm.hpp"
#include "SDL2/SDL.h"



class SceneDB
{
public:
	static SceneDB* instance;

    struct camera {
        float easeFactor = 1.0f;
        float zoomFactor = 1.0f;
        glm::vec2 pos = { 0.0f, 0.0f };
        glm::vec2 offset = { 0.0f, 0.0f };
        glm::ivec2 dimensions = { 0, 0 };

        void updateCamera(glm::vec2& playerPos) {
            glm::vec2 temp = { pos.x - playerPos.x, pos.y - playerPos.y };
            pos = glm::mix(pos, playerPos, easeFactor);
        }

        glm::vec2 getCameraPos() const {
            return pos;
        }

        void SetPosition(float x, float y) { pos = { x, y }; }
        float GetPositionX() const { return pos.x; }
        float GetPositionY() const { return pos.y; }
        void SetZoom(float zoom, SDL_Renderer* renderer) {
            zoomFactor = zoom;
            SDL_RenderSetScale(renderer, zoom, zoom);
        }
        float GetZoom() const { return zoomFactor; }
    };

    void init(SDL_Renderer* rendererIn);

	camera cam;

	camera* GetCamera() { return &cam; }
    static void Camera_SetPosition(float x, float y);
    static float Camera_GetPositionX();
    static float Camera_GetPositionY();
    static void Camera_SetZoom(float zoom);
    static float Camera_GetZoom();
private:
    SDL_Renderer* renderer = nullptr;
};

#endif
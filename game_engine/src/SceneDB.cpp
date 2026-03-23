#include "SceneDB.h"
SceneDB* SceneDB::instance = nullptr;

void SceneDB::init(SDL_Renderer* rendererIn)
{
	instance = this;
	renderer = rendererIn;
}

void SceneDB::Camera_SetPosition(float x, float y)
{
	instance->cam.SetPosition(x, y);
}

float SceneDB::Camera_GetPositionX()
{
	return instance->cam.GetPositionX();
}

float SceneDB::Camera_GetPositionY()
{
	return instance->cam.GetPositionY();
}

void SceneDB::Camera_SetZoom(float zoom)
{
	instance->cam.SetZoom(zoom, instance->renderer);
}

float SceneDB::Camera_GetZoom()
{
	return instance->cam.GetZoom();
}



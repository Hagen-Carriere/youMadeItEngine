#ifndef ENGINE_H
#define ENGINE_H

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <unordered_map>
#include <thread>      
#include <chrono>     


#include "glm/glm.hpp"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "actorDB.h"
#include "ImageDB.h"
#include "SceneDB.h"
#include "TextDB.h"
#include "AudioDB.h"
#include "Hud.h"
#include "Input.h"
#include "EventBus.h"

#include "SDL2/SDL.h"
#include "SDL2_image/SDL_image.h"
#include "SDL2_mixer/SDL_mixer.h"
#include "SDL2_ttf/SDL_ttf.h"

#include "Helper.h"
#include "AudioHelper.h"

#include "lua/lua.hpp"
#include "LuaBridge/LuaBridge.h"
#include "box2d/box2d.h"

class engine {
public:
	static engine* instance;


    void GameLoop();
    void validateFiles();
    void readGameConfig();
    void Initialize();
    void getInput();
    void Update();
    void Render();
    void endGame();

    static void ReadJsonFile(const std::string& path, rapidjson::Document& out);
	
private:
    static void luaQuit();
    static void luaSleep(const int& ms);
    static int luaGetFrame();
    static void luaOpenURL(const std::string& url);

    static void luaLoadScene(const std::string& sceneName);
    static std::string luaGetCurrentScene();
    static void luaDontDestroy(actorDB::Actor* actor);





    bool isRunning = true;
    lua_State* L = nullptr;


    std::string sceneName = "";
	std::string gameTitle = "";

    int x_resolution = 640;
    int y_resolution = 360;
    float zoomFactor = 1.0f;


    SceneDB::camera* cam;

    std::vector<SDL_Event> eventQueue = {};

    rapidjson::Document gameConfig;

    bool renderingConfigExists = false;
    rapidjson::Document renderingConfig;

    Input inputManager;
    actorDB actorManager;
    ImageDB imageManager;
    TextDB textManager;

    AudioDB audioManager;
    std::unordered_map<std::string, std::string> audioFiles = {};

    // scene manager
    SceneDB sceneManager;
    rapidjson::Document* currentScene = nullptr;
	bool currentSceneTiled = false;
	std::vector<rapidjson::Document> sceneConfigs = {};
	std::vector<bool> sceneTiled = {};
	std::unordered_map<std::string, int> sceneCache = {};

    // stringstream for output after each step
    std::stringstream output = std::stringstream();

    bool loadNextScene = false;
    std::string nextSceneName = "";

    // SDL window pointer
    SDL_Window* window = nullptr;
    // SDL renderer pointer
    SDL_Renderer* renderer = nullptr;
    // SDL clear color
    SDL_Color clearColor = { 255, 255, 255, 255 };
};

#endif  // ENGINE_H

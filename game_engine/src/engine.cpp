#include "engine.h"
engine* engine::instance = nullptr;

void engine::GameLoop()
{
    validateFiles();
    readGameConfig();
    Initialize();

    while (isRunning) {
        getInput();
        Update();
        Render();
    }
    exit(0);
}

void engine::validateFiles()
{
    // check for game.config JSON file
    if (!std::filesystem::exists("resources/game.config")) {
        std::cout << "error: resources/game.config missing";
        std::exit(0);
    }
}

void engine::readGameConfig()
{
    ReadJsonFile("resources/game.config", gameConfig);

    // read the rendering.config file if it exists
    if (std::filesystem::exists("resources/rendering.config")) {
        renderingConfigExists = true;
        ReadJsonFile("resources/rendering.config", renderingConfig);
    }
}

void engine::Initialize()
{

	instance = this;


    auto getFloatAttribute = [&](float& attr, const char* key, rapidjson::Document* doc) {
        const auto s = doc->FindMember(key);
        if (s != doc->MemberEnd()) {
            attr = s->value.GetFloat();
        }
        };


    auto getStringAttribute = [&](std::string& attr, const char* key, rapidjson::Document* doc) {
        const auto s = doc->FindMember(key);
        if (s != doc->MemberEnd()) {
            attr = s->value.GetString();
        }
        };

    auto getBoolAttribute = [&](bool& attr, const char* key, rapidjson::Document* doc) {
        const auto s = doc->FindMember(key);
        if (s != doc->MemberEnd()) {
            attr = s->value.GetBool();
        }
        };

    auto getIntegerAttribute = [&](int& attr, const char* key, rapidjson::Document* doc) {
        const auto s = doc->FindMember(key);
        if (s != doc->MemberEnd()) {
            attr = s->value.GetInt();
        }
        };




    // initialize the initial scene if not present, exit the game
	getStringAttribute(sceneName, "initial_scene", &gameConfig);
    if (sceneName == "") {
        std::cout << "error: initial_scene unspecified";
        std::exit(0);
    }

	getStringAttribute(gameTitle, "game_title", &gameConfig);

    cam = sceneManager.GetCamera();

    // initialize camera, renderer, and other components
    if (renderingConfigExists) {

		getIntegerAttribute(x_resolution, "x_resolution", &renderingConfig);
		getIntegerAttribute(y_resolution, "y_resolution", &renderingConfig);
		cam->dimensions = glm::ivec2(x_resolution, y_resolution);
		int r = 0;
		getIntegerAttribute(r, "clear_color_r", &renderingConfig);
		clearColor.r = static_cast<Uint8>(r);
		r = 0;
		getIntegerAttribute(r, "clear_color_g", &renderingConfig);
		clearColor.g = static_cast<Uint8>(r);
		r = 0;
		getIntegerAttribute(r, "clear_color_b", &renderingConfig);
		clearColor.b = static_cast<Uint8>(r);
		getFloatAttribute(cam->offset.x, "cam_offset_x", &renderingConfig);
		cam->offset.x *= -100;
		getFloatAttribute(cam->offset.y, "cam_offset_y", &renderingConfig);
		cam->offset.y *= -100;
		getFloatAttribute(zoomFactor, "zoom_factor", &renderingConfig);
		cam->zoomFactor = zoomFactor;
		getFloatAttribute(cam->easeFactor, "cam_ease_factor", &renderingConfig);
    }

    // load and initialize components
    std::string scenePath = "resources/scenes/" + sceneName + ".scene";
	sceneTiled.emplace_back(false);
    if (!std::filesystem::exists(scenePath)) {
        scenePath = "resources/scenes/" + sceneName + ".json";
		sceneTiled[sceneTiled.size() - 1] = true;
        if (!std::filesystem::exists(scenePath)) {
            std::cout << "error: scene " << sceneName << " is missing" << std::endl;
            std::exit(0);
        }
    }

    //Init Lua
    L = luaL_newstate();
    luaL_openlibs(L);
    luabridge::getGlobalNamespace(L)
        .beginNamespace("Application")
        .addFunction("Quit", &engine::luaQuit)
        .addFunction("Sleep", &engine::luaSleep)
        .addFunction("GetFrame", &engine::luaGetFrame)
        .addFunction("OpenURL", &engine::luaOpenURL)
        .endNamespace()

        .beginNamespace("Input")
        .addFunction("GetKey", &Input::GetKey)
        .addFunction("GetKeyDown", &Input::GetKeyDown)
        .addFunction("GetKeyUp", &Input::GetKeyUp)
        .addFunction("GetMouseButton", &Input::GetMouseButton)
        .addFunction("GetMouseButtonDown", &Input::GetMouseButtonDown)
        .addFunction("GetMouseButtonUp", &Input::GetMouseButtonUp)
        .addFunction("GetMouseScrollDelta", &Input::GetMouseScrollDelta)
        .addFunction("HideCursor", &Input::HideCursor)
        .addFunction("ShowCursor", &Input::ShowCursor)
        .addFunction("GetMousePosition", &Input::GetMousePosition)
        .endNamespace()

        .beginNamespace("Audio")
        .addFunction("Play", &AudioDB::playChannel)
        .addFunction("Halt", &AudioDB::stopChannel)
        .addFunction("SetVolume", &AudioDB::setVolume)
        .endNamespace()

        .beginNamespace("Image")
        .addFunction("Draw", &ImageDB::Lua_Draw)
        .addFunction("DrawEx", &ImageDB::Lua_DrawEx)
        .addFunction("DrawUI", &ImageDB::Lua_DrawUI)
        .addFunction("DrawUIEx", &ImageDB::Lua_DrawUIEx)
        .addFunction("DrawPixel", &ImageDB::Lua_DrawPixel)
        .endNamespace()

        .beginNamespace("Camera")
        .addFunction("SetPosition", &SceneDB::Camera_SetPosition)
        .addFunction("GetPositionX", &SceneDB::Camera_GetPositionX)
        .addFunction("GetPositionY", &SceneDB::Camera_GetPositionY)
        .addFunction("SetZoom", &SceneDB::Camera_SetZoom)
        .addFunction("GetZoom", &SceneDB::Camera_GetZoom)
        .endNamespace()

        .beginNamespace("Scene")
        .addFunction("Load", &engine::luaLoadScene)
        .addFunction("GetCurrent", &engine::luaGetCurrentScene)
        .addFunction("DontDestroy", &engine::luaDontDestroy)
        .endNamespace()


        .beginNamespace("Text")
        .addFunction("Draw", &TextDB::Lua_DrawText)
        .endNamespace()

        .beginNamespace("Event")
        .addFunction("Publish", &EventBus::PublishLua)
        .addFunction("Subscribe", &EventBus::SubscribeLua)
        .addFunction("Unsubscribe", &EventBus::UnsubscribeLua)
        .endNamespace();




    window = Helper::SDL_CreateWindow(gameTitle.c_str(), 100, 100, x_resolution, y_resolution, SDL_WINDOW_SHOWN);
    renderer = Helper::SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    SDL_RenderSetScale(renderer, zoomFactor, zoomFactor);

    rapidjson::Document temp;
    ReadJsonFile(scenePath, temp);
	sceneConfigs.emplace_back(std::move(temp));
    sceneCache.emplace(sceneName, sceneConfigs.size() - 1);
	currentScene = &sceneConfigs.back();
	currentSceneTiled = sceneTiled.back();


    textManager.init(&gameConfig);


    inputManager.Init(L);
    audioManager.Init();
    imageManager.init(renderer, cam);
    sceneManager.init(renderer);
    actorManager.initActors(currentScene, currentSceneTiled, renderer, cam, L);



}

void engine::getInput()
{
    inputManager.LateUpdate();
    eventQueue.clear();

    SDL_Event eIn;
    while (Helper::SDL_PollEvent(&eIn)) {
        eventQueue.push_back(eIn);
    }

    for (const auto& e : eventQueue) {
        inputManager.ProcessEvent(e);
        if (e.type == SDL_QUIT) {
            isRunning = false;
        }
    }
}


void engine::Update()
{
    //std::cout << "[Engine Frame] " << Helper::GetFrameNumber() << std::endl;

    if (loadNextScene) {
        if (sceneCache.find(nextSceneName) != sceneCache.end()) {
            currentScene = &sceneConfigs[sceneCache[nextSceneName]];
			currentSceneTiled = sceneTiled[sceneCache[nextSceneName]];
        }
        else {
            std::string path = "resources/scenes/" + nextSceneName + ".scene";
			sceneTiled.emplace_back(false);
            if (!std::filesystem::exists(path)) {
                path = "resources/scenes/" + nextSceneName + ".json";
                if (!std::filesystem::exists(path)) {
                    std::cout << "error: scene " << nextSceneName << " is missing" << std::endl;
                    std::exit(0);
                }
                sceneTiled[sceneTiled.size() - 1] = true;
            }


            rapidjson::Document temp;
            ReadJsonFile(path, temp);
            sceneConfigs.emplace_back(std::move(temp));
            sceneCache[nextSceneName] = sceneConfigs.size() - 1;
            currentScene = &sceneConfigs[sceneConfigs.size() - 1];
			currentSceneTiled = sceneTiled[sceneConfigs.size() - 1];

        }



        // Initialize new scene's actors
        actorManager.initActors(currentScene, currentSceneTiled, renderer, cam, L);

        sceneName = nextSceneName;
        loadNextScene = false;
    }
    // update actors
    actorManager.updateAllActors();
    EventBus::Get().ApplyDeferredSubscriptions();



}

void engine::Render()
{

    // set the render draw color to clearColor
    SDL_SetRenderDrawColor(renderer, clearColor.r, clearColor.g, clearColor.b, clearColor.a);

    // clear the renderer
    SDL_RenderClear(renderer);

	actorManager.tiledManager.RenderAllLayers(renderer);

    imageManager.RenderAndClearAllImages();
    textManager.FlushTextDraws(renderer);

    Helper::SDL_RenderPresent(renderer);
    
}

void engine::endGame()
{
    audioManager.stopChannel(0);
}

void engine::ReadJsonFile(const std::string& path, rapidjson::Document& out)
{
    FILE* file_pointer = nullptr;
#ifdef _WIN32
    fopen_s(&file_pointer, path.c_str(), "rb");
#else
    file_pointer = fopen(path.c_str(), "rb");
#endif
    char buffer[65536];
    rapidjson::FileReadStream stream(file_pointer, buffer, sizeof(buffer));
    out.ParseStream(stream);
    std::fclose(file_pointer);

    if (out.HasParseError()) {
        std::cout << "error: parsing json at [" + path + "]" << std::endl;
        std::exit(0);
    }
}

void engine::luaQuit()
{
    std::exit(0);
}

void engine::luaSleep(const int& ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

int engine::luaGetFrame()
{
    return Helper::GetFrameNumber();
}

void engine::luaOpenURL(const std::string& url)
{
#ifdef _WIN32
    std::system(("start " + url).c_str());
#elif __APPLE__
    std::system(("open " + url).c_str());
#else
    std::system(("xdg-open " + url).c_str());
#endif

}

void engine::luaLoadScene(const std::string& sceneName)
{
    engine::instance->loadNextScene = true;
    engine::instance->nextSceneName = sceneName;
	engine::instance->sceneName = sceneName;
}

std::string engine::luaGetCurrentScene()
{
    return instance->sceneName;
}

void engine::luaDontDestroy(actorDB::Actor* actor)
{
    if (actor) {
        actor->dontDestroyOnLoad = true;
    }
}
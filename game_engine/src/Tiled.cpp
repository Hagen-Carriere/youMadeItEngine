#include "Tiled.h"

Tiled* Tiled::instance = nullptr;

std::vector<int> Tiled::init(rapidjson::Document* in, SceneDB::camera* camIn)
{
	std::vector<int> actorLayers;

	instance = this;
	camPoint = camIn;

    // Save map dimensions
    tileWidth = (*in)["tilewidth"].GetInt();
    tileHeight = (*in)["tileheight"].GetInt();
    mapWidth = (*in)["width"].GetInt();
    mapHeight = (*in)["height"].GetInt();

    // === Tileset processing ===
    const auto& tilesets = (*in)["tilesets"];
    for (const auto& tileset : tilesets.GetArray()) {
        std::string tsxPath = "resources/images/" + std::string(tileset["source"].GetString());
        instance->loadTileset(tsxPath);
    }

    // === Parse all visible tile layers ===
    const auto& layersArray = (*in)["layers"];
	int layerCount = 0;
    for (const auto& layer : layersArray.GetArray()) {
        if (layer["type"].IsString() && std::string(layer["type"].GetString()) == "tilelayer" && layer["visible"].GetBool()){
            instance->parseLayer(layer);
		}
		else if (layer["type"].IsString() && std::string(layer["type"].GetString()) == "objectgroup" && layer["visible"].GetBool()) {
			//add index to a vector to return back to initActor
			actorLayers.push_back(layerCount);
		}
		layerCount++;
    }
	return actorLayers;
}

void Tiled::loadTileset(const std::string& source) {
    rapidjson::Document tileSet;

    FILE* file_pointer = nullptr;
#ifdef _WIN32
    fopen_s(&file_pointer, source.c_str(), "rb");
#else
    file_pointer = fopen(path.c_str(), "rb");
#endif
    char buffer[65536];
    rapidjson::FileReadStream stream(file_pointer, buffer, sizeof(buffer));
    tileSet.ParseStream(stream);
    std::fclose(file_pointer);

    if (tileSet.HasParseError()) {
        std::cout << "error: parsing json at [" + source + "]" << std::endl;
        std::exit(0);
    }


    const auto& tiles = tileSet["tiles"];
    for (const auto& tile : tiles.GetArray()) {
        if (!tile.HasMember("id") || !tile.HasMember("image")) continue;

        int localID = tile["id"].GetInt();
        std::string imagePath = tile["image"].GetString();

        // Strip path and extension
        size_t lastSlash = imagePath.find_last_of('/');
        std::string cleaned = (lastSlash != std::string::npos) ? imagePath.substr(lastSlash + 1) : imagePath;
        size_t dot = cleaned.find_last_of('.');
        if (dot != std::string::npos) cleaned = cleaned.substr(0, dot);

        // Your .json sets firstgid to 1 in basic.json
        tileIDToImageName[1 + localID] = cleaned;
    }



}

void Tiled::parseLayer(const rapidjson::Value& layerData) {
    Layer parsedLayer;
    parsedLayer.name = layerData["name"].GetString();
    const auto& data = layerData["data"];

    for (rapidjson::SizeType i = 0; i < data.Size(); ++i) {
        int gid = data[i].GetInt();
        if (gid == 0) continue;  // 0 = empty tile

        int x = i % mapWidth;
        int y = i / mapWidth;
        parsedLayer.tiles.push_back({ gid, x, y });
    }

    layers[parsedLayer.name] = parsedLayer;
}

void Tiled::renderLayer(SDL_Renderer* renderer, const std::string& layerName) {
    auto it = layers.find(layerName);
    if (it == layers.end()) {
        std::cerr << "Layer not found: " << layerName << std::endl;
        return;
    }

    constexpr float pixels_per_meter = 100.0f;

    const Layer& layer = it->second;
    for (const Tile& tile : layer.tiles) {
        auto imgIt = tileIDToImageName.find(tile.tileID);
        if (imgIt == tileIDToImageName.end()) continue;

        const std::string& imageName = imgIt->second;

        float worldX = ((tile.x * tileWidth) - (camPoint->dimensions.x * 0.5f)) / pixels_per_meter;
        float worldY = ((tile.y * tileHeight + tileHeight) - (camPoint->dimensions.y * 0.5f)) / pixels_per_meter;


        //std::cout << "Rendering TileID: " << tile.tileID
        //    << " at (" << tile.x << ", " << tile.y << ")"
        //    << " => world (" << worldX << ", " << worldY << ")"
        //    << " using image: " << imageName << std::endl;

        ImageDB::Lua_DrawEx(imageName, worldX, worldY, 0, 1.0f, 1.0f, 0.0f, 1.0f, 255, 255, 255, 255, 0);
    }
}

void Tiled::RenderAllLayers(SDL_Renderer* renderer) {
    for (const auto& [name, layer] : layers) {
        renderLayer(renderer, name);
    }
}

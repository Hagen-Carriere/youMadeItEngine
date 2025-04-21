#ifndef TILED_H
#define TILED_H
#include <string>
#include <unordered_map>
#include <vector>
#include "rapidjson/document.h"
#include "SDL2/SDL.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include "ImageDB.h"
#include "SceneDB.h"

class Tiled {
public:
	static Tiled* instance;
    std::vector<int> init(rapidjson::Document* in, SceneDB::camera* camIn);
    void RenderAllLayers(SDL_Renderer* renderer);

    SceneDB::camera* camPoint;
private:
    struct Tile {
        int tileID;
        int x, y;
    };

    struct Layer {
        std::string name;
        std::vector<Tile> tiles;
    };

    std::unordered_map<std::string, Layer> layers;
    std::unordered_map<int, std::string> tileIDToImageName;

    int tileWidth = 0;
    int tileHeight = 0;
    int mapWidth = 0;
    int mapHeight = 0;

    void loadTileset(const std::string& source);
    void renderLayer(SDL_Renderer* renderer, const std::string& layerName);
    void parseLayer(const rapidjson::Value& layerData);
};

#endif  // TILED_H

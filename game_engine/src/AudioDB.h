#ifndef AUDIODB_H
#define AUDIODB_H


#include "AudioHelper.h"
#include "SDL2_mixer/SDL_mixer.h"
#include <string>
#include <unordered_map>

class AudioDB
{
public:
    static void Init();
    static int playChannel(int channel, const std::string& name, bool repeat);
    static int stopChannel(int channel);
    static void setVolume(int channel, float volume); 


private:
	static inline std::unordered_map<std::string, Mix_Chunk*> audioMap;

};

#endif
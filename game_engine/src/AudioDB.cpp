#include "AudioDB.h"

void AudioDB::Init()
{
	AudioHelper::Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
	AudioHelper::Mix_AllocateChannels(498);

}

int AudioDB::playChannel(int channel, const std::string& name, bool repeat)
{
	Mix_Chunk* audioChunk = audioMap[name];
	if (audioChunk == nullptr)
	{
		//check if file exists
		if (std::filesystem::exists("resources/audio/" + name + ".wav"))
		{
			audioChunk = AudioHelper::Mix_LoadWAV(("resources/audio/" + name + ".wav").c_str());
		}
		else if (std::filesystem::exists("resources/audio/" + name + ".ogg"))
		{
			audioChunk = AudioHelper::Mix_LoadWAV(("resources/audio/" + name + ".ogg").c_str());
		}
		else
		{
			std::cout << "error: failed to play audio clip " + name;
			std::exit(0);
		}
		
		if (audioChunk == nullptr) {
			std::cout << "error: failed to play audio clip " + name;
			std::exit(0);
		}

		audioMap.emplace(name, audioChunk);
	}
	int repeatNum = 0;
	if (repeat)
	{
		repeatNum = -1;
	}

	AudioHelper::Mix_PlayChannel(channel, audioChunk, repeatNum);


	return channel;
}

int AudioDB::stopChannel(int channel)
{
	AudioHelper::Mix_HaltChannel(channel);
	return channel;
}

void AudioDB::setVolume(int channel, float volume)
{
	AudioHelper::Mix_Volume(channel, static_cast<int>(volume));
}

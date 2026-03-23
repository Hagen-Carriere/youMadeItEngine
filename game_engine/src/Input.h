#ifndef INPUT_H
#define INPUT_H

#include "SDL2/SDL.h"
#include "glm/glm.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include "lua/lua.hpp"
#include "LuaBridge/LuaBridge.h"


enum INPUT_STATE { INPUT_STATE_UP, INPUT_STATE_JUST_BECAME_DOWN, INPUT_STATE_DOWN, INPUT_STATE_JUST_BECAME_UP };

class Input
{
public:
    void Init(lua_State* state); // Call before main loop begins.
    static void ProcessEvent(const SDL_Event& e); // Call every frame at start of event loop.
    static void LateUpdate(); // Call at frame end.

	static SDL_Scancode GetScancode(const std::string& key);
    static bool GetKey(const std::string& key);
    static bool GetKeyDown(const std::string& key);
    static bool GetKeyUp(const std::string& key);

    static luabridge::LuaRef GetMousePosition();

    static bool GetMouseButton(int button);
    static bool GetMouseButtonDown(int button);
    static bool GetMouseButtonUp(int button);
    static float GetMouseScrollDelta();

    static void HideCursor();
    static void ShowCursor();


private:
    inline static lua_State* lua_state = nullptr;

    static inline std::unordered_map<SDL_Scancode, INPUT_STATE> keyboard_states;
    static inline std::vector<SDL_Scancode> just_became_down_scancodes;
    static inline std::vector<SDL_Scancode> just_became_up_scancodes;

    static inline glm::vec2 mouse_position;
    static inline std::unordered_map<int, INPUT_STATE> mouse_button_states;
    static inline std::vector<int> just_became_down_buttons;
    static inline std::vector<int> just_became_up_buttons;

    static inline float mouse_scroll_this_frame = 0;

    static const std::unordered_map<std::string, SDL_Scancode> __keycode_to_scancode;
};

#endif

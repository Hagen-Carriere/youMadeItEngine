#include "Input.h"

const std::unordered_map<std::string, SDL_Scancode> Input::__keycode_to_scancode = {
	// Directional (arrow) Keys
	{"up", SDL_SCANCODE_UP},
	{"down", SDL_SCANCODE_DOWN},
	{"right", SDL_SCANCODE_RIGHT},
	{"left", SDL_SCANCODE_LEFT},

	// Misc Keys
	{"escape", SDL_SCANCODE_ESCAPE},

	// Modifier Keys
	{"lshift", SDL_SCANCODE_LSHIFT},
	{"rshift", SDL_SCANCODE_RSHIFT},
	{"lctrl", SDL_SCANCODE_LCTRL},
	{"rctrl", SDL_SCANCODE_RCTRL},
	{"lalt", SDL_SCANCODE_LALT},
	{"ralt", SDL_SCANCODE_RALT},

	// Editing Keys
	{"tab", SDL_SCANCODE_TAB},
	{"return", SDL_SCANCODE_RETURN},
	{"enter", SDL_SCANCODE_RETURN},
	{"backspace", SDL_SCANCODE_BACKSPACE},
	{"delete", SDL_SCANCODE_DELETE},
	{"insert", SDL_SCANCODE_INSERT},

	// Character Keys
	{"space", SDL_SCANCODE_SPACE},
	{"a", SDL_SCANCODE_A},
	{"b", SDL_SCANCODE_B},
	{"c", SDL_SCANCODE_C},
	{"d", SDL_SCANCODE_D},
	{"e", SDL_SCANCODE_E},
	{"f", SDL_SCANCODE_F},
	{"g", SDL_SCANCODE_G},
	{"h", SDL_SCANCODE_H},
	{"i", SDL_SCANCODE_I},
	{"j", SDL_SCANCODE_J},
	{"k", SDL_SCANCODE_K},
	{"l", SDL_SCANCODE_L},
	{"m", SDL_SCANCODE_M},
	{"n", SDL_SCANCODE_N},
	{"o", SDL_SCANCODE_O},
	{"p", SDL_SCANCODE_P},
	{"q", SDL_SCANCODE_Q},
	{"r", SDL_SCANCODE_R},
	{"s", SDL_SCANCODE_S},
	{"t", SDL_SCANCODE_T},
	{"u", SDL_SCANCODE_U},
	{"v", SDL_SCANCODE_V},
	{"w", SDL_SCANCODE_W},
	{"x", SDL_SCANCODE_X},
	{"y", SDL_SCANCODE_Y},
	{"z", SDL_SCANCODE_Z},
	{"0", SDL_SCANCODE_0},
	{"1", SDL_SCANCODE_1},
	{"2", SDL_SCANCODE_2},
	{"3", SDL_SCANCODE_3},
	{"4", SDL_SCANCODE_4},
	{"5", SDL_SCANCODE_5},
	{"6", SDL_SCANCODE_6},
	{"7", SDL_SCANCODE_7},
	{"8", SDL_SCANCODE_8},
	{"9", SDL_SCANCODE_9},
	{"/", SDL_SCANCODE_SLASH},
	{";", SDL_SCANCODE_SEMICOLON},
	{"=", SDL_SCANCODE_EQUALS},
	{"-", SDL_SCANCODE_MINUS},
	{".", SDL_SCANCODE_PERIOD},
	{",", SDL_SCANCODE_COMMA},
	{"[", SDL_SCANCODE_LEFTBRACKET},
	{"]", SDL_SCANCODE_RIGHTBRACKET},
	{"\\", SDL_SCANCODE_BACKSLASH},
	{"'", SDL_SCANCODE_APOSTROPHE}
};

void Input::Init(lua_State* state)
{
	lua_state = state;
	//Initialize keyboard states
	for (int i = 0; i < SDL_NUM_SCANCODES; i++)
	{
		keyboard_states[(SDL_Scancode)i] = INPUT_STATE_UP;
	}

	//Initialize vectors
	just_became_down_scancodes.clear();
	just_became_up_scancodes.clear();
}

void Input::ProcessEvent(const SDL_Event& e)
{
	switch (e.type)
	{
	case SDL_KEYDOWN:
		keyboard_states[(SDL_Scancode)e.key.keysym.scancode] = INPUT_STATE_JUST_BECAME_DOWN;
		just_became_down_scancodes.push_back(e.key.keysym.scancode);
		break;
	case SDL_KEYUP:
		keyboard_states[(SDL_Scancode)e.key.keysym.scancode] = INPUT_STATE_JUST_BECAME_UP;
		just_became_up_scancodes.push_back(e.key.keysym.scancode);
		break;
	case SDL_MOUSEMOTION:
		mouse_position = glm::vec2(e.motion.x, e.motion.y);
		break;
	case SDL_MOUSEBUTTONDOWN:
		mouse_button_states[e.button.button] = INPUT_STATE_JUST_BECAME_DOWN;
		just_became_down_buttons.push_back(e.button.button);
		break;
	case SDL_MOUSEBUTTONUP:
		mouse_button_states[e.button.button] = INPUT_STATE_JUST_BECAME_UP;
		just_became_up_buttons.push_back(e.button.button);
		break;
	case SDL_MOUSEWHEEL:
		mouse_scroll_this_frame = e.wheel.preciseY;
		break;
	default:
		break;
	}
}

void Input::LateUpdate()
{
	// update keyboard states
	for (SDL_Scancode code : just_became_down_scancodes)
		keyboard_states[code] = INPUT_STATE_DOWN;

	for (SDL_Scancode code : just_became_up_scancodes)
		keyboard_states[code] = INPUT_STATE_UP;

	just_became_down_scancodes.clear();
	just_became_up_scancodes.clear();

	// update mouse button states
	for (int b : just_became_down_buttons)
		mouse_button_states[b] = INPUT_STATE_DOWN;

	for (int b : just_became_up_buttons)
		mouse_button_states[b] = INPUT_STATE_UP;

	just_became_down_buttons.clear();
	just_became_up_buttons.clear();

	// reset scroll delta
	mouse_scroll_this_frame = 0.0f;
}

SDL_Scancode Input::GetScancode(const std::string& key)
{
	auto it = __keycode_to_scancode.find(key);
	if (it != __keycode_to_scancode.end()) {
		return it->second;
	}
	return SDL_SCANCODE_UNKNOWN;
}

bool Input::GetKey(const std::string& key)
{
	SDL_Scancode keycode = GetScancode(key);
	if (keyboard_states[keycode] == INPUT_STATE_DOWN || keyboard_states[keycode] == INPUT_STATE_JUST_BECAME_DOWN)
	{
		return true;
	}
	return false;
	
}

bool Input::GetKeyDown(const std::string& key)
{
	SDL_Scancode keycode = GetScancode(key);
	if (keyboard_states[keycode] == INPUT_STATE_JUST_BECAME_DOWN)
	{
		return true;
	}
	return false;
}

bool Input::GetKeyUp(const std::string& key)
{
	SDL_Scancode keycode = GetScancode(key);
	if (keyboard_states[keycode] == INPUT_STATE_JUST_BECAME_UP)
	{
		return true;
	}
	return false;
}

luabridge::LuaRef Input::GetMousePosition()
{
	lua_State* L = Input::lua_state;
	luabridge::LuaRef table = luabridge::newTable(L);
	table["x"] = mouse_position.x;
	table["y"] = mouse_position.y;
	return table;

}

bool Input::GetMouseButton(int button)
{
	return mouse_button_states[button] == INPUT_STATE_DOWN || mouse_button_states[button] == INPUT_STATE_JUST_BECAME_DOWN;
}

bool Input::GetMouseButtonDown(int button)
{
	return mouse_button_states[button] == INPUT_STATE_JUST_BECAME_DOWN;
}

bool Input::GetMouseButtonUp(int button)
{
	return mouse_button_states[button] == INPUT_STATE_JUST_BECAME_UP;
}

float Input::GetMouseScrollDelta()
{
	return mouse_scroll_this_frame;
}

void Input::HideCursor()
{
	SDL_ShowCursor(SDL_DISABLE);
}

void Input::ShowCursor()
{
	SDL_ShowCursor(SDL_ENABLE);
}



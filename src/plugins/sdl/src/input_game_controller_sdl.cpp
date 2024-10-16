#include "input_game_controller_sdl.h"

#include <SDL.h>
#include <SDL_gamecontroller.h>

#include "halley/support/console.h"

using namespace Halley;

InputGameControllerSDL::InputGameControllerSDL(int number)
{
	controller = SDL_GameControllerOpen(number);
	if (!controller) {
		throw Exception("Could not open Game Controller " + toString(number) + ": " + toString(SDL_GetError()), HalleyExceptions::InputPlugin);
	}
	id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
	idx = number;
	name = String(SDL_GameControllerName(controller));
	const auto type = SDL_GameControllerGetType(controller);

	// Category
	if (type == SDL_CONTROLLER_TYPE_XBOX360 || type == SDL_CONTROLLER_TYPE_XBOXONE) {
		joystickType = JoystickType::Xbox;
	} else if (type == SDL_CONTROLLER_TYPE_PS3 || type == SDL_CONTROLLER_TYPE_PS4 || type == SDL_CONTROLLER_TYPE_PS5) {
		joystickType = JoystickType::Playstation;
	} else if (type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO || type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR) {
		joystickType = JoystickType::SwitchFull;
	} else if (type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT) {
		joystickType = JoystickType::SwitchLeftJoycon;
	} else if (type == SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT) {
		joystickType = JoystickType::SwitchRightJoycon;
	} else {
		auto nameLower = name.asciiLower();
		if (nameLower.contains("xinput") || nameLower.contains("xbox")) {
			joystickType = JoystickType::Xbox;
		} else if (nameLower.contains("playstation") || nameLower.contains("dualshock") || nameLower.contains("dualsense")) {
			joystickType = JoystickType::Playstation;
		} else if (nameLower.contains("nintendo") || nameLower.contains("switch")) {
			joystickType = JoystickType::SwitchFull;
		} else {
			joystickType = JoystickType::Generic;
		}
	}

	// Axes
	axes.resize(SDL_CONTROLLER_AXIS_MAX);
	axisAdjust = &defaultAxisAdjust;

	// Buttons
	init(SDL_CONTROLLER_BUTTON_MAX + 2);

	setEnabled(true);

	std::cout << "\tInitialized SDL Game Controller: \"" << ConsoleColour(Console::DARK_GREY) << getName() << " (idx = " << idx << ")" << ConsoleColour() << "\".\n";
	std::cout << "\t* Mapping: \"" << ConsoleColour(Console::DARK_GREY) << getMapping() << ConsoleColour() << "\".\n";
}

InputGameControllerSDL::~InputGameControllerSDL()
{
	close();
}

void InputGameControllerSDL::close()
{
	if (controller) {
		std::cout << "\tRemoved SDL Game Controller: \"" << ConsoleColour(Console::DARK_GREY) << getName() << " (idx = " << idx << ")" << ConsoleColour() << "\".\n";
		SDL_GameControllerClose(controller);
		controller = nullptr;
		id = -1;
		idx = -1;
		setEnabled(false);
	}
}

void InputGameControllerSDL::update(Time t)
{
	clearPresses();
	InputJoystick::update(t);
}

std::string_view InputGameControllerSDL::getName() const
{
	return name;
}

String InputGameControllerSDL::getMapping() const
{
	return controller ? SDL_GameControllerMapping(controller) : nullptr;
}

JoystickType InputGameControllerSDL::getJoystickType() const
{
	return joystickType;
}

int InputGameControllerSDL::getSDLJoystickId() const
{
	return id;
}

int InputGameControllerSDL::getButtonAtPosition(JoystickButtonPosition position) const
{
	bool nintendo = joystickType == JoystickType::SwitchFull || joystickType == JoystickType::SwitchLeftJoycon || joystickType == JoystickType::SwitchRightJoycon;

	switch (position) {
		case JoystickButtonPosition::FaceTop: return nintendo ? SDL_CONTROLLER_BUTTON_X : SDL_CONTROLLER_BUTTON_Y;
		case JoystickButtonPosition::FaceRight: return nintendo ? SDL_CONTROLLER_BUTTON_A : SDL_CONTROLLER_BUTTON_B;
		case JoystickButtonPosition::FaceBottom: return nintendo ? SDL_CONTROLLER_BUTTON_B : SDL_CONTROLLER_BUTTON_A;
		case JoystickButtonPosition::FaceLeft: return nintendo ? SDL_CONTROLLER_BUTTON_Y : SDL_CONTROLLER_BUTTON_X;
		case JoystickButtonPosition::Select: return SDL_CONTROLLER_BUTTON_BACK;
		case JoystickButtonPosition::Start: return SDL_CONTROLLER_BUTTON_START;
		case JoystickButtonPosition::BumperLeft: return SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
		case JoystickButtonPosition::BumperRight: return SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
		case JoystickButtonPosition::TriggerLeft: return SDL_CONTROLLER_BUTTON_MAX;
		case JoystickButtonPosition::TriggerRight: return SDL_CONTROLLER_BUTTON_MAX + 1;
		case JoystickButtonPosition::LeftStick: return SDL_CONTROLLER_BUTTON_LEFTSTICK;
		case JoystickButtonPosition::RightStick: return SDL_CONTROLLER_BUTTON_RIGHTSTICK;
		case JoystickButtonPosition::PlatformAcceptButton: return SDL_CONTROLLER_BUTTON_A;
		case JoystickButtonPosition::PlatformCancelButton: return SDL_CONTROLLER_BUTTON_B;
		case JoystickButtonPosition::DPadUp: return SDL_CONTROLLER_BUTTON_DPAD_UP;
		case JoystickButtonPosition::DPadRight: return SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
		case JoystickButtonPosition::DPadDown: return SDL_CONTROLLER_BUTTON_DPAD_DOWN;
		case JoystickButtonPosition::DPadLeft: return SDL_CONTROLLER_BUTTON_DPAD_LEFT;
		case JoystickButtonPosition::System: return SDL_CONTROLLER_BUTTON_GUIDE;
		default: throw Exception("Invalid parameter", HalleyExceptions::InputPlugin);
	}
}

String InputGameControllerSDL::getButtonName(int code) const
{
	if (joystickType == JoystickType::Xbox || joystickType == JoystickType::Generic) {
		auto buttons = std::array<const char*, 23>{
			"xbox_a",
			"xbox_b",
			"xbox_x",
			"xbox_y",
			"xbox_back",
			"xbox_guide",
			"xbox_start",
			"xbox_lsb",
			"xbox_rsb",
			"xbox_lb",
			"xbox_rb",
			"xbox_dpad_up",
			"xbox_dpad_down",
			"xbox_dpad_left",
			"xbox_dpad_right",
			"xbox_share",
			"xbox_paddle1",
			"xbox_paddle2",
			"xbox_paddle3",
			"xbox_paddle4",
			"",
			"xbox_lt",
			"xbox_rt",
		};
		return buttons[code];
	} else if (joystickType == JoystickType::Playstation) {
		auto buttons = std::array<const char*, 23>{
			"playstation_cross",
			"playstation_circle",
			"playstation_square",
			"playstation_triangle",
			"playstation_share",
			"playstation_guide",
			"playstation_option",
			"playstation_l3",
			"playstation_r3",
			"playstation_l1",
			"playstation_r1",
			"playstation_dpad_up",
			"playstation_dpad_down",
			"playstation_dpad_left",
			"playstation_dpad_right",
			"playstation_microphone",
			"",
			"",
			"",
			"",
			"playstation_touchpad",
			"playstation_l2",
			"playstation_r2",
		};
		return buttons[code];
	} else if (joystickType == JoystickType::SwitchFull) {
		auto buttons = std::array<const char*, 23>{
			"switch_b",
			"switch_a",
			"switch_y",
			"switch_x",
			"switch_minus",
			"switch_guide",
			"switch_plus",
			"switch_lsb",
			"switch_rsb",
			"switch_l",
			"switch_r",
			"switch_dpad_up",
			"switch_dpad_down",
			"switch_dpad_left",
			"switch_dpad_right",
			"switch_capture",
			"",
			"",
			"",
			"",
			"",
			"switch_zl",
			"switch_zr",
		};
		return buttons[code];
	} else if (joystickType == JoystickType::SwitchLeftJoycon) {
		auto buttons = std::array<const char*, 23>{
			"switch_alt_b",
			"switch_alt_a",
			"switch_alt_y",
			"switch_alt_x",
			"switch_minus",
			"switch_guide",
			"switch_plus",
			"switch_lsb",
			"switch_rsb",
			"switch_alt_sl",
			"switch_alt_sr",
			"switch_dpad_up",
			"switch_dpad_down",
			"switch_dpad_left",
			"switch_dpad_right",
			"switch_capture",
			"",
			"",
			"",
			"",
			"",
			"switch_zl",
			"switch_zr",
		};
		return buttons[code];
	} else if (joystickType == JoystickType::SwitchRightJoycon) {
		auto buttons = std::array<const char*, 23>{
			"switch_alt_b",
			"switch_alt_a",
			"switch_alt_y",
			"switch_alt_x",
			"switch_minus",
			"switch_guide",
			"switch_plus",
			"switch_lsb",
			"switch_rsb",
			"switch_alt_sl",
			"switch_alt_sr",
			"switch_dpad_up",
			"switch_dpad_down",
			"switch_dpad_left",
			"switch_dpad_right",
			"switch_capture",
			"",
			"",
			"",
			"",
			"",
			"switch_zl",
			"switch_zr",
		};
		return buttons[code];
	} else {
		return InputJoystick::getButtonName(code);
	}
}

bool InputGameControllerSDL::hasLED() const
{
	return SDL_GameControllerHasLED(controller);
}

void InputGameControllerSDL::setLED(Colour4c c) const
{
	SDL_GameControllerSetLED(controller, c.r, c.g, c.b);
}

std::optional<int> InputGameControllerSDL::getPlayerIndex() const
{
	if (auto idx = SDL_GameControllerGetPlayerIndex(controller); idx != -1) {
		return idx;
	} else {
		return {};
	}
}

int InputGameControllerSDL::getControllerIndex() const
{
	return idx;
}

void InputGameControllerSDL::processEvent(const SDL_Event& event)
{
	if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP) {
		const bool down = event.type == SDL_CONTROLLERBUTTONDOWN;
		onButtonStatus(event.cbutton.button, down);
	} else if (event.type == SDL_CONTROLLERAXISMOTION) {
		const auto id = event.caxis.axis;
		const auto value = static_cast<float>(event.caxis.value) / static_cast<float>(std::numeric_limits<int16_t>::max());
		axes[id] = value;
		if (id == SDL_CONTROLLER_AXIS_TRIGGERLEFT) {
			onButtonStatus(SDL_CONTROLLER_BUTTON_MAX, value > 0.5f);
		} else if (id == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
			onButtonStatus(SDL_CONTROLLER_BUTTON_MAX + 1, value > 0.5f);
		}
	}
}

void InputGameControllerSDL::doSetVibration(float low, float high)
{
	if (controller) {
		SDL_GameControllerRumble(controller, static_cast<uint16_t>(low * 65535), static_cast<uint16_t>(high * 65535), 100);
	}
}

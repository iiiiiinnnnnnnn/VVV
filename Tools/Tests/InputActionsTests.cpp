#include <InputKit/InputKit.h>
#include <cassert>
#include <iostream>

int main()
{
	auto& input = InputKit::InputSystem::Instance();
	auto& player = input.GetActions().RegisterMap("PlayerInput");
	auto& menu = input.GetActions().RegisterMap("Menu");
	player.RegisterAction("Attack").bindings = {
		{InputKit::Device::Keyboard, VK_SPACE},
		{InputKit::Device::GamePad, InputKit::GamePad::BTN_A}};
	menu.RegisterAction("Attack").bindings = {{InputKit::Device::Keyboard, VK_RETURN}};
	assert(input.FindAction("PlayerInput/Attack")->bindings.size() == 2);
	assert(input.FindAction("Menu/Attack")->bindings.front().code == VK_RETURN);

	// Looking up missing actions must never register them during gameplay.
	const auto mapCount = input.GetActions().GetMaps().size();
	const auto actionCount = player.GetActions().size();
	assert(!input.IsDown("Missing/Attack"));
	assert(!input.FindAction("PlayerInput/Missing"));
	assert(input.GetActions().GetMaps().size() == mapCount);
	assert(player.GetActions().size() == actionCount);

	// Registering an existing map must retain the startup definitions.
	input.GetActions().RegisterMap("PlayerInput");
	assert(input.FindAction("PlayerInput/Attack")->bindings.size() == 2);
	input.SetBindings("PlayerInput/Attack", {{InputKit::Device::Keyboard, 'J'}});
	assert(input.FindAction("PlayerInput/Attack")->bindings.front().code == 'J');
	assert(input.FindAction("Menu/Attack")->bindings.front().code == VK_RETURN);
	input.RemoveAction("Menu/Attack");
	assert(!input.FindAction("Menu/Attack"));
	assert(input.FindAction("PlayerInput/Attack"));
	std::cout << "InputActions hierarchy tests passed.\n";
}

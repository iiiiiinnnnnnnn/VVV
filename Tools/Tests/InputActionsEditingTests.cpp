#include <InputKit/InputActions.h>
#include <cassert>
#include <iostream>
int main()
{
    InputKit::InputActions root;
    auto& player = root.RegisterMap("PlayerInput");
    auto& attack = player.RegisterAction("Attack");
    attack.AddBinding({InputKit::Device::Keyboard, 'J'});
    attack.AddBinding({InputKit::Device::GamePad, 16});
    assert(!attack.RemoveBinding(9));
    assert(attack.bindings.size() == 2);
    assert(!player.RenameAction("Attack", ""));
    player.RegisterAction("Sprint");
    assert(!player.RenameAction("Attack", "Sprint"));
    assert(player.RenameAction("Attack", "Fire"));
    assert(!player.FindAction("Attack"));
    assert(player.FindAction("Fire")->bindings.size() == 2);
    root.RegisterMap("Menu");
    assert(!root.RenameMap("PlayerInput", "Menu"));
    assert(!root.RenameMap("PlayerInput", "Other/Map"));
    assert(root.RenameMap("PlayerInput", "Player"));
    auto* fire = root.FindMap("Player")->FindAction("Fire");
    assert(fire && fire->bindings.front().code == 'J');
    assert(fire->SetBinding(0, {InputKit::Device::Keyboard, 'K'}));
    assert(fire->RemoveBinding(1));
    assert(fire->bindings.size() == 1 && fire->bindings.front().code == 'K');
    assert(root.RemoveMap("Player"));
    assert(!root.FindMap("Player") && root.FindMap("Menu"));
    std::cout << "InputActions editing tests passed.\n";
}
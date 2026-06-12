// Component.cpp

#include "Component.h"
#include "Actor.h"
#include "Widget.h"

Actor* Component::GetOwnerAsActor(Object* owner_sub)
{
    Actor* ptr;
    if (this->owner)
        ptr = dynamic_cast<Actor*>(this->owner);
    else
        ptr = dynamic_cast<Actor*>(owner_sub);
    _ASSERT_EXPR(ptr, "This component can only be attached to Actor.");
    return ptr;
}

Widget* Component::GetOwnerAsWidget(Object* owner_sub)
{
    Widget* ptr;
    if (this->owner)
        ptr = dynamic_cast<Widget*>(this->owner);
    else
        ptr = dynamic_cast<Widget*>(owner_sub);
    _ASSERT_EXPR(ptr, "This component can only be attached to Widget.");
    return ptr;
}

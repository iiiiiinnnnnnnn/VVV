// Component.cpp

#include "Component.h"
#include "Actor.h"
#include "Widget.h"

Actor* Component::GetOwnerAsActor(Object* owner_sub) const
{
    Actor* ptr;
    if (this->owner)
        ptr = dynamic_cast<Actor*>(this->owner);
    else
        ptr = dynamic_cast<Actor*>(owner_sub);
    _ASSERT_EXPR(ptr, "This component can only be attached to Actor.");
    return ptr;
}

Widget* Component::GetOwnerAsWidget(Object* owner_sub) const
{
    Widget* ptr;
    if (this->owner)
        ptr = dynamic_cast<Widget*>(this->owner);
    else
        ptr = dynamic_cast<Widget*>(owner_sub);
    _ASSERT_EXPR(ptr, "This component can only be attached to Widget.");
    return ptr;
}

bool Component::ShouldRenderDebug() const
{
    Actor* actor = dynamic_cast<Actor*>(owner);
    return actor && actor->IsDebugGUIOpen();
}

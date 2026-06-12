// Component.cpp

#include "Component.h"
#include "Actor.h"
#include "Widget.h"

Actor* Component::GetOwnerAsActor()
{
    Actor* actor = dynamic_cast<Actor*>(this->owner);
    _ASSERT_EXPR(actor, "This component can only be attached to Actor.");
    return actor;
}

Widget* Component::GetOwnerAsWidget()
{
    Widget* widget = dynamic_cast<Widget*>(this->owner);
    _ASSERT_EXPR(widget, "This component can only be attached to Widget.");
    return widget;
}

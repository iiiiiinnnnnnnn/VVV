// Object.inl

#pragma once

#include <type_traits>

class RectTransform;

template<typename T>
T* Object::GetComponent() const
{
	if constexpr (std::is_same_v<T, Transform>)
	{
		return const_cast<Object*>(this)->GetTransform();
	}
	else if constexpr (std::is_same_v<T, RectTransform>)
	{
		return const_cast<Object*>(this)->GetRectTransform();
	}
	else
	{
		for (auto& c : components)
		{
			if (auto* p = dynamic_cast<T*>(c.get()))
				return p;
		}
	}

	return nullptr;
}

template<typename T>
std::vector<T*> Object::GetComponents() const
{
	std::vector<T*> results;
	for (const auto& component : components)
	{
		if (auto* value = dynamic_cast<T*>(component.get())) results.push_back(value);
	}
	return results;
}

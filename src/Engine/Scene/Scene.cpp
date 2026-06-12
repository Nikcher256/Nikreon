#include "Engine/Scene/Scene.hpp"

#include <algorithm>
#include <utility>

namespace Engine {

SceneObject& Scene::createObject(std::string name)
{
    SceneObject object;
    object.id = m_nextObjectId++;
    object.name = std::move(name);

    m_objects.push_back(std::move(object));
    return m_objects.back();
}

SceneObject& Scene::createSprite2D(std::string name, std::string texturePath)
{
    SceneObject& object = createObject(std::move(name));
    object.sprite2D = Sprite2DComponent{
        .texturePath  = std::move(texturePath),
    };
    return object;
}

bool Scene::destroyObject(const SceneObjectId id)
{
    if (id == InvalidSceneObjectId) {
        return false;
    }

    const auto found = std::find_if(m_objects.begin(), m_objects.end(), [id](const SceneObject& object) {
        return object.id == id;
    });
    if (found == m_objects.end()) {
        return false;
    }

    m_objects.erase(found);
    return true;
}

void Scene::clear()
{
    m_objects.clear();
}

std::span<SceneObject> Scene::objects()
{
    return m_objects;
}

std::span<const SceneObject> Scene::objects() const
{
    return m_objects;
}

SceneObject* Scene::findObject(const SceneObjectId id)
{
    for (SceneObject& object : m_objects) {
        if (object.id == id) {
            return &object;
        }
    }

    return nullptr;
}

const SceneObject* Scene::findObject(const SceneObjectId id) const
{
    for (const SceneObject& object : m_objects) {
        if (object.id == id) {
            return &object;
        }
    }

    return nullptr;
}

std::size_t Scene::objectCount() const noexcept
{
    return m_objects.size();
}

std::size_t Scene::sprite2DCount() const noexcept
{
    std::size_t count = 0;
    for (const SceneObject& object : m_objects) {
        if (object.sprite2D) {
            ++count;
        }
    }

    return count;
}

}//namespace Engine

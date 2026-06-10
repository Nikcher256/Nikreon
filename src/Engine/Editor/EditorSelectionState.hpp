#pragma once

#include "Engine/Scene/Scene.hpp"

namespace Engine {

class EditorSelectionState {
public:
    void select(SceneObjectId objectId)
    {
        m_selectedSceneObjectId = objectId;
    }

    void clear()
    {
        m_selectedSceneObjectId = InvalidSceneObjectId;
    }

    [[nodiscard]] SceneObjectId selectedSceneObjectId() const
    {
        return m_selectedSceneObjectId;
    }

    [[nodiscard]] bool hasSelection() const
    {
        return m_selectedSceneObjectId != InvalidSceneObjectId;
    }

private:
    SceneObjectId m_selectedSceneObjectId{InvalidSceneObjectId};
};

} // namespace Engine
#pragma once

#include "Engine/Core/Layer.hpp"
#include "Engine/Editor/EditorUI.hpp"
#include "Engine/Editor/EditorViewport.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/Scene2DSubmitter.hpp"
#include "Engine/Renderer/Camera/Camera2D.hpp"

#include <glm/vec2.hpp>

namespace Engine {

class ResourceManager;
class Input;
class Renderer2D;
class Renderer2DWorld;
class TextRenderer;

class EditorLayer : public Layer {
public:
    EditorLayer();

    void onUpdate(float deltaTime, const Input& input) override;
    void onRender(
        Renderer2D& renderer2D,
        Renderer2DWorld& renderer2DWorld,
        TextRenderer& textRenderer,
        ResourceManager& resources,
        const glm::uvec2& viewportSize,
        const Input& input) override;
    [[nodiscard]] const EditorViewport& viewport() const;

private:
    void updateEditorCamera(float deltaTime, const Input& input);
    void updateViewportSelection(const Input& input, const Camera2D& camera);
    void drawSelectedSpriteOutline(Renderer2DWorld& renderer2DWorld) const;
    [[nodiscard]] SceneObjectId pickSpriteAt(const glm::vec2& worldPosition) const;
    
    glm::vec2 m_previousMousePosition{0.0f, 0.0f};
    bool m_leftMouseWasPressed{false};
    
    EditorViewport m_viewport;
    Scene m_scene;
    Scene2DSubmitter m_scene2DSubmitter;
    EditorUI m_ui;
};

} // namespace Engine

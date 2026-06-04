#pragma once

#include "Engine/Core/Layer.hpp"
#include "Engine/Editor/EditorUI.hpp"
#include "Engine/Editor/EditorViewport.hpp"
#include "Engine/Editor/EditorWorldDebugController.hpp"

#include <glm/vec2.hpp>

namespace Engine {

class Input;
class Renderer2D;
class Renderer2DWorld;
class TextRenderer;

class EditorLayer : public Layer {
public:
    EditorLayer();

    void onUpdate(float deltaTime, const Input& input) override;
    void onRender(Renderer2D& renderer2D, Renderer2DWorld& renderer2DWorld, TextRenderer& textRenderer, const glm::uvec2& viewportSize, const Input& input) override;
    [[nodiscard]] const EditorViewport& viewport() const;

private:
    void updateEditorCamera(float deltaTime, const Input& input);
    
    glm::vec2 m_previousMousePosition{0.0f, 0.0f};
    
    EditorViewport m_viewport;
    EditorWorldDebugController m_worldDebug;
    EditorUI m_ui;
};

} // namespace Engine

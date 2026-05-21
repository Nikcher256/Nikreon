#pragma once

#include "Engine/Core/Layer.hpp"
#include "Engine/Editor/EditorUI.hpp"

namespace Engine {

class Input;
class Renderer2D;

class EditorLayer : public Layer {
public:
    void onUpdate(float deltaTime, const Input& input) override;
    void onRender(Renderer2D& renderer2D, const glm::uvec2& viewportSize, const Input& input) override;

private:
    EditorUI m_ui;
};

} // namespace Engine

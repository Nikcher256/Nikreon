#pragma once

#include "Engine/Core/Layer.hpp"
#include "Engine/Editor/EditorUI.hpp"
#include "Engine/Editor/EditorViewport.hpp"
#include "Engine/Scene/Scene.hpp"
#include "Engine/Scene/SceneSpriteSubmitter.hpp"
#include "Engine/Renderer/Camera/Camera2D.hpp"
#include "Engine/Editor/EditorSelectionState.hpp"
#include "Engine/Editor/EditorViewportSelectionController.hpp"
#include "Engine/Editor/EditorViewportOverlayController.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Engine {

class ResourceManager;
class Input;
class DebugRenderer;
class Renderer2D;
class Renderer3D;
class SpriteRenderer;
class TextRenderer;

class EditorLayer : public Layer {
public:
    EditorLayer();

    void onUpdate(float deltaTime, const Input& input) override;
    void onRender(
        Renderer2D& renderer2D,
        SpriteRenderer& spriteRenderer,
        Renderer3D& renderer3D,
        DebugRenderer& debugRenderer,
        TextRenderer& textRenderer,
        ResourceManager& resources,
        const glm::uvec2& viewportSize,
        const Input& input) override;
    [[nodiscard]] const EditorViewport& viewport() const;

private:
    struct CameraFocusTarget {
        bool valid{false};
        glm::vec3 position{0.0f};
        float radius{64.0f};
    };

    void updateEditorCamera(float deltaTime, const Input& input);
    void handleDeleteSelectionInput(const Input& input);
    [[nodiscard]] CameraFocusTarget selectedCameraFocusTarget() const;
    [[nodiscard]] glm::vec2 wrapCameraMouseIfNeeded(const Input& input, const glm::vec2& mousePosition, bool cameraMouseGestureActive);

    glm::vec2 m_previousMousePosition{0.0f, 0.0f};
    bool m_cameraMouseGestureActive{false};
    
    EditorViewport m_viewport;
    Scene m_scene;
    SceneSpriteSubmitter m_sceneSpriteSubmitter;
    EditorSelectionState m_selection;
    EditorViewportSelectionController m_selectionController;
    EditorViewportOverlayController m_overlayController;
    EditorUI m_ui;
};

} // namespace Engine

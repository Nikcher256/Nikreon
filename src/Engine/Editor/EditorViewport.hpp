#pragma once

#include "Engine/Editor/EditorCamera.hpp"
#include "Engine/Renderer/Camera/Camera3D.hpp"

#include <optional>
#include <string_view>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Engine {

enum class EditorViewportMode {
    Edit,
    Play,
    Simulate,
    HudEdit,
};

enum class EditorCameraMode {
    Orthographic2D,
    Perspective3D,
};

[[nodiscard]] std::string_view editorViewportModeName(EditorViewportMode mode);
[[nodiscard]] std::string_view editorCameraModeName(EditorCameraMode mode);

struct EditorViewportPresentation {
    glm::vec2 position{0.0f, 0.0f};
    glm::vec2 size{1.0f, 1.0f};
    glm::vec4 clearColor{0.055f, 0.085f, 0.14f, 1.0f};
};


class EditorViewport {
public:
    void updateInteraction(
        const EditorViewportPresentation& presentation,
        const glm::vec2& mousePosition,
        bool leftMousePressed);
    void updateEditorCamera(float deltaTime, const EditorCameraInput& input);

    void setMode(EditorViewportMode mode);
    void setCameraMode(EditorCameraMode mode);

    [[nodiscard]] Camera3D worldCamera3D(float aspectRatio) const;
    [[nodiscard]] EditorCameraMode cameraMode() const;
    [[nodiscard]] const EditorViewportPresentation& presentation() const;
    [[nodiscard]] EditorViewportMode mode() const;
    [[nodiscard]] bool hovered() const;
    [[nodiscard]] bool focused() const;
    [[nodiscard]] const std::optional<glm::vec2>& localMousePosition() const;
    [[nodiscard]] const std::optional<glm::uvec2>& pickingCoordinates() const;
    [[nodiscard]] const glm::vec3& editorCameraPosition() const;
    [[nodiscard]] float editorCameraZoom() const;

private:
    EditorViewportPresentation m_presentation;
    EditorViewportMode m_mode{EditorViewportMode::Edit};
    EditorCameraMode m_cameraMode{EditorCameraMode::Orthographic2D};
    std::optional<glm::vec2> m_localMousePosition;
    std::optional<glm::uvec2> m_pickingCoordinates;
    EditorCamera m_editorCamera;
    bool m_hovered{false};
    bool m_focused{false};
    bool m_leftMouseWasPressed{false};
};

} // namespace Engine

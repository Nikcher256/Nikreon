#pragma once

#include <optional>
#include <string_view>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace Engine {

enum class EditorViewportMode {
    Edit,
    Play,
    Simulate,
    HudEdit,
};

[[nodiscard]] std::string_view editorViewportModeName(EditorViewportMode mode);

struct EditorViewportPresentation {
    glm::vec2 position{0.0f, 0.0f};
    glm::vec2 size{1.0f, 1.0f};
};

struct EditorCameraInput {
    float moveRight{0.0f};
    float moveUp{0.0f};
    float moveForward{0.0f};
};

class EditorViewport {
public:
    void updateInteraction(
        const EditorViewportPresentation& presentation,
        const glm::vec2& mousePosition,
        bool leftMousePressed);
    void updateEditorCamera(float deltaTime, const EditorCameraInput& input);

    void setMode(EditorViewportMode mode);

    [[nodiscard]] const EditorViewportPresentation& presentation() const;
    [[nodiscard]] EditorViewportMode mode() const;
    [[nodiscard]] bool hovered() const;
    [[nodiscard]] bool focused() const;
    [[nodiscard]] const std::optional<glm::vec2>& localMousePosition() const;
    [[nodiscard]] const std::optional<glm::uvec2>& pickingCoordinates() const;
    [[nodiscard]] const glm::vec3& editorCameraPosition() const;

private:
    EditorViewportPresentation m_presentation;
    EditorViewportMode m_mode{EditorViewportMode::Edit};
    std::optional<glm::vec2> m_localMousePosition;
    std::optional<glm::uvec2> m_pickingCoordinates;
    glm::vec3 m_editorCameraPosition{0.0f, 0.0f, 5.0f};
    bool m_hovered{false};
    bool m_focused{false};
    bool m_leftMouseWasPressed{false};
};

} // namespace Engine

#pragma once

#include "Engine/Editor/EditorSelectionState.hpp"
#include "Engine/Editor/EditorViewport.hpp"
#include "Engine/Renderer/Vulkan/VulkanContext.hpp"
#include "Engine/Resources/ResourceManager.hpp"
#include "Engine/UI/Layout.hpp"
#include "Engine/UI/UIBuilder.hpp"
#include "Engine/UI/UIContext.hpp"
#include "Engine/UI/UIStyle.hpp"
#include "Engine/Scene/Scene.hpp"

#include <cstddef>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Engine {

class Input;
class Renderer2D;
class TextRenderer;

class EditorUI {
public:
    EditorUI(EditorViewport& viewport, Scene& scene, EditorSelectionState& selection);

    void update(float deltaTime);
    void render(Renderer2D& renderer2D, TextRenderer& textRenderer, ResourceManager& resources, const glm::uvec2& viewportSize, const Input& input);
    [[nodiscard]] const UIRect& viewportBounds() const;
    [[nodiscard]] VulkanContext::PresentMode presentMode() const;
    [[nodiscard]] bool keyboardInputCaptured() const;

private:
    enum class RunState {
        Stopped,
        Playing,
        Paused,
    };

    void declareUI(float width, float height, ResourceManager& resources);
    void syncBuilderBounds();
    [[nodiscard]] bool updatePanelSplitters();
    void renderPanelSplitters(Renderer2D& renderer2D);
    void setViewportModeFromIndex(std::size_t index);
    void refreshAssets(ResourceManager& resources);
    void loadSpriteAsset(ResourceManager& resources);
    void loadModelAsset(ResourceManager& resources);
    void loadModelAssetAt(std::size_t index, ResourceManager& resources);
    void createSceneSpriteFromAsset(const TextureAssetInfo& asset, ResourceManager& resources);
    void handleAssetContextActions(ResourceManager& resources, const Input& input);
    void updateAssetContextMenu(ResourceManager& resources, const Input& input);
    void renderAssetPreviews(ResourceManager& resources, Renderer2D& renderer2D, TextRenderer& textRenderer);
    void renderAssetContextMenu(Renderer2D& renderer2D, TextRenderer& textRenderer);
    [[nodiscard]] UIRect assetContextMenuBounds() const;
    [[nodiscard]] SceneObject* selectedSceneObject();
    [[nodiscard]] std::string fpsCounterText() const;

    EditorViewport& m_viewport;
    Scene& m_scene;
    EditorSelectionState& m_selection;
    UIStyle m_style;
    UIContext m_context;
    UIBuilder m_ui;

    struct EditorStyle {
        glm::vec4 viewportBorder{0.25f, 0.32f, 0.42f, 1.0f};
        glm::vec4 viewportFocusedBorder{0.32f, 0.58f, 0.88f, 1.0f};
        float toolbarHeightMin{42.0f};
        float toolbarHeightMax{52.0f};
    } m_editorStyle;

    UIRect m_hierarchyBounds;
    UIRect m_inspectorBounds;
    UIRect m_consoleBounds;
    UIRect m_viewportBounds;
    UIRect m_hierarchySplitterBounds;
    UIRect m_inspectorSplitterBounds;
    UIRect m_consoleSplitterBounds;
    float m_toolbarHeight{48.0f};
    float m_hierarchyWidth{192.0f};
    float m_inspectorWidth{230.0f};
    float m_consoleHeight{220.0f};
    glm::vec2 m_previousMousePosition{0.0f, 0.0f};
    glm::vec2 m_renderSize{1.0f, 1.0f};
    RunState m_runState{RunState::Stopped};
    glm::vec4 m_viewportClearColor{0.055f, 0.085f, 0.14f, 1.0f};
    bool m_hierarchyVisible{true};
    bool m_inspectorVisible{true};
    bool m_consoleVisible{true};
    bool m_hierarchyCollapsed{false};
    bool m_inspectorCollapsed{false};
    bool m_consoleCollapsed{false};
    float m_previewExposure{0.65f};
    float m_lastLoggedExposure{0.65f};
    float m_lightIntensity{4.0f};
    VulkanContext::PresentMode m_presentMode{VulkanContext::PresentMode::Mailbox};
    std::vector<TextureAssetInfo> m_textureAssets;
    std::vector<ModelAssetInfo> m_modelAssets;
    bool m_assetsDirty{true};
    bool m_rightMouseWasPressed{false};
    bool m_assetContextMenuOpen{false};
    std::size_t m_assetContextAssetIndex{0};
    glm::vec2 m_assetContextMenuPosition{0.0f, 0.0f};
    float m_fpsAccumulatedTime{0.0f};
    float m_displayFps{0.0f};
    int m_fpsFrameCount{0};
};

} // namespace Engine

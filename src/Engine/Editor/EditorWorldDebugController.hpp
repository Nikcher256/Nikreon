#pragma once


#include <cstddef>
#include <string>
#include <string_view>


namespace Engine{

class Renderer2DWorld;
struct Renderer2DWorldCamera;

class EditorWorldDebugController{
public:
    explicit EditorWorldDebugController(std::string defaultSptritePath = "assets/sprites/test.png");

    void update(float deltaTime);

    void setSpritePath(std::string path);
    [[nodiscard]] std::string_view spritePath() const;

    void addSprite();
    void addManySprites();
    void toggleTilemap();
    void toggleAnimatedSprite();
    void toggleParallax();
    void toggleParticles();
    void toggleDebugShapes();
    void toggleBlendModeTest();
    void clear();

    void submit(Renderer2DWorld& renderer2DWorld, const Renderer2DWorldCamera& camera) const;

    [[nodiscard]] bool spriteLoaded() const;
    [[nodiscard]] bool blendModeTestEnabled() const;
    [[nodiscard]] std::size_t spriteCount() const;
    [[nodiscard]] bool tilemapEnabled() const;
    [[nodiscard]] bool animatedSpriteEnabled() const;
    [[nodiscard]] bool parallaxEnabled() const;
    [[nodiscard]] bool particlesEnabled() const;
    [[nodiscard]] bool debugShapesEnabled() const;

private:
    void ensureSpritePath();

    std::string m_defaultSpritePath;
    std::string m_spritePath;
    std::size_t m_spriteCount{0};
    bool m_spriteLoaded{false};
    bool m_tilemapEnabled{false};
    bool m_animatedSpriteEnabled{false};
    bool m_parallaxEnabled{false};
    bool m_particlesEnabled{false};
    bool m_debugShapesEnabled{false};
    bool m_blendModeTestEnabled{false};
    float m_elapsedSeconds{0.0f};
};
} // namespace Engine
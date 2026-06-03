#include "Engine/Renderer/RenderPipeline.hpp"

#include <cassert>
#include <vector>

using namespace Engine;

class RecordingCommandRecorder final : public RenderCommandRecorder {
public:
    void beginStage(const RenderStage stage) override
    {
        stages.push_back(stage);
    }

    void endStage(const RenderStage stage) override
    {
        (void)stage;
    }

    std::vector<RenderStage> stages;
};

int main()
{
    RenderPipeline pipeline;
    const std::vector<RenderStage> expected{
        RenderStage::ShadowMaps,
        RenderStage::Skybox,
        RenderStage::World3D,
        RenderStage::World2D,
        RenderStage::WorldSpaceUI,
        RenderStage::GameHUD,
        RenderStage::EditorOverlays,
        RenderStage::Debug,
        RenderStage::PostProcess,
        RenderStage::EditorUI,
    };

    assert(std::vector<RenderStage>(pipeline.stageSequence().begin(), pipeline.stageSequence().end()) == expected);
    assert(renderStageName(RenderStage::World2D) == "World2D");
    assert(renderStageName(RenderStage::EditorUI) == "EditorUI");

    RecordingCommandRecorder recorder;
    pipeline.beginFrame({
        {1280U, 720U},
        {{40.0f, 52.0f}, {960.0f, 540.0f}, {0.1f, 0.2f, 0.3f, 1.0f}},
        EditorViewportMode::HudEdit,
    });
    pipeline.resize({1280U, 720U});
    pipeline.recordCommands(recorder, {
        {1280U, 720U},
        {{40.0f, 52.0f}, {960.0f, 540.0f}, {0.1f, 0.2f, 0.3f, 1.0f}},
        EditorViewportMode::HudEdit,
    });
    pipeline.endFrame();

    assert(recorder.stages == expected);
    return 0;
}

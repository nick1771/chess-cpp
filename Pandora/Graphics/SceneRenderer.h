#pragma once

#include "Pandora/Mathematics/Vector.h"
#include "Pandora/Mathematics/Matrix.h"

#include "Constants.h"
#include "Camera.h"

#include <array>
#include <vector>

namespace Pandora::Implementation {

    struct SceneFrameData {
        u32 vertexBufferId{};
        u32 indexBufferId{};
    };

    struct SceneGlobalUniformBuffer {
        Matrix4f projection{};
    };

    struct Vertex {
        Vector2f position{};
        Vector2f texturePosition{};
    };
}

namespace Pandora {

    struct GraphicsDevice;
    struct Scene;

    class SceneRenderer {
    public:
        SceneRenderer(GraphicsDevice& device);

        void draw(GraphicsDevice& device, Scene& scene);

        void setCamera(GraphicsDevice& device, const Camera& camera);
    private:
        std::array<Implementation::SceneFrameData, Constants::ConcurrentFrameCount> _frameData{};

        std::vector<Implementation::Vertex> _spriteVertices{};
        std::vector<u32> _spriteIndices{};

        usize _globalUniformBufferId{};
        Implementation::SceneGlobalUniformBuffer _uniformBufferData{};

        usize _globalUniformBindGroupId{};
        usize _spriteTextureBindGroupId{};
        usize _spritePipelineId{};

        Camera _camera{};
    };
}

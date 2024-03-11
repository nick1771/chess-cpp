#include "SceneRenderer.h"
#include "GraphicsDevice.h"
#include "Scene.h"

#include "Pandora/Mathematics/MatrixTransform.h"
#include "Pandora/File.h"

namespace Pandora {

    constexpr auto SpriteVertexCount = 4;
    constexpr auto SpriteIndexCount = 6;

    SceneRenderer::SceneRenderer(GraphicsDevice& device) {
        using namespace Constants;

        auto vertexShaderByteCode = readFileToBytes("./Assets/Shaders/vertex_shader2.spv");
        auto fragmentShaderByteCode = readFileToBytes("./Assets/Shaders/fragment_shader2.spv");

        auto spritePipelineCreateInfo = PipelineCreateInfo{};
        spritePipelineCreateInfo.vertexShaderByteCode = vertexShaderByteCode;
        spritePipelineCreateInfo.fragmentShaderByteCode = fragmentShaderByteCode;

        spritePipelineCreateInfo.vertexLayout.push(VertexElementType::Float2);
        spritePipelineCreateInfo.vertexLayout.push(VertexElementType::Float2);

        auto uniformBindGroup = BindGroup{};
        uniformBindGroup.location0 = BindGroupLocationType::Vertex;
        uniformBindGroup.type0 = BindGroupElementType::UniformBuffer;

        auto textureBindGroup = BindGroup{};
        textureBindGroup.location0 = BindGroupLocationType::Fragment;
        textureBindGroup.type0 = BindGroupElementType::SamplerTexture;

        _globalUniformBindGroupId = device.createBindGroup(uniformBindGroup);
        _spriteTextureBindGroupId = device.createBindGroup(textureBindGroup);

        spritePipelineCreateInfo.bindGroupLayout.push(_globalUniformBindGroupId);
        spritePipelineCreateInfo.bindGroupLayout.push(_spriteTextureBindGroupId);

        _spritePipelineId = device.createPipeline(spritePipelineCreateInfo);

        const auto uniformBufferCreateInfo = BufferCreateInfo{ BufferType::Uniform, sizeof(Implementation::SceneGlobalUniformBuffer) };
        _globalUniformBufferId = device.createBuffer(uniformBufferCreateInfo);
        device.setBindGroupBinding(_globalUniformBindGroupId, 0, _globalUniformBufferId);

        const auto vertexBufferSize = ConcurrentFrameCount * MaximumSpriteCount * sizeof(Implementation::Vertex) * SpriteVertexCount;
        const auto indexBufferSize = ConcurrentFrameCount * MaximumSpriteCount * sizeof(u32) * SpriteIndexCount;

        const auto vertexBufferCreateInfo = BufferCreateInfo{ BufferType::Vertex, vertexBufferSize };
        const auto indexBufferCreateInfo = BufferCreateInfo{ BufferType::Index, indexBufferSize };

        for (auto& frame : _frameData) {
            frame.vertexBufferId = device.createBuffer(vertexBufferCreateInfo);
            frame.indexBufferId = device.createBuffer(indexBufferCreateInfo);
        }
    }

    static bool compareSprites(const Sprite& left, const Sprite& right) {
        if (left.zIndex < right.zIndex) {
            return true;
        } else if (left.zIndex == right.zIndex) {
            return left.texture.getDeviceHandleId() < right.texture.getDeviceHandleId();
        } else {
            return false;
        }
    }

    struct SpriteDrawCommand {
        u32 indexCount{};
        u32 quadOffset{};
        u32 textureId{};
    };

    static std::vector<SpriteDrawCommand> mapSpritesToDrawCommands(
        std::vector<Implementation::Vertex>& vertices,
        std::vector<u32>& indices,
        std::vector<Sprite>& sprites
    ) {
        auto drawCommands = std::vector<SpriteDrawCommand>{};

        std::ranges::sort(sprites, compareSprites);

        vertices.resize(sprites.size() * SpriteVertexCount);
        indices.resize(sprites.size() * SpriteIndexCount);

        auto currentIndexOffset = 0ull;
        auto currentVertexOffset = 0ull;
        auto currentQuadOffset = 0;

        auto currentCommand = SpriteDrawCommand{};
        currentCommand.textureId = sprites.front().texture.getDeviceHandleId();

        auto currentZIndex = sprites.front().zIndex;

        for (const auto& sprite : sprites) {
            const auto spriteTextureId = sprite.texture.getDeviceHandleId();
  
            if (spriteTextureId != currentCommand.textureId || currentZIndex != sprite.zIndex) {
                drawCommands.push_back(currentCommand);
                currentCommand.textureId = spriteTextureId;
                currentCommand.quadOffset = currentQuadOffset;
                currentCommand.indexCount = 0;
                currentZIndex = sprite.zIndex;
            }

            const auto size = Vector2f{ sprite.texture.getSize() } * sprite.scale;

            const auto topLeftX = sprite.position.x - sprite.origin.x;
            const auto topLeftY = sprite.position.y - sprite.origin.y;

            const auto topRightX = sprite.position.x + size.x - sprite.origin.x;
            const auto topRightY = sprite.position.y - sprite.origin.y;

            const auto bottomRightX = sprite.position.x + size.x - sprite.origin.y;
            const auto bottomRightY = sprite.position.y + size.y - sprite.origin.y;

            const auto bottomLeftX = sprite.position.x - sprite.origin.x;
            const auto bottomLeftY = sprite.position.y + size.y - sprite.origin.y;

            vertices[currentVertexOffset].position = { topLeftX, topLeftY };
            vertices[currentVertexOffset + 1].position = { topRightX, topRightY };
            vertices[currentVertexOffset + 2].position = { bottomRightX, bottomRightY };
            vertices[currentVertexOffset + 3].position = { bottomLeftX, bottomLeftY };

            vertices[currentVertexOffset].texturePosition = { 1.0f, 0.0f };
            vertices[currentVertexOffset + 1].texturePosition = { 0.0f, 0.0f };
            vertices[currentVertexOffset + 2].texturePosition = { 0.0f, 1.0f };
            vertices[currentVertexOffset + 3].texturePosition = { 1.0f, 1.0f };

            const auto quadOffset = currentCommand.indexCount / SpriteIndexCount * SpriteVertexCount;

            indices[currentIndexOffset] = quadOffset;
            indices[currentIndexOffset + 1] = quadOffset + 1;
            indices[currentIndexOffset + 2] = quadOffset + 2;

            indices[currentIndexOffset + 3] = quadOffset + 2;
            indices[currentIndexOffset + 4] = quadOffset + 3;
            indices[currentIndexOffset + 5] = quadOffset;

            currentCommand.indexCount += SpriteIndexCount;

            currentVertexOffset += SpriteVertexCount;
            currentIndexOffset += SpriteIndexCount;
            currentQuadOffset += 1;
        }

        drawCommands.push_back(currentCommand);

        return drawCommands;
    }

    void SceneRenderer::draw(GraphicsDevice& device, Scene& scene) {
        if (scene.sprites.empty()) {
            return;
        }

        const auto& frame = _frameData[device.getCurrentFrameIndex()];
        const auto drawCommands = mapSpritesToDrawCommands(_spriteVertices, _spriteIndices, scene.sprites);

        auto vertexData = std::as_bytes(std::span{ _spriteVertices });
        auto indexData = std::as_bytes(std::span{ _spriteIndices });

        device.beginCommands();

        device.copyBuffer(frame.vertexBufferId, vertexData);
        device.copyBuffer(frame.indexBufferId, indexData);

        device.setBindGroupBinding(_globalUniformBindGroupId, 0, _globalUniformBufferId);
        device.setBindGroup(_globalUniformBindGroupId, _spritePipelineId, 0);

        device.setPipeline(_spritePipelineId);
        device.setVertexBuffer(frame.vertexBufferId);
        device.setIndexBuffer(frame.indexBufferId);

        device.beginRenderPass();

        for (const auto& command : drawCommands) {
            const auto indexOffset = command.quadOffset * SpriteIndexCount;
            const auto vertexOffset = command.quadOffset * SpriteVertexCount;

            device.setBindGroupBinding(_spriteTextureBindGroupId, 0, command.textureId);
            device.setBindGroup(_spriteTextureBindGroupId, _spritePipelineId, 1);

            device.drawIndexed(command.indexCount, indexOffset, vertexOffset);
        }

        device.endAndPresent();
    }

    void SceneRenderer::setCamera(GraphicsDevice& device, const Camera& camera) {
        if (_camera.size == camera.size) {
            return;
        }

        _camera = camera;
        _uniformBufferData.projection = ortho(0.f, _camera.size.x, 0.f, _camera.size.y, -1.f, 1.f);

        auto uniformData = std::as_bytes(std::span{ &_uniformBufferData, 1 });
        device.setBufferData(_globalUniformBufferId, uniformData);
    }
}

#pragma once

#include "Texture.hpp"
#include "UniformBufferObjects.hpp"
#include <map>
#include <memory>
#include <vulkan/vulkan_core.h>
class Material {
private:
    MaterialUBO ubo{};
    std::map<TextureType, std::shared_ptr<Texture>> textures;

public:

    void addTexture(std::shared_ptr<Texture> texture);
    MaterialUBO* UBO();
    const std::map<TextureType, std::shared_ptr<Texture>>& textureMap() const;
    void destroy();
};
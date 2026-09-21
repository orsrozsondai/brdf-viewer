#include "Material.hpp"
#include "Texture.hpp"
#include "UniformBufferObjects.hpp"
#include <memory>
#include <vulkan/vulkan_core.h>

void Material::addTexture(std::shared_ptr<Texture> texture) {
    textures.insert_or_assign(texture->getType(), texture);
}

MaterialUBO* Material::UBO() {
    return &ubo;
}

const std::map<TextureType, std::shared_ptr<Texture>>& Material::textureMap() const {
    return textures;
}

void Material::destroy() {
    for (auto& [_,texture] : textures) 
        texture->destroy();
}
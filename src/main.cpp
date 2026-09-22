#include "App.hpp"
#include "Camera.hpp"
#include "EnvMap.hpp"
#include "Material.hpp"
#include "MeshLoader.hpp"
#include "Pipeline.hpp"
#include "Scene.hpp"
#include "Texture.hpp"
#include "UniformBufferObjects.hpp"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <memory>
#include <sys/types.h>
#include <utility>
#include <vulkan/vulkan_core.h>



int main() {
    auto skullMesh = std::make_unique<MeshLoader>("res/models/skull.obj", "Skull");

    auto sphereMesh = std::make_unique<MeshLoader>("res/models/sphere.obj", "Sphere");

    auto helmetMesh = std::make_unique<MeshLoader>("res/models/DamagedHelmet/DamagedHelmet.obj", "DamagedHelmet");
    auto grenadeMesh = std::make_unique<MeshLoader>("res/models/stick_grenade/stick_grenade_1k.obj", "Grenade");

    App app("BRDF viewer");

    Material helmetMaterial;
    helmetMaterial.addTexture(std::make_shared<Texture>(app.getRenderContext(), "res/models/DamagedHelmet/textures/albedo.png", TEXTURE_ALBEDO));
    helmetMaterial.addTexture(std::make_shared<Texture>(app.getRenderContext(), "res/models/DamagedHelmet/textures/normal.png", TEXTURE_NORMAL_MAP));
    helmetMaterial.addTexture(std::make_shared<Texture>(app.getRenderContext(), "res/models/DamagedHelmet/textures/roughness.png", TEXTURE_ROUGHNESS_MAP));
    helmetMaterial.addTexture(std::make_shared<Texture>(app.getRenderContext(), "res/models/DamagedHelmet/textures/metallic.png", TEXTURE_METALLIC_MAP));
    helmetMaterial.addTexture(std::make_shared<Texture>(app.getRenderContext(), "res/models/DamagedHelmet/textures/emission.png", TEXTURE_EMISSION_MAP));
    helmetMaterial.addTexture(std::make_shared<Texture>(app.getRenderContext(), "res/models/DamagedHelmet/textures/ambientOcclusion.png", TEXTURE_AMBIENT_OCCLUSION_MAP));
   
    Material grenadeMaterial;
    grenadeMaterial.addTexture(std::make_shared<Texture>(app.getRenderContext(), "res/models/stick_grenade/textures/stick_grenade_diff_1k.jpg", TEXTURE_ALBEDO));
    grenadeMaterial.addTexture(std::make_shared<Texture>(app.getRenderContext(), "res/models/stick_grenade/textures/stick_grenade_nor_gl_1k.jpg", TEXTURE_NORMAL_MAP));
    grenadeMaterial.addTexture(std::make_shared<Texture>(app.getRenderContext(), "res/models/stick_grenade/textures/stick_grenade_rough_1k.jpg", TEXTURE_ROUGHNESS_MAP));
    grenadeMaterial.addTexture(std::make_shared<Texture>(app.getRenderContext(), "res/models/stick_grenade/textures/stick_grenade_metal_1k.jpg", TEXTURE_METALLIC_MAP));
    grenadeMaterial.addTexture(std::make_shared<Texture>(app.getRenderContext(), "res/models/stick_grenade/textures/stick_grenade_ao_1k.jpg", TEXTURE_AMBIENT_OCCLUSION_MAP));
    
    
    Pipeline p = Pipeline(app.getRenderContext(), "object.vert","pbr.frag");

    
    

    int width, height;
    glfwGetWindowSize(app.getRenderContext().window, &width, &height);
    Camera camera(
        {0, 0, 0},
        5.0f,
        (float)width/height,
        45.0f
    );
    Scene scene(app.getRenderContext(), &p, &camera);
    scene.addMesh(std::move(helmetMesh), helmetMaterial);
    scene.addMesh(std::move(grenadeMesh), grenadeMaterial);
    scene.addMesh(std::move(skullMesh), {});
    scene.addMesh(std::move(sphereMesh), {});

    EnvMap env(app.getRenderContext(), "res/envmaps/ferndale_studio_01_4k.hdr", p.getDescriptorSetLayouts()[2]);
    scene.addEnvMap(&env);

    
    app.setScene(&scene);
    
    app.setCamera(&camera);
    
    app.run();
   
    app.destroy();
    return EXIT_SUCCESS;
}

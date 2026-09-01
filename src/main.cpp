#include "App.hpp"
#include "Camera.hpp"
#include "EnvMap.hpp"
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

    auto grenadeMesh = std::make_unique<MeshLoader>("res/models/stick_grenade/stick_grenade_1k.obj", "Grenade");

    App app("BRDF viewer");

    std::vector<Texture*> grenadeTex;
    grenadeTex.push_back(new Texture(app.getRenderContext(), "res/models/stick_grenade/textures/stick_grenade_diff_1k.jpg", TEXTURE_ALBEDO));
    grenadeTex.push_back(new Texture(app.getRenderContext(), "res/models/stick_grenade/textures/stick_grenade_nor_dx_1k.jpg", TEXTURE_NORMAL_MAP));
    grenadeTex.push_back(new Texture(app.getRenderContext(), "res/models/stick_grenade/textures/stick_grenade_rough_1k.jpg", TEXTURE_ROUGHNESS_MAP));
    grenadeTex.push_back(new Texture(app.getRenderContext(), "res/models/stick_grenade/textures/stick_grenade_metal_1k.jpg", TEXTURE_METALLIC_MAP));
   
    Pipeline p = Pipeline(app.getRenderContext(), "object.vert","pbr.frag");

    EnvMap env(app.getRenderContext(), "res/envmaps/rural_evening_road_4k.hdr", p.getDescriptorSetLayouts()[2]);
    
    

    int width, height;
    glfwGetWindowSize(app.getRenderContext().window, &width, &height);
    Camera camera(
        {0, 0, 0},
        5.0f,
        (float)width/height,
        45.0f
    );
    Scene scene(app.getRenderContext(), &p, &camera);
    // scene.addMesh(std::move(sphereMesh));
    // scene.addMesh(std::move(skullMesh));
    scene.addMesh(std::move(grenadeMesh), grenadeTex);
    scene.addEnvMap(&env);

    
    app.setScene(&scene);
    
    app.setCamera(&camera);
    
    app.run();
   
    app.destroy();
    for (Texture* tex : grenadeTex) delete tex;
    return EXIT_SUCCESS;
}

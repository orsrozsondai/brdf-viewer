#include "App.hpp"
#include "Camera.hpp"
#include "EnvMap.hpp"
#include "MeshLoader.hpp"
#include "Pipeline.hpp"
#include "Scene.hpp"
#include "Texture.hpp"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <memory>
#include <sys/types.h>
#include <utility>
#include <vulkan/vulkan_core.h>



int main() {
    auto skullMesh = std::make_unique<MeshLoader>("res/models/skull.obj", "Skull");

    auto sphereMesh = std::make_unique<MeshLoader>("res/models/sphere.obj", "Sphere");

    App app("BRDF viewer");

    auto texture = Texture(app.getRenderContext(), "res/models/stick_grenade/textures/stick_grenade_rough_1k.jpg", Texture::ROUGHNESS_MAP);
    std::vector<Texture*> texvec;
    texvec.push_back(&texture);
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
    scene.addMesh(std::move(sphereMesh), texvec);
    scene.addMesh(std::move(skullMesh));
    scene.addEnvMap(&env);

    
    app.setScene(&scene);
    
    app.setCamera(&camera);
    
    app.run();
   
    app.destroy();
    return EXIT_SUCCESS;
}

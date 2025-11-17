#pragma once
#include "System.h"
#include <InputHandler.h>
#include <FontSystem.h>
#include "AssetManager.h"
#include <ComponentList.h>
#include "Camera.h"
#include "Mesh.h"
#include "Terrain.h"
#include "MeshLoader.h"
#include "TextureLoader.h"
#include "ShaderProgram.h"

#include "AssetManager.h"
#include "WindowConfigProperties.h"
#include "MeshProperties.h"
#include "TextProperties.h"
// Forward declarations to avoid heavy includes in the header
struct GLFWwindow;

namespace Framework {

    class FontSystem;

    class GameHUD
    {
    public:
        GameHUD();

        // Call once after your GLFW window is created
        void Initialize(GLFWwindow* window);

        // Call every frame after you render the game view
        void Draw(FontSystem& fontSystem, int screenW, int screenH);

    private:
        GLFWwindow* m_window;
        bool        m_lastLeftDown;
    };

} // namespace Framework

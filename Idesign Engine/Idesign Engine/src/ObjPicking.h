#pragma once
//#include <glad/glad.h>
//#include <glm/glm.hpp>
//#include "ShaderProgram.h"
//
//// Double-buffered color-ID picking.
//// Usage:
////  - picker.Init(w,h);
////  - each frame (after your normal scene pass, before post):
////      picker.BeginPass(proj, view);
////      for (each object) picker.Draw(model, vao, indexCount, objectId);
////      picker.EndPass();
////      picker.SwapBuffers(); // IMPORTANT: do this once per frame
////  - on ImGui click over your Game image: picker.RequestPickUV({u,v});
////  - late in the frame: int id = picker.ConsumePick(); // -1 = no hit
//
//class ObjPicker {
//public:
//    ObjPicker() = default;
//    ~ObjPicker() { Shutdown(); }
//
//    void Init(int w, int h);
//    void Shutdown();
//    void Resize(int w, int h);
//
//    void BeginPass(const glm::mat4& proj, const glm::mat4& view);
//    void Draw(const glm::mat4& model, GLuint vao, GLsizei indexCount, uint32_t objectId);
//    void EndPass();
//
//    // Rotate write/read buffers (call once per frame after EndPass()).
//    void SwapBuffers();
//
//    // UI integration: call when user clicks the rendered image.
//    // uv.x, uv.y in [0,1], already accounting for any letterboxing.
//    void RequestPickUV(glm::vec2 uv);
//
//    // Reads from the *previous* frame's buffer if a request is pending.
//    // Returns decoded objectId (as passed to Draw), or -1 if none.
//    int ConsumePick();
//
//    // (optional) for debugging: get the "read" color texture
//    GLuint GetReadColorTex() const { return pickColor[readIdx]; }
//
//private:
//    void createFBO(int idx);
//    void destroyFBO(int idx);
//    static glm::vec3 encodeID24(uint32_t id);
//    static uint32_t decodeID24(unsigned char r, unsigned char g, unsigned char b);
//
//private:
//    // double-buffered FBOs
//    GLuint pickFBO[2]{ 0, 0 };
//    GLuint pickColor[2]{ 0, 0 };
//    GLuint pickDepth[2]{ 0, 0 };
//
//    int width = 0;
//    int height = 0;
//
//    int writeIdx = 0;   // render into this one this frame
//    int readIdx = 1;    // read from this one this frame (previous frame's result)
//
//    ShaderProgram pickShader{ "Assets/shaders/pick.vert", "Assets/shaders/pick.frag" };
//    glm::vec2 pendingUV; // set by RequestPickUV, consumed in ConsumePick
//};
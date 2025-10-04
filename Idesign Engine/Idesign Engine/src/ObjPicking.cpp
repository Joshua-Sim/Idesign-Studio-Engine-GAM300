#include"pch.h"
//#include "ObjPicking.h"
//#include <iostream>
//#include "ImguiManager.h"
//#include <glm/gtc/type_ptr.hpp>
//
//static const GLenum kDrawBuf = GL_COLOR_ATTACHMENT0;
//
//void ObjPicker::Init(int w, int h) {
//    width = w;
//    height = h;
//
//    if (!pickShader.initGL()) {
//        std::cerr << "ObjPicker: Failed to initialize pick shader!\n";
//        return;
//    }
//
//    createFBO(0);
//    createFBO(1);
//}
//
//void ObjPicker::Shutdown() {
//    destroyFBO(0);
//    destroyFBO(1);
//}
//
//void ObjPicker::Resize(int w, int h) {
//    if (w == width && h == height) return;
//
//    width = w;
//    height = h;
//
//    destroyFBO(0);
//    destroyFBO(1);
//    createFBO(0);
//    createFBO(1);
//}
//
//void ObjPicker::BeginPass(const glm::mat4& proj, const glm::mat4& view) {
//    glBindFramebuffer(GL_FRAMEBUFFER, pickFBO[writeIdx]);
//    glViewport(0, 0, width, height);
//    glEnable(GL_DEPTH_TEST);
//    glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // id=0 means "no hit"
//    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
//
//    pickShader.use();
//    pickShader.setMat4("uProj", proj);
//    pickShader.setMat4("uView", view);
//}
//
//void ObjPicker::Draw(const glm::mat4& model, GLuint vao, GLsizei indexCount, uint32_t objectId) {
//    // Reserve 0 for background; encode (objectId+1)
//    uint32_t id = objectId + 1u;
//    glm::vec3 rgb = encodeID24(id);
//
//    pickShader.setMat4("uModel", model);
//    pickShader.setVec3("uIdColor", rgb);
//
//    glBindVertexArray(vao);
//    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
//    glBindVertexArray(0);
//}
//
//void ObjPicker::EndPass() {
//    glBindFramebuffer(GL_FRAMEBUFFER, 0);
//}
//
//void ObjPicker::SwapBuffers() {
//    std::swap(writeIdx, readIdx);
//}
//
//void ObjPicker::RequestPickUV(glm::vec2 uv) {
//    pendingUV = uv;
//}
//
//int ObjPicker::ConsumePick() {
//    if (pendingUV.x < 0.0f || pendingUV.y < 0.0f) return -1;
//
//    // Convert UV to pixel coordinates
//    // Note: Flip Y because OpenGL origin is bottom-left, but UV typically top-left
//    int px = (int)glm::clamp(pendingUV.x * float(width), 0.0f, float(width - 1));
//    int py = (int)glm::clamp((1.0f - pendingUV.y) * float(height), 0.0f, float(height - 1));
//
//    // Read from previous frame's buffer (readIdx)
//    unsigned char rgb[3] = { 0, 0, 0 };
//    glBindFramebuffer(GL_READ_FRAMEBUFFER, pickFBO[readIdx]);
//    glReadPixels(px, py, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, rgb);
//    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
//
//    // Clear pending request
//    pendingUV = { -1.0f, -1.0f };
//
//    uint32_t id = decodeID24(rgb[0], rgb[1], rgb[2]);
//    if (id == 0u) return -1;     // background
//    return int(id - 1u);         // undo +1
//}
//
//// ---------- helpers ----------
//
//void ObjPicker::createFBO(int idx) {
//    glGenFramebuffers(1, &pickFBO[idx]);
//    glBindFramebuffer(GL_FRAMEBUFFER, pickFBO[idx]);
//
//    // Create color texture
//    glGenTextures(1, &pickColor[idx]);
//    glBindTexture(GL_TEXTURE_2D, pickColor[idx]);
//    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pickColor[idx], 0);
//
//    // Create depth renderbuffer
//    glGenRenderbuffers(1, &pickDepth[idx]);
//    glBindRenderbuffer(GL_RENDERBUFFER, pickDepth[idx]);
//    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
//    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, pickDepth[idx]);
//
//    glDrawBuffers(1, &kDrawBuf);
//
//    // Check FBO completeness
//    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
//    if (status != GL_FRAMEBUFFER_COMPLETE) {
//        std::cerr << "ObjPicker: FBO " << idx << " incomplete! Status: 0x"
//            << std::hex << status << std::dec << "\n";
//    }
//
//    glBindFramebuffer(GL_FRAMEBUFFER, 0);
//}
//
//void ObjPicker::destroyFBO(int idx) {
//    if (pickDepth[idx]) {
//        glDeleteRenderbuffers(1, &pickDepth[idx]);
//        pickDepth[idx] = 0;
//    }
//    if (pickColor[idx]) {
//        glDeleteTextures(1, &pickColor[idx]);
//        pickColor[idx] = 0;
//    }
//    if (pickFBO[idx]) {
//        glDeleteFramebuffers(1, &pickFBO[idx]);
//        pickFBO[idx] = 0;
//    }
//}
//
//glm::vec3 ObjPicker::encodeID24(uint32_t id) {
//    // Pack 24 bits into RGB [0..1]
//    float r = float(id & 0xFF) / 255.0f;
//    float g = float((id >> 8) & 0xFF) / 255.0f;
//    float b = float((id >> 16) & 0xFF) / 255.0f;
//    return { r, g, b };
//}
//
//uint32_t ObjPicker::decodeID24(unsigned char r, unsigned char g, unsigned char b) {
//    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16);
//}
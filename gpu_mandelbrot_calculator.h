#pragma once

#include "zoom_mandelbrot_calculator.h"
#include <SDL2/SDL.h>

// Define this to get modern OpenGL functions
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>
#include <string>

class GpuMandelbrotCalculator : public ZoomMandelbrotCalculator
{
public:
    GpuMandelbrotCalculator(int width, int height);
    ~GpuMandelbrotCalculator();

    void compute(std::function<void()> progressCallback) override;
    void reset() override;
    
    // GPU implementation doesn't return data to CPU
    const std::vector<int> &getData() const override { return dummyData; }

    bool hasOwnOutput() const override { return true; }
    void render() override;

private:
    std::vector<int> dummyData;
    
    GLuint programId;
    GLuint vao;
    GLuint vbo;
    
    // Shader uniforms
    GLint locMinR, locMinI, locMaxR, locMaxI;
    GLint locMaxIter;
    
    void initShaders();
    void initGeometry();
    GLuint compileShader(GLenum type, const std::string &source);
};

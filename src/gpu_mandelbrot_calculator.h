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
    enum class Precision
    {
        FLOAT,
        DOUBLE
    };

    GpuMandelbrotCalculator(int width, int height, Precision precision = Precision::FLOAT);
    ~GpuMandelbrotCalculator();

    void compute(std::function<void()> progressCallback) override;
    void reset() override;

    // GPU implementation now returns data to CPU
    const std::vector<int> &getData() const override { return data; }

    // No longer has own output, behaves like standard calculator
    bool hasOwnOutput() const override { return false; }
    
    std::string getEngineName() const override { 
        return (precision == Precision::FLOAT) ? " gpuf" : " gpud"; 
    }

private:
    std::vector<int> data;
    Precision precision;

    GLuint programId;
    GLuint vao;
    GLuint vbo;
    GLuint fbo;
    GLuint texture;

    // Shader uniforms
    GLint locMinR, locMinI, locMaxR, locMaxI;
    GLint locMaxIter;

    void initShaders();
    void initGeometry();
    void initFBO();
    GLuint compileShader(GLenum type, const std::string &source);
};

#include "gpu_mandelbrot_calculator.h"
#include <iostream>
#include <vector>

GpuMandelbrotCalculator::GpuMandelbrotCalculator(int w, int h)
    : ZoomMandelbrotCalculator(w, h), programId(0), vao(0), vbo(0)
{
    // Initialize dummy data to avoid crashes if someone calls getData()
    dummyData.resize(1, 0);
    
    // We assume an OpenGL context is already active when this is created
    initShaders();
    initGeometry();
}

GpuMandelbrotCalculator::~GpuMandelbrotCalculator()
{
    if (programId) glDeleteProgram(programId);
    if (vbo) glDeleteBuffers(1, &vbo);
    // VAO deletion might need check for extension support depending on GL version, 
    // but usually safe in modern GL
    // glDeleteVertexArrays(1, &vao); 
}

void GpuMandelbrotCalculator::compute(std::function<void()> progressCallback)
{
    // GPU computes during render, so nothing to do here.
    // We can just trigger the callback immediately to signal "done".
    if (progressCallback)
        progressCallback();
}

void GpuMandelbrotCalculator::reset()
{
    // Nothing to reset for GPU
}

void GpuMandelbrotCalculator::render()
{
    if (!programId) return;

    glUseProgram(programId);

    // Update uniforms
    glUniform1f(locMinR, static_cast<float>(minr));
    glUniform1f(locMinI, static_cast<float>(mini));
    glUniform1f(locMaxR, static_cast<float>(maxr));
    glUniform1f(locMaxI, static_cast<float>(maxi));
    glUniform1i(locMaxIter, MAX_ITER);

    // Draw full screen quad
    // Bind VAO if we used one, or just setup attributes
    // For simplicity/compatibility, let's use the VBO directly
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glUseProgram(0);
}

void GpuMandelbrotCalculator::initGeometry()
{
    // Full screen quad coordinates (-1 to 1)
    float vertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GpuMandelbrotCalculator::initShaders()
{
    // Vertex Shader
    const std::string vsSource = R"(
        #version 120
        attribute vec2 position;
        varying vec2 texCoord;
        void main() {
            gl_Position = vec4(position, 0.0, 1.0);
            // Map from [-1, 1] to [0, 1]
            texCoord = position * 0.5 + 0.5;
        }
    )";

    // Fragment Shader
    // Note: We use highp float. Double precision (dvec2) requires GLSL 4.0+ (GL 4.0).
    // For compatibility, we'll stick to float for now.
    // If the user has a modern GPU, we could use #version 400 and 'double'.
    const std::string fsSource = R"(
        #version 120
        
        uniform float minR;
        uniform float minI;
        uniform float maxR;
        uniform float maxI;
        uniform int maxIter;
        
        varying vec2 texCoord;
        
        void main() {
            // Map texture coordinate [0,1] to complex plane
            // Flip Y coordinate to match CPU rendering (where 0 is top)
            float texY = 1.0 - texCoord.y;
            
            float cx = minR + texCoord.x * (maxR - minR);
            float cy = minI + texY * (maxI - minI);
            
            float zx = cx;
            float zy = cy;
            
            int iter = 0;
            for (int i = 0; i < 768; ++i) { // Hardcoded maxIter for loop unrolling
                if (i >= maxIter) break;
                
                float zx2 = zx * zx;
                float zy2 = zy * zy;
                
                if (zx2 + zy2 > 4.0) {
                    iter = i;
                    break;
                }
                
                zy = 2.0 * zx * zy + cy;
                zx = zx2 - zy2 + cx;
                iter = i;
            }
            
            if (iter == maxIter || iter == 767) {
                gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            } else {
                // Simple coloring based on iteration
                float t = float(iter) / float(maxIter);
                gl_FragColor = vec4(t, t * 0.5, 1.0 - t, 1.0);
            }
        }
    )";

    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSource);

    programId = glCreateProgram();
    glAttachShader(programId, vs);
    glAttachShader(programId, fs);
    glLinkProgram(programId);

    GLint linked;
    glGetProgramiv(programId, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(programId, 512, NULL, log);
        std::cerr << "Shader Linking Error: " << log << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    // Get uniform locations
    locMinR = glGetUniformLocation(programId, "minR");
    locMinI = glGetUniformLocation(programId, "minI");
    locMaxR = glGetUniformLocation(programId, "maxR");
    locMaxI = glGetUniformLocation(programId, "maxI");
    locMaxIter = glGetUniformLocation(programId, "maxIter");
}

GLuint GpuMandelbrotCalculator::compileShader(GLenum type, const std::string &source)
{
    GLuint shader = glCreateShader(type);
    const char *src = source.c_str();
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        std::cerr << "Shader Compilation Error (" << (type == GL_VERTEX_SHADER ? "VS" : "FS") << "): " << log << std::endl;
    }
    return shader;
}

#include "gpu_mandelbrot_calculator.h"
#include <iostream>
#include <vector>

GpuMandelbrotCalculator::GpuMandelbrotCalculator(int w, int h, Precision prec)
    : ZoomMandelbrotCalculator(w, h), precision(prec), programId(0), vao(0), vbo(0), fbo(0), texture(0)
{
    data.resize(width * height);

    // We assume an OpenGL context is already active when this is created

    // Check if context is active
    if (!glGetString(GL_VERSION))
    {
        std::cerr << "CRITICAL ERROR: No active OpenGL context in GpuMandelbrotCalculator constructor!" << std::endl;
    }

    // OpenGL context verified

    initShaders();
    initGeometry();
    initFBO();
}

GpuMandelbrotCalculator::~GpuMandelbrotCalculator()
{
    if (programId)
        glDeleteProgram(programId);
    if (vao)
        glDeleteVertexArrays(1, &vao);
    if (vbo)
        glDeleteBuffers(1, &vbo);
    if (fbo)
        glDeleteFramebuffers(1, &fbo);
    if (texture)
        glDeleteTextures(1, &texture);
}

void GpuMandelbrotCalculator::compute(std::function<void()> progressCallback)
{
    if (!programId || !fbo)
    {
        std::cerr << "Program or FBO missing." << std::endl;
        return;
    }

    // Bind FBO to render off-screen
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);

    glUseProgram(programId);

    // Update uniforms
    glUniform1d(locMinR, minr);
    glUniform1d(locMinI, mini);
    glUniform1d(locMaxR, maxr);
    glUniform1d(locMaxI, maxi);
    glUniform1i(locMaxIter, MAX_ITER);

    // Draw full screen quad using VAO
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    glUseProgram(0);

    // Force GPU to finish
    glFinish();

    // Read back pixels
    // We read RGBA unsigned bytes
    std::vector<uint8_t> pixels(width * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // Check for GL errors
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR)
    {
        std::cerr << "OpenGL Error during readback: " << err << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Convert pixels to iteration counts
    // Shader encodes: R = low byte, G = high byte
    // The shader already flips Y coordinate: cy = minI + (1.0 - texCoord.y) * (maxI - minI)
    // This means texCoord.y=0 (bottom of GL texture) maps to maxI (bottom of complex plane)
    // and texCoord.y=1 (top of GL texture) maps to minI (top of complex plane)
    // glReadPixels reads from y=0 (bottom) to y=height-1 (top) in GL coordinates
    // We want our data buffer to have y=0 at top (minI), so we flip during readback

    for (int y = 0; y < height; ++y)
    {
        // glReadPixels gives us bottom row first (y=0 in GL = bottom)
        // We want top row first (y=0 in CPU = top = minI)
        // So flip: GL row y -> CPU row (height-1-y)

        const uint8_t *srcRow = &pixels[y * width * 4];
        int *dstRow = &data[(height - 1 - y) * width];

        for (int x = 0; x < width; ++x)
        {
            uint8_t r = srcRow[x * 4 + 0];
            uint8_t g = srcRow[x * 4 + 1];
            // uint8_t b = srcRow[x * 4 + 2];

            // Decode iteration count
            int iter = r + (g * 256);
            if (iter > MAX_ITER)
                iter = MAX_ITER;

            dstRow[x] = iter;
        }
    }

    if (progressCallback)
        progressCallback();
}
void GpuMandelbrotCalculator::reset()
{
    // Nothing to reset for GPU
}

void GpuMandelbrotCalculator::initFBO()
{
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Framebuffer is not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GpuMandelbrotCalculator::initGeometry()
{
    // Full screen quad coordinates (-1 to 1)
    float vertices[] = {
        -1.0f, -1.0f,
        1.0f, -1.0f,
        -1.0f, 1.0f,
        1.0f, 1.0f};

    // Create and bind VAO (required for Core Profile)
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Setup vertex attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void *)0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GpuMandelbrotCalculator::initShaders()
{
    // Configure precision type for shader based on constructor parameter
    // float:  ~45ms on Intel integrated GPU, precision good to zoom ~1e-6
    // double: ~545ms on Intel integrated GPU, precision good to zoom ~1e-15
    const char *precisionType = (precision == Precision::FLOAT) ? "float" : "double";

    // Vertex Shader - GLSL 4.0 Core
    const std::string vsSource = R"(
        #version 400 core
        in vec2 position;
        out vec2 texCoord;
        void main() {
            gl_Position = vec4(position, 0.0, 1.0);
            // Map from [-1, 1] to [0, 1]
            texCoord = position * 0.5 + 0.5;
        }
    )";

    // Fragment Shader - GLSL 4.0 Core with configurable precision
    // Encodes iteration count into RGBA
    // $PRECISION_TYPE will be replaced by C++ code before compilation
    std::string fsSourceTemplate = R"(
        #version 400 core
        
        uniform double minR;
        uniform double minI;
        uniform double maxR;
        uniform double maxI;
        uniform int maxIter;
        
        in vec2 texCoord;
        out vec4 fragColor;
        
        void main() {
            // Map texture coordinate [0,1] to complex plane
            // Convert double uniforms to computation precision
            // Note: texCoord.y=0 is bottom in OpenGL, but we want y=0 to be top (mini)
            // So we flip: use (1.0 - texCoord.y)
            $PRECISION_TYPE x = $PRECISION_TYPE(minR) + $PRECISION_TYPE(texCoord.x) * $PRECISION_TYPE(maxR - minR);
            $PRECISION_TYPE y = $PRECISION_TYPE(minI) + $PRECISION_TYPE(1.0 - texCoord.y) * $PRECISION_TYPE(maxI - minI);
            
            // Start with z = c (matching CPU implementation)
            $PRECISION_TYPE r = x;
            $PRECISION_TYPE i = y;
            $PRECISION_TYPE r2;
            $PRECISION_TYPE i2;
            
            int iter = 0;
            // We can use a dynamic loop in GLSL 4.0
            for (int k = 0; k < maxIter; ++k) {
                r2 = r * r;
                i2 = i * i;
                
                if (r2 + i2 >= $PRECISION_TYPE(4.0)) {
                    iter = k;
                    break;
                }
                
                $PRECISION_TYPE ri = r * i;
                i = ri + ri + y; // z = z^2 + c
                r = r2 - i2 + x;
            }
            
            // If loop completed without breaking, we're in the set
            if (iter == 0 && r2 + i2 < $PRECISION_TYPE(4.0)) {
                iter = maxIter;
            }
            
            // Encode iter into RG channels
            // R = low byte, G = high byte
            
            float rOut = mod(float(iter), 256.0) / 255.0;
            float gOut = floor(float(iter) / 256.0) / 255.0;
            
            fragColor = vec4(rOut, gOut, 0.0, 1.0);
        }
    )";

    // Replace $PRECISION_TYPE with the actual type
    std::string fsSource = fsSourceTemplate;
    size_t pos = 0;
    while ((pos = fsSource.find("$PRECISION_TYPE", pos)) != std::string::npos)
    {
        fsSource.replace(pos, 15, precisionType);
        pos += strlen(precisionType);
    }

    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSource);

    programId = glCreateProgram();
    glAttachShader(programId, vs);
    glAttachShader(programId, fs);

    // Bind attribute location before linking
    glBindAttribLocation(programId, 0, "position");

    glLinkProgram(programId);

    GLint linked;
    glGetProgramiv(programId, GL_LINK_STATUS, &linked);
    if (!linked)
    {
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
    if (!compiled)
    {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        std::cerr << "Shader Compilation Error (" << (type == GL_VERTEX_SHADER ? "VS" : "FS") << "): " << log << std::endl;
    }
    return shader;
}

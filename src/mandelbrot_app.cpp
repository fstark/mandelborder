#include "mandelbrot_app.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <format>
#include <chrono>
#include <chrono>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Define this to get modern OpenGL functions
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL_opengl.h>

MandelbrotApp::MandelbrotApp(int w, int h, bool speed, const std::string &engineType)
    : width(w), height(h), pixelSize(1), window(nullptr), renderer(nullptr), texture(nullptr), glContext(nullptr), ownsGLContext(false),
      autoZoomActive(false), speedMode(speed), verboseMode(false), exitAfterFirstDisplay(false),
      autoScreenshotMode(false), currentEngineType(GridMandelbrotCalculator::EngineType::BORDER)
{
    // Parse engine type
    if (engineType == "border")
    {
        currentEngineType = GridMandelbrotCalculator::EngineType::BORDER;
    }
    else if (engineType == "standard")
    {
        currentEngineType = GridMandelbrotCalculator::EngineType::STANDARD;
    }
    else if (engineType == "simd")
    {
        currentEngineType = GridMandelbrotCalculator::EngineType::SIMD;
    }
    else if (engineType == "gpuf" || engineType == "gpu")
    {
        currentEngineType = GridMandelbrotCalculator::EngineType::GPUF;
    }
    else if (engineType == "gpud")
    {
        currentEngineType = GridMandelbrotCalculator::EngineType::GPUD;
    }
    else
    {
        std::cerr << "Unknown engine type: " << engineType << ", defaulting to BORDER" << std::endl;
        currentEngineType = GridMandelbrotCalculator::EngineType::BORDER;
    }

    calcWidth = width / pixelSize;
    calcHeight = height / pixelSize;

    initSDL();

    // Create renderer first (it will create its own OpenGL context if accelerated)
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        // Fallback to software renderer
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer)
        {
            throw std::runtime_error(std::string("Renderer creation failed: ") + SDL_GetError());
        }
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    // Get the OpenGL context from the renderer (if it created one)
    glContext = SDL_GL_GetCurrentContext();
    if (glContext)
    {
        ownsGLContext = false; // We don't own it, the renderer does
        SDL_GL_SetSwapInterval(0); // Disable VSync for offscreen rendering
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, calcWidth, calcHeight);
    if (!texture)
    {
        throw std::runtime_error(std::string("Texture creation failed: ") + SDL_GetError());
    }

    // Speed mode: 4x4 grid with parallel computation
    // Normal mode: 1x1 grid (effectively single calculator) with progressive rendering
    if (currentEngineType == GridMandelbrotCalculator::EngineType::GPUF ||
        currentEngineType == GridMandelbrotCalculator::EngineType::GPUD)
    {
        // GPU always uses 1x1 grid
        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 1, 1);
        gridCalc->setSpeedMode(speedMode);
        gridCalc->setEngineType(currentEngineType);
        calculator = std::move(gridCalc);
    }
    else if (speedMode)
    {
        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 4, 4);
        gridCalc->setSpeedMode(true);
        gridCalc->setEngineType(currentEngineType);
        calculator = std::move(gridCalc);
    }
    else
    {
        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 1, 1);
        gridCalc->setSpeedMode(false);
        gridCalc->setEngineType(currentEngineType);
        calculator = std::move(gridCalc);
    }

    zoomChooser = std::make_unique<ZoomPointChooser>(calcWidth, calcHeight);

    // Initialize random seed first
    srand(static_cast<unsigned>(time(nullptr)));

    // Initialize gradient with default polynomial (non-swapped)
    // Use fixed polynomial: r(t) = 9*(1-t)*t³*255, g(t) = 15*(1-t)²*t²*255, b(t) = 8.5*(1-t)³*t*255
    gradient = std::make_unique<PolynomialGradient>(9.0, 15.0, 8.5);
}

MandelbrotApp::~MandelbrotApp()
{
    if (glContext && ownsGLContext)
        SDL_GL_DeleteContext(glContext);
    if (texture)
        SDL_DestroyTexture(texture);
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
    SDL_Quit();
}

void MandelbrotApp::initSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        throw std::runtime_error(std::string("SDL initialization failed: ") + SDL_GetError());
    }

    // Set OpenGL attributes before creating window
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    window = SDL_CreateWindow(
        "Mandelbrot Set - Boundary Tracing",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);

    if (!window)
    {
        SDL_Quit();
        throw std::runtime_error(std::string("Window creation failed: ") + SDL_GetError());
    }

    // We defer renderer/context creation to switchTo... methods
}

// Removed switchToOpenGL and switchToSDLRenderer as we now use a unified approach

void MandelbrotApp::compute()
{
    // For GPU mode, ensure OpenGL context is current
    if ((currentEngineType == GridMandelbrotCalculator::EngineType::GPUF ||
         currentEngineType == GridMandelbrotCalculator::EngineType::GPUD) &&
        glContext)
    {
        SDL_GL_MakeCurrent(window, glContext);
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    calculator->compute([this]()
                        {
                            this->render();
                        });

    if (verboseMode)
    {
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        double milliseconds = duration.count() / 1000.0;

        std::string engineName = calculator->getEngineName();
        
        std::cout << std::format("{} {:>4}x{:<4} {:>8.1f} ms  {:>20.16f} {:>20.16f} {:>12.2e}\n",
                                 engineName,
                                 calculator->getWidth(), calculator->getHeight(),
                                 milliseconds,
                                 calculator->getCre(), calculator->getCim(), calculator->getDiam());
    }
}

void MandelbrotApp::render()
{
    // Ensure GL context is active for GPU computation if needed
    // But render() is for display. compute() is where the work happens.
    // Since we read back data, we always use the CPU render path (SDL Renderer)

    if (!renderer)
        return;

    Uint32 *pixels;
    int pitch;

    SDL_LockTexture(texture, nullptr, (void **)&pixels, &pitch);

    const auto &data = calculator->getData();

    for (int y = 0; y < calcHeight; ++y)
    {
        for (int x = 0; x < calcWidth; ++x)
        {
            int p = y * calcWidth + x;
            int iter = data[p];

            if (iter == MandelbrotCalculator::MAX_ITER)
            {
                pixels[y * (pitch / 4) + x] = 0xFF000000; // Black (Alpha=255)
            }
            else
            {
                double t = static_cast<double>(iter) / MandelbrotCalculator::MAX_ITER;
                SDL_Color color = gradient->getColor(t);

                if (iter % 2 != 0)
                {
                    // Shift value (brightness) for odd iterations
                    const int shift = 34;
                    color.r = std::min(255, color.r + shift);
                    color.g = std::min(255, color.g + shift);
                    color.b = std::min(255, color.b + shift);
                }
                // ARGB8888: A R G B
                pixels[y * (pitch / 4) + x] = (0xFF << 24) | (color.r << 16) | (color.g << 8) | color.b;
            }
        }
    }

    SDL_UnlockTexture(texture);

    if (SDL_RenderClear(renderer) < 0)
        std::cerr << "RenderClear failed: " << SDL_GetError() << std::endl;
    if (SDL_RenderCopy(renderer, texture, nullptr, nullptr) < 0)
        std::cerr << "RenderCopy failed: " << SDL_GetError() << std::endl;
    SDL_RenderPresent(renderer);
    
    // Auto-screenshot if mode is enabled
    if (autoScreenshotMode)
    {
        saveScreenshot("mandelbrot");
    }
}

SDL_Rect MandelbrotApp::calculateSelectionRect(int startX, int startY, int endX, int endY, bool centerBased)
{
    int dx = endX - startX;
    int dy = endY - startY;
    double aspectRatio = (double)width / height;
    int w, h, rectX, rectY;

    // Calculate raw dimensions
    if (centerBased)
    {
        w = std::abs(dx) * 2;
        h = std::abs(dy) * 2;
    }
    else
    {
        w = std::abs(dx);
        h = std::abs(dy);
    }

    // Apply aspect ratio correction
    if (w / aspectRatio > h)
        h = (int)(w / aspectRatio);
    else
        w = (int)(h * aspectRatio);

    // Calculate top-left position
    if (centerBased)
    {
        rectX = startX - w / 2;
        rectY = startY - h / 2;
    }
    else
    {
        // Keep startX, startY as the anchor point
        // Extend the rectangle in the direction of the drag
        if (dx >= 0)
            rectX = startX; // Dragging right: anchor is left edge
        else
            rectX = startX - w; // Dragging left: anchor is right edge

        if (dy >= 0)
            rectY = startY; // Dragging down: anchor is top edge
        else
            rectY = startY - h; // Dragging up: anchor is bottom edge
    }

    return {rectX, rectY, w, h};
}

void MandelbrotApp::resetZoom()
{
    calculator->updateBounds(-0.5, 0.0, 3.0);
}

bool MandelbrotApp::isZoomDisabled() const
{
    return calculator->getDiam() < 1e-15;
}

void MandelbrotApp::setPixelSize(int newSize)
{
    if (pixelSize == newSize)
        return;

    // Save current view parameters
    double currentCre = calculator->getCre();
    double currentCim = calculator->getCim();
    double currentDiam = calculator->getDiam();

    pixelSize = newSize;
    calcWidth = width / pixelSize;
    calcHeight = height / pixelSize;

    // Recreate calculator with appropriate grid size based on speed mode
    if (currentEngineType == GridMandelbrotCalculator::EngineType::GPUF ||
        currentEngineType == GridMandelbrotCalculator::EngineType::GPUD)
    {
        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 1, 1);
        gridCalc->setSpeedMode(speedMode);
        gridCalc->setEngineType(currentEngineType);
        calculator = std::move(gridCalc);
    }
    else if (speedMode)
    {
        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 4, 4);
        gridCalc->setSpeedMode(true);
        gridCalc->setEngineType(currentEngineType);
        calculator = std::move(gridCalc);
    }
    else
    {
        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 1, 1);
        gridCalc->setSpeedMode(false);
        gridCalc->setEngineType(currentEngineType);
        calculator = std::move(gridCalc);
    }
    calculator->updateBounds(currentCre, currentCim, currentDiam);

    zoomChooser = std::make_unique<ZoomPointChooser>(calcWidth, calcHeight);

    // Recreate texture
    if (texture)
        SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, calcWidth, calcHeight);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);

    if (!texture)
    {
        throw std::runtime_error(std::string("Texture creation failed: ") + SDL_GetError());
    }

    // Recompute
    compute();
    render();

    // Pixel size updated
}

void MandelbrotApp::handleResize(int newWidth, int newHeight)
{
    if (newWidth == width && newHeight == height)
        return;

    // Save current view parameters
    double currentCre = calculator->getCre();
    double currentCim = calculator->getCim();
    double currentDiam = calculator->getDiam();

    // Update dimensions
    width = newWidth;
    height = newHeight;
    calcWidth = width / pixelSize;
    calcHeight = height / pixelSize;

    // Recreate calculator with appropriate grid size based on speed mode
    if (currentEngineType == GridMandelbrotCalculator::EngineType::GPUF ||
        currentEngineType == GridMandelbrotCalculator::EngineType::GPUD)
    {
        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 1, 1);
        gridCalc->setSpeedMode(speedMode);
        gridCalc->setEngineType(currentEngineType);
        calculator = std::move(gridCalc);
    }
    else if (speedMode)
    {
        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 4, 4);
        gridCalc->setSpeedMode(true);
        gridCalc->setEngineType(currentEngineType);
        calculator = std::move(gridCalc);
    }
    else
    {
        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 1, 1);
        gridCalc->setSpeedMode(false);
        gridCalc->setEngineType(currentEngineType);
        calculator = std::move(gridCalc);
    }
    calculator->updateBounds(currentCre, currentCim, currentDiam);

    // Recreate zoom chooser
    zoomChooser = std::make_unique<ZoomPointChooser>(calcWidth, calcHeight);

    // Recreate texture with new dimensions
    if (texture)
        SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING, calcWidth, calcHeight);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);

    if (!texture)
    {
        throw std::runtime_error(std::string("Texture creation failed: ") + SDL_GetError());
    }

    // Recompute with new dimensions
    compute();
    render();

    // Window resized
}

void MandelbrotApp::zoomToRegion(int x1, int y1, int x2, int y2)
{
    if (x1 == x2 || y1 == y2)
        return;

    // Ensure x1,y1 is top-left
    if (x1 > x2)
        std::swap(x1, x2);
    if (y1 > y2)
        std::swap(y1, y2);

    // Convert pixel coordinates to complex plane
    double minr = calculator->getMinR();
    double mini = calculator->getMinI();
    double stepr = calculator->getStepR();
    double stepi = calculator->getStepI();

    // Adjust for resolution difference between window and calculation
    double re1 = minr + (x1 / (double)width) * (stepr * calcWidth);
    double im1 = mini + (y1 / (double)height) * (stepi * calcHeight);
    double re2 = minr + (x2 / (double)width) * (stepr * calcWidth);
    double im2 = mini + (y2 / (double)height) * (stepi * calcHeight);

    double new_cre = (re1 + re2) / 2.0;
    double new_cim = (im1 + im2) / 2.0;
    double new_diam = std::max(re2 - re1, im2 - im1);

    calculator->updateBounds(new_cre, new_cim, new_diam);
}void MandelbrotApp::animateRectToRect(int startX, int startY, int startWidth, int startHeight,
                                      int endX, int endY, int endWidth, int endHeight,
                                      int steps, int frameDelay)
{
    // Skip animation in speed mode
    if (calculator->getSpeedMode())
        return;

    // Animate rectangle transformation over specified number of steps
    for (int step = 0; step <= steps; ++step)
    {
        double t = (double)step / steps;

        // Interpolate rectangle dimensions
        int currentX = (int)(startX + (endX - startX) * t);
        int currentY = (int)(startY + (endY - startY) * t);
        int currentWidth = (int)(startWidth + (endWidth - startWidth) * t);
        int currentHeight = (int)(startHeight + (endHeight - startHeight) * t);

        // Redraw the current frame
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect rect = {currentX, currentY, currentWidth, currentHeight};
        SDL_RenderDrawRect(renderer, &rect);
        SDL_RenderPresent(renderer);

        SDL_Delay(frameDelay);
    }
}

void MandelbrotApp::blinkRect(int x, int y, int w, int h, int times, int blinkDelay)
{
    // Skip blinking in speed mode
    if (calculator->getSpeedMode())
        return;

    for (int i = 0; i < times; ++i)
    {
        // Draw with rectangle
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect rect = {x, y, w, h};
        SDL_RenderDrawRect(renderer, &rect);
        SDL_RenderPresent(renderer);
        SDL_Delay(blinkDelay);

        // Draw without rectangle
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
        SDL_Delay(blinkDelay);
    }
}

void MandelbrotApp::zoomToRect(int x1, int y1, int x2, int y2, bool inverse)
{
    // Disable zoom-in when diameter is too small
    if (!inverse && isZoomDisabled())
    {
        return;
    }

    if (inverse)
    {
        // Zoom OUT: animate full screen shrinking to rectangle
        animateRectToRect(0, 0, width, height, x1, y1, x2 - x1, y2 - y1);

        // Calculate scale and new center
        double scale = std::max((double)width / (x2 - x1), (double)height / (y2 - y1));
        int offsetX = (x1 + x2) / 2 - width / 2;
        int offsetY = (y1 + y2) / 2 - height / 2;

        // Adjust step size for resolution difference
        double effectiveStepR = calculator->getStepR() * ((double)calcWidth / width);
        double effectiveStepI = calculator->getStepI() * ((double)calcHeight / height);

        double new_cre = calculator->getCre() + offsetX * effectiveStepR * scale;
        double new_cim = calculator->getCim() + offsetY * effectiveStepI * scale;
        double new_diam = calculator->getDiam() * scale;
        calculator->updateBounds(new_cre, new_cim, new_diam);
    }
    else
    {
        // Zoom IN: animate rectangle expanding to full screen
        animateRectToRect(x1, y1, x2 - x1, y2 - y1, 0, 0, width, height);
        zoomToRegion(x1, y1, x2, y2);
    }

    calculator->reset();
    compute();
    render();
}

void MandelbrotApp::run()
{
    if (!exitAfterFirstDisplay)
    {
        std::cout << "Keyboard controls:" << std::endl;
    std::cout << "  ESC      - Quit (or cancel drag)" << std::endl;
    std::cout << "  SPACE    - Recompute" << std::endl;
    std::cout << "  R        - Reset zoom to full set" << std::endl;
    std::cout << "  F        - Toggle fast mode (parallel computation)" << std::endl;
    std::cout << "  S        - Save screenshot" << std::endl;
    std::cout << "  Shift+S  - Toggle auto-screenshot mode" << std::endl;
    std::cout << "  E        - Cycle engine (Border→Standard→SIMD→GPU-Float→GPU-Double)" << std::endl;
    std::cout << "  P        - Random palette" << std::endl;
    std::cout << "  V        - Toggle verbose mode" << std::endl;
    std::cout << "  A        - Toggle auto-zoom" << std::endl;
    std::cout << "  X        - Toggle pixel size (1x or 10x)" << std::endl;
    std::cout << "\nMouse controls:" << std::endl;
    std::cout << "  Drag     - Zoom into region" << std::endl;
    std::cout << "  Shift+Drag - Zoom out from region" << std::endl;
    std::cout << "  Ctrl+Drag  - Center-based zoom" << std::endl;
    }

    compute();
    render();

    if (exitAfterFirstDisplay)
    {
        std::cout << "Exiting after first display as requested" << std::endl;
        return;
    }

    bool running = true;
    SDL_Event event;

    bool dragging = false;
    int dragStartX = 0, dragStartY = 0;
    int dragEndX = 0, dragEndY = 0;
    int currentMouseX = 0, currentMouseY = 0;

    while (running)
    {
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    handleResize(event.window.data1, event.window.data2);
                }
                break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
                {
                    if (dragging)
                    {
                        // Cancel drag operation
                        dragging = false;
                        render(); // Redraw without selection rectangle
                    }
                    else
                    {
                        running = false;
                    }
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE)
                {
                    calculator->reset();
                    compute();
                    render();
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r)
                {
                    resetZoom();
                    calculator->reset();
                    compute();
                    render();
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_s)
                {
                    // Check for Shift modifier
                    SDL_Keymod modState = SDL_GetModState();
                    if (modState & KMOD_SHIFT)
                    {
                        // Toggle auto-screenshot mode (Shift+S)
                    {
                        autoScreenshotMode = !autoScreenshotMode;
                    }                        // If we just enabled auto-screenshot, capture the current frame immediately
                        if (autoScreenshotMode)
                        {
                            saveScreenshot("mandelbrot");
                        }
                    }
                    else
                    {
                        // Save single screenshot (S)
                        saveScreenshot("mandelbrot");
                    }
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_f)
                {
                    // Toggle fast/speed mode
                    speedMode = !speedMode;

                    // Save current view parameters
                    double currentCre = calculator->getCre();
                    double currentCim = calculator->getCim();
                    double currentDiam = calculator->getDiam();

                    // Recreate calculator with appropriate grid size
                    if (currentEngineType == GridMandelbrotCalculator::EngineType::GPUF ||
                        currentEngineType == GridMandelbrotCalculator::EngineType::GPUD)
                    {
                        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 1, 1);
                        gridCalc->setSpeedMode(speedMode);
                        gridCalc->setEngineType(currentEngineType);
                        calculator = std::move(gridCalc);
                        std::cout << "Speed mode: " << (speedMode ? "ON" : "OFF") << " (GPU 1x1)" << std::endl;
                    }
                    else if (speedMode)
                    {
                        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 4, 4);
                        gridCalc->setSpeedMode(true);
                        gridCalc->setEngineType(currentEngineType);
                        calculator = std::move(gridCalc);
                    }
                    else
                    {
                        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 1, 1);
                        gridCalc->setSpeedMode(false);
                        gridCalc->setEngineType(currentEngineType);
                        calculator = std::move(gridCalc);
                    }
                    calculator->updateBounds(currentCre, currentCim, currentDiam);

                    // Recompute with new calculator
                    compute();
                    render();
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_e)
                {
                    // Toggle engine type
                    if (currentEngineType == GridMandelbrotCalculator::EngineType::BORDER)
                    {
                        currentEngineType = GridMandelbrotCalculator::EngineType::STANDARD;
                    }
                    else if (currentEngineType == GridMandelbrotCalculator::EngineType::STANDARD)
                    {
                        currentEngineType = GridMandelbrotCalculator::EngineType::SIMD;
                    }
                    else if (currentEngineType == GridMandelbrotCalculator::EngineType::SIMD)
                    {
                        currentEngineType = GridMandelbrotCalculator::EngineType::GPUF;
                    }
                    else if (currentEngineType == GridMandelbrotCalculator::EngineType::GPUF)
                    {
                        currentEngineType = GridMandelbrotCalculator::EngineType::GPUD;
                    }
                    else
                    {
                        currentEngineType = GridMandelbrotCalculator::EngineType::BORDER;
                    }

                    // Save current view parameters
                    double currentCre = calculator->getCre();
                    double currentCim = calculator->getCim();
                    double currentDiam = calculator->getDiam();

                    // Recreate calculator based on engine type
                    if (currentEngineType == GridMandelbrotCalculator::EngineType::GPUF ||
                        currentEngineType == GridMandelbrotCalculator::EngineType::GPUD)
                    {
                        // GPU mode always uses 1x1 grid
                        // OpenGL context is already created at startup
                        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 1, 1);
                        gridCalc->setSpeedMode(speedMode);
                        gridCalc->setEngineType(currentEngineType);
                        calculator = std::move(gridCalc);
                    }
                    else
                    {
                        // Restore grid size based on speed mode
                        int rows = speedMode ? 4 : 1;
                        int cols = speedMode ? 4 : 1;
                        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, rows, cols);
                        gridCalc->setSpeedMode(speedMode);
                        gridCalc->setEngineType(currentEngineType);
                        calculator = std::move(gridCalc);
                    }

                    calculator->updateBounds(currentCre, currentCim, currentDiam);
                    compute();
                    render();
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_p)
                {
                    gradient = Gradient::createRandom();
                    render();
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_v)
                {
                    verboseMode = !verboseMode;
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_a)
                {
                    autoZoomActive = !autoZoomActive;
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_x)
                {
                    int newSize = (pixelSize == 1) ? 10 : 1;
                    setPixelSize(newSize);
                }
                else if (dragging && (event.key.keysym.sym == SDLK_LCTRL ||
                                      event.key.keysym.sym == SDLK_RCTRL))
                {
                    // CTRL pressed/released during drag - redraw rectangle immediately
                    SDL_RenderClear(renderer);
                    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

                    SDL_Keymod modState = SDL_GetModState();
                    bool ctrlPressed = (modState & KMOD_CTRL) != 0;

                    SDL_Rect rect = calculateSelectionRect(dragStartX, dragStartY, currentMouseX, currentMouseY, ctrlPressed);
                    SDL_RenderDrawRect(renderer, &rect);
                    SDL_RenderPresent(renderer);
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    dragging = true;
                    dragStartX = event.button.x;
                    dragStartY = event.button.y;
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT && dragging)
                {
                    dragging = false;
                    dragEndX = event.button.x;
                    dragEndY = event.button.y;

                    // Check if SHIFT is pressed for zoom out (inverse)
                    SDL_Keymod modState = SDL_GetModState();
                    bool shiftPressed = (modState & KMOD_SHIFT) != 0;

                    // Skip zoom-in if disabled (allow zoom-out)
                    if (!shiftPressed && isZoomDisabled())
                    {
                        std::cout << "Zoom disabled: diameter too small (" << calculator->getDiam() << ")" << std::endl;
                        break;
                    }

                    int x1, y1, x2, y2;

                    // Check if this was a click (no drag) vs a drag
                    int dx = dragEndX - dragStartX;
                    int dy = dragEndY - dragStartY;
                    int dragDistance = std::abs(dx) + std::abs(dy);

                    if (dragDistance < 5)
                    {
                        // Click with no drag - zoom 2x centered on click point
                        int w = width / 2;
                        int h = height / 2;

                        x1 = dragStartX - w / 2;
                        y1 = dragStartY - h / 2;
                        x2 = x1 + w;
                        y2 = y1 + h;
                    }
                    else
                    {
                        // Check if CTRL is pressed for center-based zoom
                        bool ctrlPressed = (modState & KMOD_CTRL) != 0;

                        SDL_Rect rect = calculateSelectionRect(dragStartX, dragStartY, dragEndX, dragEndY, ctrlPressed);
                        x1 = rect.x;
                        y1 = rect.y;
                        x2 = x1 + rect.w;
                        y2 = y1 + rect.h;
                    }

                    zoomToRect(x1, y1, x2, y2, shiftPressed);
                }
                break;

            case SDL_MOUSEMOTION:
                if (dragging)
                {
                    // Track current mouse position
                    currentMouseX = event.motion.x;
                    currentMouseY = event.motion.y;

                    // Draw selection rectangle with correct aspect ratio
                    SDL_RenderClear(renderer);
                    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

                    // Check if CTRL is pressed for center-based zoom
                    SDL_Keymod modState = SDL_GetModState();
                    bool ctrlPressed = (modState & KMOD_CTRL) != 0;

                    SDL_Rect rect = calculateSelectionRect(dragStartX, dragStartY, currentMouseX, currentMouseY, ctrlPressed);
                    SDL_RenderDrawRect(renderer, &rect);
                    SDL_RenderPresent(renderer);
                }
                break;
            }
        }

        // Auto-zoom functionality
        if (autoZoomActive)
        {
            // Check if zoom is disabled, reset to home if so
            if (isZoomDisabled())
            {
                gradient = Gradient::createRandom();
                resetZoom();
                calculator->reset();
                compute();
                render();
            }
            else
            {
                // Calculate zoom rectangle dimensions in calculation coordinates
                int calcRectW = calcWidth / 4;
                int calcRectH = calcHeight / 4;

                // Find an interesting point to zoom to (in calculation coordinates)
                int calcCenterX, calcCenterY;
                zoomChooser->findInterestingPoint(calculator->getData(),
                                                  MandelbrotCalculator::MAX_ITER,
                                                  calcCenterX, calcCenterY,
                                                  calcRectW, calcRectH);

                // Convert back to window coordinates for display and zooming
                int rectW = calcRectW * pixelSize;
                int rectH = calcRectH * pixelSize;
                int centerX = calcCenterX * pixelSize;
                int centerY = calcCenterY * pixelSize;

                int x1 = centerX - rectW / 2;
                int y1 = centerY - rectH / 2;
                int x2 = x1 + rectW;
                int y2 = y1 + rectH;

                // Blink the rectangle 3 times before zooming
                blinkRect(x1, y1, rectW, rectH, 3, 150);

                // Perform the zoom
                zoomToRect(x1, y1, x2, y2, false);
            }
        }

        SDL_Delay(16); // ~60 FPS
    }
}

void MandelbrotApp::setExitAfterFirstDisplay(bool exit)
{
    exitAfterFirstDisplay = exit;
}

std::string MandelbrotApp::generateUniqueFilename(const std::string& basename, const std::string& extension)
{
    // Generate timestamped base filename
    time_t now = time(nullptr);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", localtime(&now));
    
    std::string baseFilename = basename + "_" + timestamp;
    
    // Always start with -000 suffix to ensure proper sorting
    // Try suffixes -000 to -999
    for (int i = 0; i <= 999; ++i)
    {
        std::ostringstream oss;
        oss << baseFilename << "-" << std::setfill('0') << std::setw(3) << i << extension;
        std::string filename = oss.str();
        
        std::ifstream testFile(filename);
        if (!testFile.good())
        {
            // File doesn't exist, we can use this name
            return filename;
        }
        testFile.close();
    }
    
    // All 1000 variations (-000 to -999) exist, throw error
    throw std::runtime_error("Cannot generate unique filename: too many files with timestamp " + 
                           std::string(timestamp));
}

void MandelbrotApp::saveScreenshot(const std::string& basename)
{
    // Generate unique filename with timestamp
    std::string filename;
    try
    {
        filename = generateUniqueFilename(basename, ".png");
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "Failed to generate unique filename: " << e.what() << std::endl;
        return;
    }
    
    // Lock texture and read pixels
    void* pixels;
    int pitch;
    if (SDL_LockTexture(texture, nullptr, &pixels, &pitch) != 0)
    {
        std::cerr << "Failed to lock texture: " << SDL_GetError() << std::endl;
        return;
    }
    
    // SDL_PIXELFORMAT_RGB888 is 4 bytes per pixel, but we need to convert to RGB (3 bytes)
    // Allocate buffer for RGB data (3 bytes per pixel)
    std::vector<unsigned char> rgbPixels(calcWidth * calcHeight * 3);
    
    // Convert from pixel format to RGB (3 bytes)
    // The texture stores pixels as 32-bit integers: (R << 16) | (G << 8) | B
    // On little-endian systems, bytes in memory are: B G R 0
    unsigned char* src = static_cast<unsigned char*>(pixels);
    unsigned char* dst = rgbPixels.data();
    
    for (int y = 0; y < calcHeight; ++y)
    {
        unsigned char* rowSrc = src + y * pitch;
        for (int x = 0; x < calcWidth; ++x)
        {
            // On little-endian: bytes are B, G, R, 0
            // We need R, G, B for PNG
            *dst++ = rowSrc[2]; // R (third byte)
            *dst++ = rowSrc[1]; // G (second byte)
            *dst++ = rowSrc[0]; // B (first byte)
            rowSrc += 4; // Move to next pixel (4 bytes per pixel in source)
        }
    }
    
    SDL_UnlockTexture(texture);
    
    // Save using stb_image_write (RGB format, 3 bytes per pixel)
    // Pitch for RGB is simply width * 3
    int result = stbi_write_png(filename.c_str(), calcWidth, calcHeight, 3, 
                                 rgbPixels.data(), calcWidth * 3);
    
    if (result)
    {
        std::cout << "Screenshot saved: " << filename << std::endl;
    }
    else
    {
        std::cerr << "Failed to save screenshot: " << filename << std::endl;
    }
}

void MandelbrotApp::setVerboseMode(bool verbose)
{
    verboseMode = verbose;
    if (calculator)
    {
    }
}

void MandelbrotApp::setAutoZoom(bool enabled)
{
    autoZoomActive = enabled;
}

void MandelbrotApp::setRandomPalette()
{
    gradient = Gradient::createRandom();
}

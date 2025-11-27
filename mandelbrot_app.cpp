#include "mandelbrot_app.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <ctime>

MandelbrotApp::MandelbrotApp(int w, int h, bool speed)
    : width(w), height(h), pixelSize(1), window(nullptr), renderer(nullptr), texture(nullptr), 
      autoZoomActive(false), speedMode(speed), verboseMode(false), exitAfterFirstDisplay(false),
      currentEngineType(GridMandelbrotCalculator::EngineType::BORDER)
{
    calcWidth = width / pixelSize;
    calcHeight = height / pixelSize;

    // Speed mode: 4x4 grid with parallel computation
    // Normal mode: 1x1 grid (effectively single calculator) with progressive rendering
    if (speedMode)
    {
        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 4, 4);
        gridCalc->setSpeedMode(true);
        gridCalc->setVerboseMode(verboseMode);
        gridCalc->setEngineType(currentEngineType);
        calculator = std::move(gridCalc);
    }
    else
    {
        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 1, 1);
        gridCalc->setSpeedMode(false);
        gridCalc->setVerboseMode(verboseMode);
        gridCalc->setEngineType(currentEngineType);
        calculator = std::move(gridCalc);
    }

    zoomChooser = std::make_unique<ZoomPointChooser>(calcWidth, calcHeight);

    // Initialize random seed first
    srand(static_cast<unsigned>(time(nullptr)));

    // Initialize gradient with random parameters
    gradient = Gradient::createRandom();

    initSDL();
}

MandelbrotApp::~MandelbrotApp()
{
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

    window = SDL_CreateWindow(
        "Mandelbrot Set - Boundary Tracing",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!window)
    {
        SDL_Quit();
        throw std::runtime_error(std::string("Window creation failed: ") + SDL_GetError());
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        SDL_DestroyWindow(window);
        SDL_Quit();
        throw std::runtime_error(std::string("Renderer creation failed: ") + SDL_GetError());
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); // Nearest neighbor scaling

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
                                SDL_TEXTUREACCESS_STREAMING, calcWidth, calcHeight);
    if (!texture)
    {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        throw std::runtime_error(std::string("Texture creation failed: ") + SDL_GetError());
    }
}

void MandelbrotApp::render()
{
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
                pixels[y * (pitch / 4) + x] = 0; // Black
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
                pixels[y * (pitch / 4) + x] = (color.r << 16) | (color.g << 8) | color.b;
            }
        }
    }

    SDL_UnlockTexture(texture);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
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
    if (speedMode)
    {
        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 4, 4);
        gridCalc->setSpeedMode(true);
        gridCalc->setVerboseMode(verboseMode);
        gridCalc->setEngineType(currentEngineType);
        calculator = std::move(gridCalc);
    }
    else
    {
        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 1, 1);
        gridCalc->setSpeedMode(false);
        gridCalc->setVerboseMode(verboseMode);
        gridCalc->setEngineType(currentEngineType);
        calculator = std::move(gridCalc);
    }
    calculator->updateBounds(currentCre, currentCim, currentDiam);

    zoomChooser = std::make_unique<ZoomPointChooser>(calcWidth, calcHeight);

    // Recreate texture
    if (texture)
        SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
                                SDL_TEXTUREACCESS_STREAMING, calcWidth, calcHeight);

    if (!texture)
    {
        throw std::runtime_error(std::string("Texture creation failed: ") + SDL_GetError());
    }

    // Recompute
    calculator->compute([this]()
                        { this->render(); });
    render();

    std::cout << "Pixel size set to: " << pixelSize << " (" << calcWidth << "x" << calcHeight << ")" << std::endl;
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
    if (speedMode)
    {
        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 4, 4);
        gridCalc->setSpeedMode(true);
        gridCalc->setVerboseMode(verboseMode);
        gridCalc->setEngineType(currentEngineType);
        calculator = std::move(gridCalc);
    }
    else
    {
        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 1, 1);
        gridCalc->setSpeedMode(false);
        gridCalc->setVerboseMode(verboseMode);
        gridCalc->setEngineType(currentEngineType);
        calculator = std::move(gridCalc);
    }
    calculator->updateBounds(currentCre, currentCim, currentDiam);

    // Recreate zoom chooser
    zoomChooser = std::make_unique<ZoomPointChooser>(calcWidth, calcHeight);

    // Recreate texture with new dimensions
    if (texture)
        SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
                                SDL_TEXTUREACCESS_STREAMING, calcWidth, calcHeight);

    if (!texture)
    {
        throw std::runtime_error(std::string("Texture creation failed: ") + SDL_GetError());
    }

    // Recompute with new dimensions
    calculator->compute([this]()
                        { this->render(); });
    render();

    std::cout << "Window resized to: " << width << "x" << height
              << " (calc: " << calcWidth << "x" << calcHeight << ")" << std::endl;
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

    if (verboseMode)
    {
        std::cout << "Zoomed to: center=(" << std::scientific << std::setprecision(10)
                  << calculator->getCre() << ", " << calculator->getCim()
                  << "), diameter=" << calculator->getDiam() << std::defaultfloat << std::endl;
    }
}

void MandelbrotApp::animateRectToRect(int startX, int startY, int startWidth, int startHeight,
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
        std::cout << "Zoom disabled: diameter too small (" << calculator->getDiam() << ")" << std::endl;
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
    calculator->compute([this]()
                        { this->render(); });
    render();
}

void MandelbrotApp::run()
{
    calculator->compute([this]()
                        { this->render(); });
    render();

    if (exitAfterFirstDisplay)
    {
        std::cout << "Exiting after first display as requested" << std::endl;
        return;
    }

    std::cout << "Press ESC to quit, SPACE to recompute, R to reset zoom, S to toggle speed mode, A for auto-zoom" << std::endl;
    std::cout << "Press E to switch engine (Border/Standard/SIMD), P to switch to a random palette, V to toggle verbose mode" << std::endl;
    std::cout << "Click and drag to zoom into a region (SHIFT to zoom out, CTRL for center-based)" << std::endl;

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
                    calculator->compute([this]()
                                        { this->render(); });
                    render();
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r)
                {
                    resetZoom();
                    calculator->reset();
                    calculator->compute([this]()
                                        { this->render(); });
                    render();
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_h)
                {
                    resetZoom();
                    calculator->reset();
                    calculator->compute([this]
                                        { this->render(); });
                    render();
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_s)
                {
                    // Toggle speed mode
                    speedMode = !speedMode;
                    
                    // Save current view parameters
                    double currentCre = calculator->getCre();
                    double currentCim = calculator->getCim();
                    double currentDiam = calculator->getDiam();
                    
                    // Recreate calculator with appropriate grid size
                    if (speedMode)
                    {
                        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 4, 4);
                        gridCalc->setSpeedMode(true);
                        gridCalc->setVerboseMode(verboseMode);
                        gridCalc->setEngineType(currentEngineType);
                        calculator = std::move(gridCalc);
                        std::cout << "Speed mode: ON (parallel 4x4 grid)" << std::endl;
                    }
                    else
                    {
                        auto gridCalc = std::make_unique<GridMandelbrotCalculator>(calcWidth, calcHeight, 1, 1);
                        gridCalc->setSpeedMode(false);
                        gridCalc->setVerboseMode(verboseMode);
                        gridCalc->setEngineType(currentEngineType);
                        calculator = std::move(gridCalc);
                        std::cout << "Speed mode: OFF (progressive 1x1)" << std::endl;
                    }
                    calculator->updateBounds(currentCre, currentCim, currentDiam);
                    
                    // Recompute with new calculator
                    calculator->compute([this]()
                                        { this->render(); });
                    render();
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_e)
                {
                    // Toggle engine type
                    if (currentEngineType == GridMandelbrotCalculator::EngineType::BORDER)
                    {
                        currentEngineType = GridMandelbrotCalculator::EngineType::STANDARD;
                        std::cout << "Switched to STANDARD engine" << std::endl;
                    }
                    else if (currentEngineType == GridMandelbrotCalculator::EngineType::STANDARD)
                    {
                        currentEngineType = GridMandelbrotCalculator::EngineType::SIMD;
                        std::cout << "Switched to SIMD engine" << std::endl;
                    }
                    else
                    {
                        currentEngineType = GridMandelbrotCalculator::EngineType::BORDER;
                        std::cout << "Switched to BORDER engine" << std::endl;
                    }

                    // Update current calculator
                    // We know calculator is always a GridMandelbrotCalculator in this app
                    GridMandelbrotCalculator* gridCalc = dynamic_cast<GridMandelbrotCalculator*>(calculator.get());
                    if (gridCalc)
                    {
                        gridCalc->setEngineType(currentEngineType);
                        calculator->compute([this]()
                                            { this->render(); });
                        render();
                    }
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_p)
                {
                    gradient = Gradient::createRandom();
                    render();
                    std::cout << "Switched to new random palette" << std::endl;
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_v)
                {
                    verboseMode = !verboseMode;
                    std::cout << "Verbose mode: " << (verboseMode ? "ON" : "OFF") << std::endl;
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_a)
                {
                    autoZoomActive = !autoZoomActive;
                    std::cout << "Auto-zoom: " << (autoZoomActive ? "ON" : "OFF") << std::endl;
                }
                else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_f)
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
                std::cout << "Auto-zoom: diameter limit reached, resetting to home" << std::endl;
                gradient = Gradient::createRandom();
                std::cout << "Switched to new random palette" << std::endl;
                resetZoom();
                calculator->reset();
                calculator->compute([this]()
                                    { this->render(); });
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

#pragma once

#include <SDL2/SDL.h>
#include <vector>
#include <memory>
#include <string>
#include "mandelbrot_calculator.h"
#include "grid_mandelbrot_calculator.h"
#include "zoom_point_chooser.h"
#include "gradient.h"

class MandelbrotApp
{
public:
    MandelbrotApp(int width, int height, bool speedMode = false, const std::string& engineType = "gpu");
    ~MandelbrotApp();

    void run();
    void setExitAfterFirstDisplay(bool exit);
    void setVerboseMode(bool verbose);
    void setAutoZoom(bool enabled);
    void setRandomPalette();
    void setPixelSize(int size);

private:
    int width;
    int height;
    int calcWidth;
    int calcHeight;
    int pixelSize;

    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_GLContext glContext; // OpenGL context for GPU rendering
    bool ownsGLContext;      // Whether we own the context and should delete it

    std::unique_ptr<MandelbrotCalculator> calculator;
    std::unique_ptr<ZoomPointChooser> zoomChooser;
    std::unique_ptr<Gradient> gradient;

    bool autoZoomActive;
    bool speedMode;
    bool verboseMode;
    bool exitAfterFirstDisplay;
    bool autoScreenshotMode;
    GridMandelbrotCalculator::EngineType currentEngineType;

    void initSDL();
    void switchToOpenGL();
    void switchToSDLRenderer();
    void createCalculator();
    void render();
    void compute(); // Helper to handle context switching
    void handleResize(int newWidth, int newHeight);

    // Interaction helpers
    SDL_Rect calculateSelectionRect(int startX, int startY, int endX, int endY, bool centerBased);
    void zoomToRegion(int x1, int y1, int x2, int y2);
    void zoomToRect(int x1, int y1, int x2, int y2, bool inverse = false);
    void animateRectToRect(int startX, int startY, int startWidth, int startHeight,
                           int endX, int endY, int endWidth, int endHeight,
                           int steps = 15, int frameDelay = 16);
    void blinkRect(int x, int y, int w, int h, int times = 3, int blinkDelay = 150);
    void resetZoom();
    bool isZoomDisabled() const;
    void saveScreenshot(const std::string& basename = "mandelbrot");
    std::string generateUniqueFilename(const std::string& basename, const std::string& extension);
};

#pragma once

#include <SDL2/SDL.h>
#include <vector>
#include <memory>
#include "mandelbrot_calculator.h"
#include "zoom_point_chooser.h"
#include "gradient.h"

class MandelbrotApp
{
public:
    MandelbrotApp(int width, int height);
    ~MandelbrotApp();

    void run();

private:
    int width;
    int height;
    int calcWidth;
    int calcHeight;
    int pixelSize;

    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;

    std::unique_ptr<MandelbrotCalculator> calculator;
    std::unique_ptr<ZoomPointChooser> zoomChooser;
    std::unique_ptr<Gradient> gradient;

    bool autoZoomActive;

    void initSDL();
    void render();

    // Interaction helpers
    SDL_Rect calculateSelectionRect(int startX, int startY, int endX, int endY, bool centerBased);
    void zoomToRegion(int x1, int y1, int x2, int y2);
    void zoomToRect(int x1, int y1, int x2, int y2, bool inverse = false);
    void animateRectToRect(int startX, int startY, int startWidth, int startHeight,
                           int endX, int endY, int endWidth, int endHeight,
                           int steps = 15, int frameDelay = 16);
    void blinkRect(int x, int y, int w, int h, int times = 3, int blinkDelay = 150);
    void resetZoom();
    void setPixelSize(int newSize);
};

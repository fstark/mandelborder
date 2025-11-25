/* Modern Mandelbrot boundary tracing using SDL2 - Inspired by Joel Yliluoma's algorithm */

#include <SDL2/SDL.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <iostream>
#include <iomanip>
#include <chrono>

class MandelbrotRenderer
{
private:
    static constexpr int WIDTH = 800;
    static constexpr int HEIGHT = 600;
    static constexpr int MAX_ITER = 768;

    // Interesting region parameters
    // Default: full set view
    double cre = -0.5;
    double cim = 0.0;
    double diam = 3.0;

    // Alternative: zoomed detail
    // double cre = -1.36022;
    // double cim = 0.0653316;
    // double diam = 0.035;

    double minr, mini, maxr, maxi;
    double stepr, stepi;

    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;

    std::vector<int> data;
    std::vector<unsigned char> done;
    std::vector<unsigned> queue;
    unsigned queueHead, queueTail;

    std::vector<SDL_Color> palette;

    bool speedMode;

    enum Flags
    {
        LOADED = 1,
        QUEUED = 2
    };

public:
    MandelbrotRenderer()
        : window(nullptr), renderer(nullptr), texture(nullptr),
          queueHead(0), queueTail(0), speedMode(false)
    {
        data.resize(WIDTH * HEIGHT, 0);
        done.resize(WIDTH * HEIGHT, 0);
        queue.resize((WIDTH + HEIGHT) * 4);

        updateBounds(cre, cim, diam);

        initSDL();
        generatePalette();
    }

    ~MandelbrotRenderer()
    {
        if (texture)
            SDL_DestroyTexture(texture);
        if (renderer)
            SDL_DestroyRenderer(renderer);
        if (window)
            SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void initSDL()
    {
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
        {
            std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
            exit(1);
        }

        window = SDL_CreateWindow(
            "Mandelbrot Set - Boundary Tracing",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            WIDTH, HEIGHT, SDL_WINDOW_SHOWN);

        if (!window)
        {
            std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
            SDL_Quit();
            exit(1);
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer)
        {
            std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(window);
            SDL_Quit();
            exit(1);
        }

        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
                                    SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
        if (!texture)
        {
            std::cerr << "Texture creation failed: " << SDL_GetError() << std::endl;
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            exit(1);
        }
    }

    void generatePalette()
    {
        palette.resize(256);
        for (int c = 0; c < 256; ++c)
        {
            palette[c].r = static_cast<Uint8>(128 - 127 * std::cos(c * 0.01227 * 1));
            palette[c].g = static_cast<Uint8>(128 - 127 * std::cos(c * 0.01227 * 3));
            palette[c].b = static_cast<Uint8>(128 - 127 * std::cos(c * 0.01227 * 5));
            palette[c].a = 255;
        }
    }

    int iterate(double x, double y)
    {
        double r = x, i = y;
        int iter;

        for (iter = 0; iter < MAX_ITER; ++iter)
        {
            double r2 = r * r;
            double i2 = i * i;

            if (r2 + i2 >= 4.0)
                break;

            double ri = r * i;
            i = ri + ri + y; // z = z^2 + c
            r = r2 - i2 + x;
        }

        return iter;
    }

    void addQueue(unsigned p)
    {
        if (done[p] & QUEUED)
            return;
        done[p] |= QUEUED;
        queue[queueHead++] = p;
        if (queueHead == queue.size())
            queueHead = 0;
    }

    int load(unsigned p)
    {
        if (done[p] & LOADED)
            return data[p];

        unsigned x = p % WIDTH;
        unsigned y = p / WIDTH;

        int result = iterate(minr + x * stepr, mini + y * stepi);

        done[p] |= LOADED;
        return data[p] = result;
    }

    void scan(unsigned p)
    {
        unsigned x = p % WIDTH;
        unsigned y = p / WIDTH;

        int center = load(p);

        bool ll = x >= 1;
        bool rr = x < WIDTH - 1;
        bool uu = y >= 1;
        bool dd = y < HEIGHT - 1;

        // Check if neighbors differ from center
        bool l = ll && load(p - 1) != center;
        bool r = rr && load(p + 1) != center;
        bool u = uu && load(p - WIDTH) != center;
        bool d = dd && load(p + WIDTH) != center;

        if (l)
            addQueue(p - 1);
        if (r)
            addQueue(p + 1);
        if (u)
            addQueue(p - WIDTH);
        if (d)
            addQueue(p + WIDTH);

        // Check corner pixels (diagonal neighbors)
        if ((uu && ll) && (l || u))
            addQueue(p - WIDTH - 1);
        if ((uu && rr) && (r || u))
            addQueue(p - WIDTH + 1);
        if ((dd && ll) && (l || d))
            addQueue(p + WIDTH - 1);
        if ((dd && rr) && (r || d))
            addQueue(p + WIDTH + 1);
    }

    void compute()
    {
        std::cout << "Computing Mandelbrot set using boundary tracing..." << std::endl;

        // Start high-precision timer
        auto startTime = std::chrono::high_resolution_clock::now();

        // Start by adding screen edges to queue
        for (unsigned y = 0; y < HEIGHT; ++y)
        {
            addQueue(y * WIDTH + 0);
            addQueue(y * WIDTH + (WIDTH - 1));
        }
        for (unsigned x = 1; x < WIDTH - 1; ++x)
        {
            addQueue(0 * WIDTH + x);
            addQueue((HEIGHT - 1) * WIDTH + x);
        }

        // Process the queue (ring buffer)
        unsigned flag = 0;
        unsigned processed = 0;
        while (queueTail != queueHead)
        {
            unsigned p;

            // Alternate between front and back of queue for better cache locality
            if (queueHead <= queueTail || ++flag & 3)
            {
                p = queue[queueTail++];
                if (queueTail == queue.size())
                    queueTail = 0;
            }
            else
            {
                p = queue[--queueHead];
            }

            scan(p);

            // Update display periodically (skip in speed mode)
            ++processed;
            if (!speedMode && processed % 1000 == 0)
            {
                render();
            }
        }

        // Fill uncalculated areas with neighbor color
        for (unsigned p = 0; p < WIDTH * HEIGHT - 1; ++p)
        {
            if (done[p] & LOADED)
            {
                if (!(done[p + 1] & LOADED))
                {
                    data[p + 1] = data[p];
                    done[p + 1] |= LOADED;
                }
            }
        }

        // Stop timer and calculate elapsed time
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        double milliseconds = duration.count() / 1000.0;
        double seconds = duration.count() / 1000000.0;

        unsigned totalPixels = WIDTH * HEIGHT;
        double ratio = (double)processed / totalPixels * 100.0;
        std::cout << "Computation complete! Processed " << processed << " / " << totalPixels
                  << " pixels (" << std::fixed << std::setprecision(1) << ratio << "%)";

        if (speedMode)
        {
            double processedPixelsPerSec = processed / seconds;
            double totalPixelsPerSec = totalPixels / seconds;
            std::cout << " in " << std::fixed << std::setprecision(1) << milliseconds << " ms"
                      << " (" << std::fixed << std::setprecision(0) << processedPixelsPerSec << " processed px/s"
                      << ", " << std::fixed << std::setprecision(0) << totalPixelsPerSec << " total px/s)";
        }
        std::cout << std::endl;
    }

    void render()
    {
        Uint32 *pixels;
        int pitch;

        SDL_LockTexture(texture, nullptr, (void **)&pixels, &pitch);

        for (int y = 0; y < HEIGHT; ++y)
        {
            for (int x = 0; x < WIDTH; ++x)
            {
                int p = y * WIDTH + x;
                int iter = data[p] % 256;
                SDL_Color color = palette[iter];
                pixels[y * (pitch / 4) + x] = (color.r << 16) | (color.g << 8) | color.b;
            }
        }

        SDL_UnlockTexture(texture);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    void run()
    {
        compute();
        render();

        std::cout << "Press ESC to quit, SPACE to recompute, R to reset zoom, S to toggle speed mode" << std::endl;
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
                        reset();
                        compute();
                        render();
                    }
                    else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r)
                    {
                        resetZoom();
                        reset();
                        compute();
                        render();
                    }
                    else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_s)
                    {
                        speedMode = !speedMode;
                        std::cout << "Speed mode: " << (speedMode ? "ON" : "OFF") << std::endl;
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

                        int dx = currentMouseX - dragStartX;
                        int dy = currentMouseY - dragStartY;
                        double aspectRatio = (double)WIDTH / HEIGHT;
                        int rectX, rectY, width, height;

                        if (ctrlPressed)
                        {
                            // Center-based
                            width = std::abs(dx) * 2;
                            height = std::abs(dy) * 2;

                            if (width / aspectRatio > height)
                                height = (int)(width / aspectRatio);
                            else
                                width = (int)(height * aspectRatio);

                            rectX = dragStartX - width / 2;
                            rectY = dragStartY - height / 2;
                        }
                        else
                        {
                            // Corner-based
                            width = std::abs(dx);
                            height = std::abs(dy);

                            if (width / aspectRatio > height)
                                height = (int)(width / aspectRatio);
                            else
                                width = (int)(height * aspectRatio);

                            rectX = std::min(dragStartX, currentMouseX);
                            rectY = std::min(dragStartY, currentMouseY);
                        }

                        SDL_Rect rect = {rectX, rectY, width, height};
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

                        int x1, y1, x2, y2;
                        double aspectRatio = (double)WIDTH / HEIGHT;

                        // Check if this was a click (no drag) vs a drag
                        int dx = dragEndX - dragStartX;
                        int dy = dragEndY - dragStartY;
                        int dragDistance = std::abs(dx) + std::abs(dy);

                        if (dragDistance < 5)
                        {
                            // Click with no drag - zoom 2x centered on click point
                            int width = WIDTH / 2;
                            int height = HEIGHT / 2;

                            x1 = dragStartX - width / 2;
                            y1 = dragStartY - height / 2;
                            x2 = x1 + width;
                            y2 = y1 + height;
                        }
                        else
                        {
                            // Check if CTRL is pressed for center-based zoom
                            bool ctrlPressed = (modState & KMOD_CTRL) != 0;

                            if (ctrlPressed)
                            {
                                // Center-based zoom
                                int width = std::abs(dx) * 2;
                                int height = std::abs(dy) * 2;

                                if (width / aspectRatio > height)
                                    height = (int)(width / aspectRatio);
                                else
                                    width = (int)(height * aspectRatio);

                                x1 = dragStartX - width / 2;
                                y1 = dragStartY - height / 2;
                                x2 = x1 + width;
                                y2 = y1 + height;
                            }
                            else
                            {
                                // Corner-based zoom
                                int width = std::abs(dx);
                                int height = std::abs(dy);

                                if (width / aspectRatio > height)
                                    height = (int)(width / aspectRatio);
                                else
                                    width = (int)(height * aspectRatio);

                                x1 = std::min(dragStartX, dragEndX);
                                y1 = std::min(dragStartY, dragEndY);
                                x2 = x1 + width;
                                y2 = y1 + height;
                            }
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

                        int dx = currentMouseX - dragStartX;
                        int dy = currentMouseY - dragStartY;

                        // Maintain aspect ratio (WIDTH:HEIGHT = 4:3)
                        double aspectRatio = (double)WIDTH / HEIGHT;
                        int rectX, rectY, width, height;

                        if (ctrlPressed)
                        {
                            // Center-based: rectangle centered on dragStart
                            width = std::abs(dx) * 2;
                            height = std::abs(dy) * 2;

                            if (width / aspectRatio > height)
                                height = (int)(width / aspectRatio);
                            else
                                width = (int)(height * aspectRatio);

                            rectX = dragStartX - width / 2;
                            rectY = dragStartY - height / 2;
                        }
                        else
                        {
                            // Corner-based: rectangle from dragStart to current position
                            width = std::abs(dx);
                            height = std::abs(dy);

                            if (width / aspectRatio > height)
                                height = (int)(width / aspectRatio);
                            else
                                width = (int)(height * aspectRatio);

                            rectX = std::min(dragStartX, currentMouseX);
                            rectY = std::min(dragStartY, currentMouseY);
                        }

                        SDL_Rect rect = {rectX, rectY, width, height};
                        SDL_RenderDrawRect(renderer, &rect);
                        SDL_RenderPresent(renderer);
                    }
                    break;
                }
            }

            SDL_Delay(16); // ~60 FPS
        }
    }

    void reset()
    {
        std::fill(data.begin(), data.end(), 0);
        std::fill(done.begin(), done.end(), 0);
        queueHead = queueTail = 0;
    }

    void resetZoom()
    {
        updateBounds(-0.5, 0.0, 3.0);
    }

    void zoomToRegion(int x1, int y1, int x2, int y2)
    {
        if (x1 == x2 || y1 == y2)
            return;

        // Ensure x1,y1 is top-left
        if (x1 > x2)
            std::swap(x1, x2);
        if (y1 > y2)
            std::swap(y1, y2);

        // Convert pixel coordinates to complex plane
        double re1 = minr + x1 * stepr;
        double im1 = mini + y1 * stepi;
        double re2 = minr + x2 * stepr;
        double im2 = mini + y2 * stepi;

        double new_cre = (re1 + re2) / 2.0;
        double new_cim = (im1 + im2) / 2.0;
        double new_diam = std::max(re2 - re1, im2 - im1);

        updateBounds(new_cre, new_cim, new_diam);

        std::cout << "Zoomed to: center=(" << cre << ", " << cim
                  << "), diameter=" << diam << std::endl;
    }

    void updateBounds(double new_cre, double new_cim, double new_diam)
    {
        cre = new_cre;
        cim = new_cim;
        diam = new_diam;
        minr = cre - diam * 0.5 * WIDTH / HEIGHT;
        mini = cim - diam * 0.5;
        maxr = cre + diam * 0.5 * WIDTH / HEIGHT;
        maxi = cim + diam * 0.5;
        stepr = (maxr - minr) / WIDTH;
        stepi = (maxi - mini) / HEIGHT;
    }

    void animateRectToRect(int startX, int startY, int startWidth, int startHeight,
                           int endX, int endY, int endWidth, int endHeight,
                           int steps = 15, int frameDelay = 16)
    {
        // Skip animation in speed mode
        if (speedMode)
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

    void zoomToRect(int x1, int y1, int x2, int y2, bool inverse = false)
    {
        if (inverse)
        {
            // Zoom OUT: animate full screen shrinking to rectangle
            animateRectToRect(0, 0, WIDTH, HEIGHT, x1, y1, x2 - x1, y2 - y1);

            // Calculate scale and new center
            double scale = std::max((double)WIDTH / (x2 - x1), (double)HEIGHT / (y2 - y1));
            int offsetX = (x1 + x2) / 2 - WIDTH / 2;
            int offsetY = (y1 + y2) / 2 - HEIGHT / 2;

            double new_cre = cre + offsetX * stepr * scale;
            double new_cim = cim + offsetY * stepi * scale;
            double new_diam = diam * scale;
            updateBounds(new_cre, new_cim, new_diam);
        }
        else
        {
            // Zoom IN: animate rectangle expanding to full screen
            animateRectToRect(x1, y1, x2 - x1, y2 - y1, 0, 0, WIDTH, HEIGHT);
            zoomToRegion(x1, y1, x2, y2);
        }

        reset();
        compute();
        render();
    }
};

int main(int argc, char *argv[])
{
    try
    {
        MandelbrotRenderer app;
        app.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

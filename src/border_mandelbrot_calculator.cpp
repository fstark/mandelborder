#include "border_mandelbrot_calculator.h"
#include <cmath>
#include <algorithm>

BorderMandelbrotCalculator::BorderMandelbrotCalculator(int w, int h)
    : StorageMandelbrotCalculator(w, h), queueHead(0), queueTail(0)
{
    done.resize(width * height, 0);
    // Resize to max possible pixels + 1 to prevent ring buffer overflow
    queue.resize(width * height + 1);
}

void BorderMandelbrotCalculator::reset()
{
    StorageMandelbrotCalculator::reset();
    std::fill(done.begin(), done.end(), 0);
    queueHead = queueTail = 0;
}

int BorderMandelbrotCalculator::iterate(double x, double y)
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

void BorderMandelbrotCalculator::addQueue(unsigned p)
{
    if (done[p] & QUEUED)
        return;
    done[p] |= QUEUED;
    queue[queueHead++] = p;
    if (queueHead == queue.size())
        queueHead = 0;
}

int BorderMandelbrotCalculator::load(unsigned p)
{
    if (done[p] & LOADED)
        return data[p];

    unsigned x = p % width;
    unsigned y = p / width;

    int result = iterate(minr + x * stepr, mini + y * stepi);

    done[p] |= LOADED;
    return data[p] = result;
}

void BorderMandelbrotCalculator::scan(unsigned p)
{
    int x = p % width;
    int y = p / width;

    int center = load(p);

    bool ll = x >= 1;
    bool rr = x < width - 1;
    bool uu = y >= 1;
    bool dd = y < height - 1;

    // Check if neighbors differ from center
    bool l = ll && load(p - 1) != center;
    bool r = rr && load(p + 1) != center;
    bool u = uu && load(p - width) != center;
    bool d = dd && load(p + width) != center;

    if (l)
        addQueue(p - 1);
    if (r)
        addQueue(p + 1);
    if (u)
        addQueue(p - width);
    if (d)
        addQueue(p + width);

    // Check corner pixels (diagonal neighbors)
    if ((uu && ll) && (l || u))
        addQueue(p - width - 1);
    if ((uu && rr) && (r || u))
        addQueue(p - width + 1);
    if ((dd && ll) && (l || d))
        addQueue(p + width - 1);
    if ((dd && rr) && (r || d))
        addQueue(p + width + 1);
}

void BorderMandelbrotCalculator::compute(std::function<void()> progressCallback)
{
    // Start high-precision timer
    data.assign(width * height, 0);

    // First Pass: Border Tracing

    // Start by adding screen edges to queue
    for (int y = 0; y < height; ++y)
    {
        addQueue(y * width + 0);
        addQueue(y * width + (width - 1));
    }
    for (int x = 1; x < width - 1; ++x)
    {
        addQueue(0 * width + x);
        addQueue((height - 1) * width + x);
    }

    // Process the queue (mixed FIFO/LIFO for better visual effect)
    unsigned processed = 0;
    unsigned flag = 0;
    while (queueTail != queueHead)
    {
        unsigned p;
        if (queueHead <= queueTail || ++flag & 3)
        {
            // FIFO: dequeue from tail
            p = queue[queueTail++];
            if (queueTail == queue.size())
                queueTail = 0;
        }
        else
        {
            // LIFO: dequeue from head
            if (queueHead == 0)
                queueHead = queue.size();
            p = queue[--queueHead];
        }

        scan(p);

        // Update display periodically (skip in speed mode)
        ++processed;
        if (!speedMode && processed % 1000 == 0)
        {
            if (progressCallback)
                progressCallback();
        }
    }

    // Fill uncalculated areas with neighbor color
    for (int p = 0; p < width * height - 1; ++p)
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

}

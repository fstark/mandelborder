#include "simd_mandelbrot_calculator.h"
#include <cmath>
#include <array>

SimdMandelbrotCalculator::SimdMandelbrotCalculator(int w, int h)
    : StorageMandelbrotCalculator(w, h)
{
}

void SimdMandelbrotCalculator::compute(std::function<void()> progressCallback)
{
    // Batch size for SIMD. 
    // AVX2 processes 4 doubles (256 bits). AVX-512 processes 8 doubles (512 bits).
    // 8 is a good number to unroll loops for.
    constexpr int BATCH_SIZE = 8;

    unsigned processed = 0;

    for (int y = 0; y < height; ++y)
    {
        double cy = mini + y * stepi;
        
        for (int x = 0; x < width; x += BATCH_SIZE)
        {
            int current_batch_size = std::min(BATCH_SIZE, width - x);
            
            // Arrays for batch processing
            // Use 64-bit integers for mask and iters to match double width (helps vectorization)
            alignas(64) double cr[BATCH_SIZE];
            alignas(64) double ci[BATCH_SIZE];
            alignas(64) double zr[BATCH_SIZE];
            alignas(64) double zi[BATCH_SIZE];
            alignas(64) long long iters[BATCH_SIZE];
            alignas(64) long long mask[BATCH_SIZE]; // 1 if active, 0 if escaped

            // Initialize batch
            // We initialize all BATCH_SIZE elements to ensure the loop size is constant
            // For elements beyond width, we just duplicate the last valid coordinate or use 0
            // This avoids branches in initialization
            for (int i = 0; i < BATCH_SIZE; ++i)
            {
                // Use a safe index for calculation to avoid branching
                // If i >= current_batch_size, we just compute a dummy value (e.g. 0,0)
                // We won't save it anyway.
                int offset = (i < current_batch_size) ? i : 0;
                
                cr[i] = minr + (x + offset) * stepr;
                ci[i] = cy;
                zr[i] = cr[i];
                zi[i] = ci[i];
                iters[i] = 0;
                // Mask is 0 for padding elements so they don't keep iterating
                mask[i] = (i < current_batch_size) ? 1 : 0;
            }

            // Main iteration loop
            for (int k = 0; k < MAX_ITER; ++k)
            {
                // Branchless inner loop for better auto-vectorization
                // The compiler should unroll this and use SIMD instructions
                for (int i = 0; i < BATCH_SIZE; ++i)
                {
                    double r2 = zr[i] * zr[i];
                    double i2 = zi[i] * zi[i];
                    double ri = zr[i] * zi[i];

                    // Calculate next values
                    double next_zr = r2 - i2 + cr[i];
                    double next_zi = ri + ri + ci[i];

                    // Check escape condition
                    bool escaped = (r2 + i2 >= 4.0);
                    
                    // Update mask: if already inactive (0) or escaped (true), result is 0
                    mask[i] = mask[i] & (!escaped);

                    // Update Z values only if still active
                    zr[i] = mask[i] ? next_zr : zr[i];
                    zi[i] = mask[i] ? next_zi : zi[i];
                    
                    // Increment iteration count if active
                    iters[i] += mask[i];
                }

                // Check if any lanes are still active
                // We do this outside the vector loop to avoid breaking vectorization
                long long active_lanes = 0;
                for (int i = 0; i < BATCH_SIZE; ++i)
                {
                    active_lanes |= mask[i];
                }

                if (active_lanes == 0)
                    break;
            }

            // Store results
            for (int i = 0; i < current_batch_size; ++i)
            {
                data[y * width + x + i] = iters[i];
            }
            
            processed += current_batch_size;
        }

        if (!speedMode && processed % (width * 10) < BATCH_SIZE)
        {
            if (progressCallback)
                progressCallback();
        }
    }

}

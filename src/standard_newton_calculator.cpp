#include "standard_newton_calculator.h"
#include <cmath>

typedef struct {
  double x;
  double y;
} vec2;

const vec2 reminusone = vec2(-1.0, 0.0);

vec2 complexMultiply(vec2 a, vec2 b) {
  return vec2(a.x * b.x - a.y * b.y, a.y * b.x + a.x * b.y);
}

vec2 complexDivide(vec2 a, vec2 b) {
  double denom = b.x * b.x + b.y * b.y;
  return vec2((a.x * b.x + a.y * b.y) / denom, (a.y * b.x - a.x * b.y) / denom);
}

float complexDistance(vec2 a, vec2 b) {
  return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

vec2 complexScale(double factor, vec2 z) {
  z.x *= factor;
  z.y *= factor;
  return z;
}
vec2 complexAdd(vec2 a, vec2 b) { return vec2(a.x + b.x, a.y + b.y); }

// roots
const vec2 Root0 = vec2(1, 0.00001);
const vec2 Root1 = vec2(-.5, .86603);
const vec2 Root2 = vec2(-.5, -.86603);

vec2 function(vec2 z) {
  // z^3-1
  return complexAdd(reminusone, complexMultiply(z, complexMultiply(z, z)));
}

vec2 derivative(vec2 z) {
  // derivative of z^3-1 which is 3z^2
  return complexScale(3., complexMultiply(z, z));
}

StandardNewtonCalculator::StandardNewtonCalculator(int w, int h)
    : StorageMandelbrotCalculator(w, h) {}

int StandardNewtonCalculator::iterate(double x, double y) {
  vec2 z = vec2(x, y);
  for (int i = 0;i < MAX_ITER
    ; ++i) {
    z = complexAdd(z, complexScale(-1.0, complexDivide(function(z), derivative(z))));
  }
  double distr0 = complexDistance(z, Root0);
  double distr1 = complexDistance(z, Root1);
  double distr2 = complexDistance(z, Root2);
  if (distr0 < distr1) {
    if (distr0 < distr2)
      return 0;
    return 2;
  } else {
    if (distr1 < distr2)
      return 1;
    else return 2;
  }   
   return 4;
}

void StandardNewtonCalculator::compute(std::function<void()> progressCallback) {
  int processed = 0;
  for (int y = 0; y < height; ++y) {
    double cy = mini + y * stepi;
    for (int x = 0; x < width; ++x) {
      double cx = minr + x * stepr;
      data[y * width + x] = iterate(cx, cy);
      processed++;
    }

    // Update display periodically (skip in speed mode)
    if (!speedMode && processed % (width * 10) == 0) // Update every 10 lines
    {
      if (progressCallback)
        progressCallback();
    }
  }
}

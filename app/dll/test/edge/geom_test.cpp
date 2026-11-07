// Unit tests for geom.h.
#include <cstdio>
#include "../../src/edge/geom.h"

using namespace edgeplus;

static_assert(Union(Rect{0, 0, 10, 10}, Rect{20, 5, 10, 10}) ==
                  Rect{0, 0, 30, 15},
              "the union spans both rects");
static_assert(Union(Rect{}, Rect{4, 4, 2, 2}) == Rect{4, 4, 2, 2},
              "an empty rect adds nothing");
static_assert(Union(Rect{4, 4, 2, 2}, Rect{}) == Rect{4, 4, 2, 2},
              "an empty rect adds nothing, either side");
static_assert(GrowVertically(Rect{0, 10, 5, 20}, 3) == Rect{0, 7, 5, 26},
              "growing reaches above and below, never sideways");
static_assert(Contains(GrowVertically(Rect{0, 10, 5, 20}, 3), Point{2, 8}) &&
                  !Contains(Rect{0, 10, 5, 20}, Point{2, 8}),
              "a point just above the rows is within reach");

int main() {
  std::printf("OK: 5 checks passed\n");
  return 0;
}

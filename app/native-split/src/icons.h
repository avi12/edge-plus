// icons.h — the vector icons the DLL draws in Edge's own buttons. One concern:
// build gfx::VectorIcon data in Edge's heap (never freed, so a button that
// outlives this DLL never points into unloaded memory): the pane grip's six
// dots, and the ⋯ row's arrows — Edge's own ↑ and ↓, and ← → as quarter turns
// of its ↑, so all four come from one icon family.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <vector>
#include "edge.h"
#include "pane_tree.h"

namespace fixedge::icons {

using namespace fixedge::edge155;

// Edge's gfx::VectorIcon ABI — Chromium's VectorIcon {reps, reps_size, name},
// each VectorIconRep {path, path_size}, each path element a 4-byte command id
// or float argument.
struct IconRepresentation {
  const void* path;
  size_t pathSize;
};
struct VectorIcon {
  const IconRepresentation* representations;
  size_t representationCount;
  const char* name;
};

using PathElement = uint32_t;

inline PathElement CommandElement(int command) {
  return static_cast<PathElement>(command);
}
inline PathElement ArgumentElement(float value) {
  PathElement element = 0;
  memcpy(&element, &value, sizeof(element));
  return element;
}
inline float ArgumentOf(PathElement element) {
  float value = 0.f;
  memcpy(&value, &element, sizeof(value));
  return value;
}

// One single-representation icon, copied into one Edge-heap block: the two
// structs, then the path, then the name.
inline const VectorIcon* Build(const char* name,
                               const std::vector<PathElement>& path) {
  const size_t nameSize = strlen(name) + 1;
  const size_t pathBytes = path.size() * sizeof(PathElement);
  const size_t byteCount =
      sizeof(VectorIcon) + sizeof(IconRepresentation) + pathBytes + nameSize;
  auto* block = static_cast<uint8_t*>(edge::EdgeNew(byteCount));
  auto* vectorIcon = reinterpret_cast<VectorIcon*>(block);
  auto* representation =
      reinterpret_cast<IconRepresentation*>(block + sizeof(VectorIcon));
  uint8_t* pathCopy = block + sizeof(VectorIcon) + sizeof(IconRepresentation);
  auto* nameCopy = reinterpret_cast<char*>(pathCopy + pathBytes);
  memcpy(pathCopy, path.data(), pathBytes);
  memcpy(nameCopy, name, nameSize);
  *representation = IconRepresentation{pathCopy, path.size()};
  *vectorIcon = VectorIcon{representation, 1, nameCopy};
  return vectorIcon;
}

// The 6-dot grip (two columns, three rows of r=1 dots on a 16 canvas), drawn
// like Edge's ⋯ icon.
inline const VectorIcon* Grip() {
  static const VectorIcon* icon = [] {
    constexpr int kRows = 3;
    constexpr int kColumns = 2;
    constexpr float kCanvasSize = 16.f;
    constexpr float kFirstCenterX = 6.f;
    constexpr float kFirstCenterY = 4.f;
    constexpr float kDotSpacing = 4.f;
    constexpr float kDotRadius = 1.f;
    std::vector<PathElement> path{CommandElement(off::ICON_CANVAS_DIMENSIONS),
                                  ArgumentElement(kCanvasSize)};
    for (int row = 0; row < kRows; row++) {
      for (int column = 0; column < kColumns; column++) {
        const float centerX =
            kFirstCenterX + kDotSpacing * static_cast<float>(column);
        const float centerY =
            kFirstCenterY + kDotSpacing * static_cast<float>(row);
        // The dot's right-most point, then two half arcs (rx ry rotation
        // large-arc sweep dx dy) around it.
        path.push_back(CommandElement(off::ICON_MOVE_TO));
        path.push_back(ArgumentElement(centerX + kDotRadius));
        path.push_back(ArgumentElement(centerY));
        for (const float deltaX : {-2.f, 2.f}) {
          path.push_back(CommandElement(off::ICON_R_ARC_TO));
          for (const float argument : {1.f, 1.f, 0.f, 1.f, 1.f, deltaX, 0.f}) {
            path.push_back(ArgumentElement(argument));
          }
        }
        path.push_back(CommandElement(off::ICON_CLOSE));
      }
    }
    return Build("kFixEdgePaneGripIcon", path);
  }();
  return icon;
}

// --- the Gemini spark ----------------------------------------------------------

// Gemini's four-pointed spark on a 24 canvas: its points at the canvas's edge
// midpoints (top T, right R, bottom B, left L), joined by quarter circles
// centred on the corners, so each side curves in toward the middle (sweep 0 =
// counter-clockwise picks that centre). Drawn in Gemini's colours, blue
// through purple to red from left to right: the whole spark in the first
// colour, then the part right of each next cut over it in the next colour,
// every layer overlapping the one below so no seam shows between them.
inline constexpr float kSparkCanvasSize = 24.f;
inline constexpr float kSparkRadius = 10.f;
inline constexpr float kSparkCentre = 12.f;  // T and B's x, L and R's y
inline constexpr float kSparkNear =
    kSparkCentre - kSparkRadius;                                 // T's y, L's x
inline constexpr float kSparkFar = kSparkCentre + kSparkRadius;  // B's y, R's x
inline constexpr int kSparkBandCount = 8;
struct SparkColor {
  float red, green, blue;
};
inline constexpr SparkColor kSparkBlue{0x42, 0x85, 0xf4};
inline constexpr SparkColor kSparkPurple{0x9b, 0x72, 0xcb};
inline constexpr SparkColor kSparkRed{0xd9, 0x65, 0x70};

inline SparkColor SparkBandColor(int band) {
  const float position =
      static_cast<float>(band) / static_cast<float>(kSparkBandCount - 1);
  const bool isFirstHalf = position < 0.5f;
  const SparkColor from = isFirstHalf ? kSparkBlue : kSparkPurple;
  const SparkColor to = isFirstHalf ? kSparkPurple : kSparkRed;
  const float mix = (isFirstHalf ? position : position - 0.5f) * 2.f;
  return SparkColor{from.red + (to.red - from.red) * mix,
                    from.green + (to.green - from.green) * mix,
                    from.blue + (to.blue - from.blue) * mix};
}

inline void AddMoveTo(std::vector<PathElement>& path, float x, float y) {
  path.push_back(CommandElement(off::ICON_MOVE_TO));
  path.push_back(ArgumentElement(x));
  path.push_back(ArgumentElement(y));
}

// An arc of a spark side, to the point `deltaX`, `deltaY` from where the path
// is, on the same circle and direction as the side it is part of.
inline void AddSparkArc(std::vector<PathElement>& path, float deltaX,
                        float deltaY) {
  path.push_back(CommandElement(off::ICON_R_ARC_TO));
  for (const float argument :
       {kSparkRadius, kSparkRadius, 0.f, 0.f, 0.f, deltaX, deltaY}) {
    path.push_back(ArgumentElement(argument));
  }
}

// The part of the spark at or right of x = `cut`, as one closed path.
inline void AddSparkRightOf(std::vector<PathElement>& path, float cut) {
  if (cut <= kSparkNear) {  // the whole spark: T, R, B, L, back to T
    AddMoveTo(path, kSparkCentre, kSparkNear);
    AddSparkArc(path, kSparkRadius, kSparkRadius);
    AddSparkArc(path, -kSparkRadius, kSparkRadius);
    AddSparkArc(path, -kSparkRadius, -kSparkRadius);
    AddSparkArc(path, kSparkRadius, -kSparkRadius);
  } else if (cut < kSparkCentre) {  // cuts the two left sides, keeps T
    const float reach = std::sqrt(kSparkRadius * kSparkRadius -
                                  (cut - kSparkNear) * (cut - kSparkNear));
    const float upperY = kSparkNear + reach;  // on the L-to-T side
    const float lowerY = kSparkFar - reach;   // on the B-to-L side
    AddMoveTo(path, cut, upperY);
    AddSparkArc(path, kSparkCentre - cut, kSparkNear - upperY);
    AddSparkArc(path, kSparkRadius, kSparkRadius);
    AddSparkArc(path, -kSparkRadius, kSparkRadius);
    AddSparkArc(path, cut - kSparkCentre, lowerY - kSparkFar);
  } else {  // cuts the two right sides, keeps R
    const float reach = std::sqrt(kSparkRadius * kSparkRadius -
                                  (kSparkFar - cut) * (kSparkFar - cut));
    const float upperY = kSparkNear + reach;  // on the T-to-R side
    const float lowerY = kSparkFar - reach;   // on the R-to-B side
    AddMoveTo(path, cut, upperY);
    AddSparkArc(path, kSparkFar - cut, kSparkCentre - upperY);
    AddSparkArc(path, cut - kSparkFar, lowerY - kSparkCentre);
  }
  path.push_back(CommandElement(off::ICON_CLOSE));
}

inline const VectorIcon* GeminiSpark() {
  static const VectorIcon* icon = [] {
    std::vector<PathElement> path{CommandElement(off::ICON_CANVAS_DIMENSIONS),
                                  ArgumentElement(kSparkCanvasSize)};
    constexpr float kOpaque = 255.f;
    const float bandWidth =
        (kSparkFar - kSparkNear) / static_cast<float>(kSparkBandCount);
    for (int band = 0; band < kSparkBandCount; band++) {
      const SparkColor color = SparkBandColor(band);
      path.push_back(CommandElement(off::ICON_NEW_PATH));
      path.push_back(CommandElement(off::ICON_PATH_COLOR_ARGB));
      for (const float channel :
           {kOpaque, color.red, color.green, color.blue}) {
        path.push_back(ArgumentElement(channel));
      }
      AddSparkRightOf(path, kSparkNear + bandWidth * static_cast<float>(band));
    }
    return Build("kFixEdgeGeminiSparkIcon", path);
  }();
  return icon;
}

// The same spark in one colour, the one the icon is drawn in: Chrome's Gemini
// button shows it so while its panel is open.
inline const VectorIcon* GeminiSparkMono() {
  static const VectorIcon* icon = [] {
    std::vector<PathElement> path{CommandElement(off::ICON_CANVAS_DIMENSIONS),
                                  ArgumentElement(kSparkCanvasSize)};
    path.push_back(CommandElement(off::ICON_NEW_PATH));
    AddSparkRightOf(path, kSparkNear);
    return Build("kFixEdgeGeminiSparkMonoIcon", path);
  }();
  return icon;
}

// --- the arrows --------------------------------------------------------------

// How many arguments a path command takes, for the commands Edge's arrow icon
// uses; -1 for any other, which the quarter turn refuses.
inline int ArgumentCount(int command) {
  switch (command) {
    case off::ICON_NEW_PATH:
    case off::ICON_CLOSE:
      return 0;
    case off::ICON_CANVAS_DIMENSIONS:
    case off::ICON_V_LINE_TO:
      return 1;
    case off::ICON_MOVE_TO:
    case off::ICON_LINE_TO:
    case off::ICON_R_LINE_TO:
      return 2;
    case off::ICON_R_ARC_TO:
      return 7;
    default:
      return -1;
  }
}

// `source` (an up arrow) turned a quarter to point `direction` (left or right)
// on its square canvas: absolute points turn about the canvas, relative moves
// turn without the offset, and a vertical line is written as an absolute line
// to its end point. Null if the source uses a command this does not know.
inline const VectorIcon* QuarterTurn(const VectorIcon* source,
                                     Direction direction, const char* name) {
  const IconRepresentation& representation = source->representations[0];
  const auto* elements = static_cast<const PathElement*>(representation.path);
  const bool isLeft = direction == Direction::kLeft;
  float canvas = 0.f;
  float currentX = 0.f, currentY = 0.f, startX = 0.f, startY = 0.f;
  std::vector<PathElement> out;
  auto appendPoint = [&](int command, float x, float y) {
    out.push_back(CommandElement(command));
    out.push_back(ArgumentElement(isLeft ? y : canvas - y));
    out.push_back(ArgumentElement(isLeft ? canvas - x : x));
  };
  auto turnedDelta = [&](float deltaX, float deltaY, float* turnedX,
                         float* turnedY) {
    *turnedX = isLeft ? deltaY : -deltaY;
    *turnedY = isLeft ? -deltaX : deltaX;
  };
  size_t i = 0;
  while (i < representation.pathSize) {
    const int command = static_cast<int>(elements[i]);
    const int argumentCount = ArgumentCount(command);
    const bool isKnownCommand =
        argumentCount >= 0 &&
        i + static_cast<size_t>(argumentCount) < representation.pathSize;
    if (!isKnownCommand) {
      return nullptr;
    }
    const PathElement* arguments = elements + i + 1;
    switch (command) {
      case off::ICON_CANVAS_DIMENSIONS:
        canvas = ArgumentOf(arguments[0]);
        out.push_back(elements[i]);
        out.push_back(arguments[0]);
        break;
      case off::ICON_MOVE_TO:
      case off::ICON_LINE_TO:
        currentX = ArgumentOf(arguments[0]);
        currentY = ArgumentOf(arguments[1]);
        if (command == off::ICON_MOVE_TO) {
          startX = currentX;
          startY = currentY;
        }
        appendPoint(command, currentX, currentY);
        break;
      case off::ICON_V_LINE_TO:
        currentY = ArgumentOf(arguments[0]);
        appendPoint(off::ICON_LINE_TO, currentX, currentY);
        break;
      case off::ICON_R_LINE_TO: {
        const float deltaX = ArgumentOf(arguments[0]);
        const float deltaY = ArgumentOf(arguments[1]);
        currentX += deltaX;
        currentY += deltaY;
        float turnedX = 0.f, turnedY = 0.f;
        turnedDelta(deltaX, deltaY, &turnedX, &turnedY);
        out.push_back(elements[i]);
        out.push_back(ArgumentElement(turnedX));
        out.push_back(ArgumentElement(turnedY));
        break;
      }
      case off::ICON_R_ARC_TO: {
        const float deltaX = ArgumentOf(arguments[5]);
        const float deltaY = ArgumentOf(arguments[6]);
        currentX += deltaX;
        currentY += deltaY;
        float turnedX = 0.f, turnedY = 0.f;
        turnedDelta(deltaX, deltaY, &turnedX, &turnedY);
        out.push_back(elements[i]);
        out.insert(out.end(), arguments, arguments + 5);  // radii and flags
        out.push_back(ArgumentElement(turnedX));
        out.push_back(ArgumentElement(turnedY));
        break;
      }
      case off::ICON_CLOSE:
        currentX = startX;
        currentY = startY;
        out.push_back(elements[i]);
        break;
      default:  // NEW_PATH
        out.push_back(elements[i]);
        break;
    }
    i += 1 + static_cast<size_t>(argumentCount);
  }
  return Build(name, out);
}

// The arrow for adding a pane in `direction`: Edge's ↑ / ↓, and its ↑ turned
// for ← / → (Edge's own back / forward arrows if the turn is refused).
inline const void* Arrow(Direction direction) {
  const auto* upArrow =
      static_cast<const VectorIcon*>(edge::At(rva::Icon_ArrowUp));
  switch (direction) {
    case Direction::kUp:
      return upArrow;
    case Direction::kDown:
      return edge::At(rva::Icon_ArrowDown);
    case Direction::kLeft: {
      static const VectorIcon* left =
          QuarterTurn(upArrow, Direction::kLeft, "kFixEdgeArrowLeftIcon");
      return left ? left : edge::At(rva::Icon_ArrowBack);
    }
    default: {
      static const VectorIcon* right =
          QuarterTurn(upArrow, Direction::kRight, "kFixEdgeArrowRightIcon");
      return right ? right : edge::At(rva::Icon_ArrowForward);
    }
  }
}

}  // namespace fixedge::icons

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
    std::vector<PathElement> path{
        CommandElement(edge155::off::ICON_CANVAS_DIMENSIONS),
        ArgumentElement(kCanvasSize)};
    for (int row = 0; row < kRows; row++) {
      for (int column = 0; column < kColumns; column++) {
        const float centerX =
            kFirstCenterX + kDotSpacing * static_cast<float>(column);
        const float centerY =
            kFirstCenterY + kDotSpacing * static_cast<float>(row);
        // The dot's right-most point, then two half arcs (rx ry rotation
        // large-arc sweep dx dy) around it.
        path.push_back(CommandElement(edge155::off::ICON_MOVE_TO));
        path.push_back(ArgumentElement(centerX + kDotRadius));
        path.push_back(ArgumentElement(centerY));
        for (const float deltaX : {-2.f, 2.f}) {
          path.push_back(CommandElement(edge155::off::ICON_R_ARC_TO));
          for (const float argument : {1.f, 1.f, 0.f, 1.f, 1.f, deltaX, 0.f}) {
            path.push_back(ArgumentElement(argument));
          }
        }
        path.push_back(CommandElement(edge155::off::ICON_CLOSE));
      }
    }
    return Build("kFixEdgePaneGripIcon", path);
  }();
  return icon;
}

// --- the arrows --------------------------------------------------------------

// How many arguments a path command takes, for the commands Edge's arrow icon
// uses; -1 for any other, which the quarter turn refuses.
inline int ArgumentCount(int command) {
  switch (command) {
    case edge155::off::ICON_NEW_PATH:
    case edge155::off::ICON_CLOSE:
      return 0;
    case edge155::off::ICON_CANVAS_DIMENSIONS:
    case edge155::off::ICON_V_LINE_TO:
      return 1;
    case edge155::off::ICON_MOVE_TO:
    case edge155::off::ICON_LINE_TO:
    case edge155::off::ICON_R_LINE_TO:
      return 2;
    case edge155::off::ICON_R_ARC_TO:
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
      case edge155::off::ICON_CANVAS_DIMENSIONS:
        canvas = ArgumentOf(arguments[0]);
        out.push_back(elements[i]);
        out.push_back(arguments[0]);
        break;
      case edge155::off::ICON_MOVE_TO:
      case edge155::off::ICON_LINE_TO:
        currentX = ArgumentOf(arguments[0]);
        currentY = ArgumentOf(arguments[1]);
        if (command == edge155::off::ICON_MOVE_TO) {
          startX = currentX;
          startY = currentY;
        }
        appendPoint(command, currentX, currentY);
        break;
      case edge155::off::ICON_V_LINE_TO:
        currentY = ArgumentOf(arguments[0]);
        appendPoint(edge155::off::ICON_LINE_TO, currentX, currentY);
        break;
      case edge155::off::ICON_R_LINE_TO: {
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
      case edge155::off::ICON_R_ARC_TO: {
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
      case edge155::off::ICON_CLOSE:
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
      static_cast<const VectorIcon*>(edge::At(edge155::rva::Icon_ArrowUp));
  switch (direction) {
    case Direction::kUp:
      return upArrow;
    case Direction::kDown:
      return edge::At(edge155::rva::Icon_ArrowDown);
    case Direction::kLeft: {
      static const VectorIcon* left =
          QuarterTurn(upArrow, Direction::kLeft, "kFixEdgeArrowLeftIcon");
      return left ? left : edge::At(edge155::rva::Icon_ArrowBack);
    }
    default: {
      static const VectorIcon* right =
          QuarterTurn(upArrow, Direction::kRight, "kFixEdgeArrowRightIcon");
      return right ? right : edge::At(edge155::rva::Icon_ArrowForward);
    }
  }
}

}  // namespace fixedge::icons

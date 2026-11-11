import {
  clampedOffset,
  movedIds,
  slideOffsets,
  slotOffset,
  targetIndex
} from "./reorder-math.ts";
import { type ComponentProps, tick } from "svelte";
import { Spring } from "svelte/motion";
import { SvelteMap } from "svelte/reactivity";
import { control } from "winui-svelte";
import { projectAs, releaseProjected } from "winui-svelte/bindings/lifetime";
import { PointerRoutedEventArgs } from "winui-svelte/bindings/microsoft/ui/xaml/input/PointerRoutedEventArgs";
import { UIElement } from "winui-svelte/bindings/microsoft/ui/xaml/UIElement";
import type { Border } from "winui-svelte/components";

// Dragging a row of the theme list: the pressed row lifts and
// follows the pointer, the others slide aside to open the slot it will land
// in, and on release it springs from where it was let go into that slot, and
// only then is the new order committed. The list's own drag reorder is not
// used: WinUI commits it as a removal and an insertion, and fades the moved
// row out and in.

/** The part of a row's control the drag moves and measures. */
interface RowControl {
  readonly actualHeight: number;
  translation: {
    x: number;
    y: number;
    z: number;
  };
}

/** The handler type of a pointer event of a `Border`: WinUI's own element and arguments. */
type PointerHandler = NonNullable<ComponentProps<typeof Border>["onpointerpressed"]>;
type PointerSender = Parameters<PointerHandler>[0];
type PointerEventArguments = Parameters<PointerHandler>[1];
type PointerToken = PointerRoutedEventArgs["pointer"];

/** What a WinUI pointer event of the row says, in the window's coordinates. */
interface PointerReading {
  pointer: PointerToken;
  pointerId: number;
  y: number;
  isLeftButtonPressed: boolean;
}

// The window's content is the frame of reference: a row that moves with the
// pointer would otherwise move the coordinates too.
function readPointer({ sender, args }: {
  sender: PointerSender;
  args: PointerEventArguments;
}): PointerReading {
  const projectedArgs = projectAs(args, PointerRoutedEventArgs);
  const element = projectAs(sender, UIElement);
  const { pointer } = projectedArgs;
  const point = projectedArgs.getCurrentPoint(element.xamlRoot?.content ?? element);
  const reading = {
    pointer,
    pointerId: pointer.pointerId,
    y: point.position.y,
    isLeftButtonPressed: point.properties.isLeftButtonPressed
  };
  releaseProjected(element);
  releaseProjected(projectedArgs);
  return reading;
}

// Capture sends every event of the pointer to the row until it is let go, so
// the drag goes on when the pointer leaves it.
function capturePointer({ sender, pointer }: {
  sender: PointerSender;
  pointer: PointerToken;
}) {
  const element = projectAs(sender, UIElement);
  element.capturePointer(pointer);
  releaseProjected(element);
}

function releasePointer({ sender, pointer }: {
  sender: PointerSender;
  pointer: PointerToken;
}) {
  const element = projectAs(sender, UIElement);
  element.releasePointerCapture(pointer);
  releaseProjected(element);
}

/** The WinUI pointer handlers a row takes to be dragged. */
export interface DragHandlers {
  onpointerpressed: PointerHandler;
  onpointermoved: PointerHandler;
  onpointerreleased: PointerHandler;
  onpointercanceled: PointerHandler;
}

interface Gesture {
  id: string;
  ids: readonly string[];
  index: number;
  heights: readonly number[];
  startY: number;
  pointer: PointerToken;
  pointerId: number;
  source: PointerSender;
  isDragging: boolean;
  target: number;
}

/** Pixels the pointer must travel before a press becomes a drag. */
const DRAG_THRESHOLD = 4;
/** How a row glides aside for the dragged one: no overshoot. */
const SLIDE_FEEL = {
  stiffness: 0.2,
  damping: 1
};
/** How the dragged row settles in its slot: a gentle overshoot. */
const DROP_FEEL = {
  stiffness: 0.15,
  damping: 0.55
};

/** The drag of one list: attach `row(id)` to each row's element and give it `handlers(id)`. */
export class ThemeReorder {
  /** The id of the row lifted now; null when none is. */
  draggedId = $state<string | null>(null);
  /** True from the moment the new order is committed until the rows stand in it: no glide of its own then. */
  isCommitting = false;
  private readonly rows = new SvelteMap<string, RowControl>();
  private readonly motions = new SvelteMap<string, Spring<number>>();
  private gesture: Gesture | null = null;
  private readonly orderOf: () => readonly string[];
  private readonly onCommit: (ids: string[]) => void;

  constructor({ orderOf, onCommit }: {
    /** The ids of the rows from top to bottom, as they stand now. */
    orderOf: () => readonly string[];
    /** The drop landed: the ids in their new order. */
    onCommit: (ids: string[]) => void;
  }) {
    this.orderOf = orderOf;
    this.onCommit = onCommit;
  }

  /**
   * The attachment for a row's element: the drag moves and measures its
   * control, and the row's spring is what its translation follows.
   */
  row(id: string) {
    return control<RowControl>(native => {
      const motion = new Spring(0, SLIDE_FEEL);
      this.rows.set(id, native);
      this.motions.set(id, motion);
      $effect(() => {
        native.translation = {
          x: 0,
          y: motion.current,
          z: 0
        };
      });
      return () => {
        if (this.rows.get(id) === native) {
          this.rows.delete(id);
          this.motions.delete(id);
        }
      };
    });
  }

  /** The handlers that make a row draggable. */
  handlers(id: string): DragHandlers {
    return {
      onpointerpressed: (sender, args) => this.press({
        sender,
        reading: readPointer({
          sender,
          args
        }),
        id
      }),
      onpointermoved: (sender, args) => this.move(
        readPointer({
          sender,
          args
        })
      ),
      onpointerreleased: (sender, args) => this.release(
        readPointer({
          sender,
          args
        })
      ),
      onpointercanceled: (sender, args) => this.cancel(
        readPointer({
          sender,
          args
        })
      )
    };
  }

  private press({ sender, reading, id }: {
    sender: PointerSender;
    reading: PointerReading;
    id: string;
  }) {
    const isIdle = this.gesture === null && this.draggedId === null;
    const ids = this.orderOf();
    const index = ids.indexOf(id);
    const canStart = isIdle && reading.isLeftButtonPressed && index >= 0;
    if (!canStart) {
      return;
    }

    capturePointer({
      sender,
      pointer: reading.pointer
    });
    this.gesture = {
      id,
      ids,
      index,
      heights: ids.map(rowId => this.rows.get(rowId)?.actualHeight ?? 0),
      startY: reading.y,
      pointer: reading.pointer,
      pointerId: reading.pointerId,
      source: sender,
      isDragging: false,
      target: index
    };
  }

  // The gesture the event belongs to: the one pointer that pressed the row.
  private gestureOf(reading: PointerReading) {
    const { gesture } = this;
    const isOwn = gesture !== null && reading.pointerId === gesture.pointerId;
    return isOwn ? gesture : null;
  }

  private move(reading: PointerReading) {
    const gesture = this.gestureOf(reading);
    if (gesture === null) {
      return;
    }

    const rawOffset = reading.y - gesture.startY;
    const isPressOnly = !gesture.isDragging && Math.abs(rawOffset) < DRAG_THRESHOLD;
    if (isPressOnly) {
      return;
    }

    if (!gesture.isDragging) {
      gesture.isDragging = true;
      this.draggedId = gesture.id;
    }

    const state = {
      heights: gesture.heights,
      index: gesture.index,
      offset: rawOffset
    };
    this.place({
      id: gesture.id,
      offset: clampedOffset(state)
    });
    const target = targetIndex(state);
    if (target !== gesture.target) {
      gesture.target = target;
      this.slideOthers();
    }
  }

  private release(reading: PointerReading) {
    const gesture = this.gestureOf(reading);
    if (gesture === null) {
      return;
    }

    this.gesture = null;
    releasePointer({
      sender: gesture.source,
      pointer: gesture.pointer
    });

    if (!gesture.isDragging) {
      return;
    }

    const slot = slotOffset({
      heights: gesture.heights,
      index: gesture.index,
      target: gesture.target
    });
    void this.glide({
      id: gesture.id,
      to: slot,
      feel: DROP_FEEL
    }).then(() => this.commit(gesture));
  }

  private cancel(reading: PointerReading) {
    const gesture = this.gestureOf(reading);
    if (gesture === null) {
      return;
    }

    this.gesture = null;

    if (!gesture.isDragging) {
      return;
    }

    const settled = gesture.ids.map(id => this.glide({
      id,
      to: 0,
      feel: SLIDE_FEEL
    }));
    void Promise.all(settled).then(() => {
      this.draggedId = null;
    });
  }

  private slideOthers() {
    const { gesture } = this;
    if (gesture === null) {
      return;
    }

    const slides = slideOffsets({
      heights: gesture.heights,
      index: gesture.index,
      target: gesture.target
    });
    for (const [iRow, id] of gesture.ids.entries()) {
      const isOther = id !== gesture.id;
      if (isOther) {
        void this.glide({
          id,
          to: slides[iRow] ?? 0,
          feel: SLIDE_FEEL
        });
      }
    }
  }

  // The new order and the end of every offset land in one frame: the rows are
  // reordered, and only then does each stand at its own place again.
  private async commit(gesture: Gesture) {
    const isMoved = gesture.target !== gesture.index;
    this.isCommitting = true;

    if (isMoved) {
      this.onCommit(
        movedIds({
          ids: gesture.ids,
          index: gesture.index,
          target: gesture.target
        })
      );
    }

    await tick();
    for (const id of gesture.ids) {
      this.place({
        id,
        offset: 0
      });
    }

    this.draggedId = null;
    this.isCommitting = false;
  }

  // The row is put at the offset at once, the way it follows the pointer.
  private place({ id, offset }: {
    id: string;
    offset: number;
  }) {
    void this.motions.get(id)?.set(offset, { instant: true });
  }

  // The row's spring is pulled to the offset; resolves when it has settled.
  private glide({ id, to, feel }: {
    id: string;
    to: number;
    feel: typeof SLIDE_FEEL;
  }) {
    const motion = this.motions.get(id);
    if (motion === undefined) {
      return Promise.resolve();
    }

    motion.stiffness = feel.stiffness;
    motion.damping = feel.damping;
    return motion.set(to);
  }
}

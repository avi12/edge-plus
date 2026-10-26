# Live tab hover preview

Make a hovered tab's card show a **live** miniature instead of a frozen snapshot,
by reusing Edge's own capture pipeline. Implemented in
`app/native-split/src/chrome/live_preview.h`; RVAs in `offsets.h`.

## What Edge already does

Hovering a tab opens a hover card that shows a thumbnail of the tab. That
thumbnail is produced by a **continuous** video capturer —
`BackgroundThumbnailVideoCapturer`, a viz `FrameSinkVideoCapturer` — and the card
**already repaints every frame it receives**:

```
hover
  -> TabHoverCardController::UpdateHoverCard
  -> ...::StartThumbnailObservation
  -> TabHoverCardThumbnailObserver::Observe  (ThumbnailImage::Subscribe)
  (capturer runs) BackgroundThumbnailVideoCapturer
     -> each frame: ThumbnailImage::AssignSkBitmap
     -> TabHoverCardController::OnPreviewImageAvailable   (per frame)
     -> TabHoverCardBubbleView::SetTargetTabImage         (per frame, card shows it)
```

So the plumbing for a live preview is **already present**. It looks static only
because Edge stops the capturer after the first good frame:

```
ThumbnailCaptureDriver::UpdateCaptureState
  -> ThumbnailTabHelper::TabStateTracker::StopCapture   (rva 0x2b06e90)
  -> BackgroundThumbnailVideoCapturer::Stop
```

## The change

Keep the capturer alive while a hover card is open. One detour does the work; two
more track whether a card is on screen so the capturer is held only then.

| hook (rva) | signature | role |
|---|---|---|
| `ThumbnailTabHelper::TabStateTracker::StopCapture` `0x2b06e90` | `void(this)` | while a card is open, **return without stopping** — the running capturer keeps delivering frames |
| `TabHoverCardThumbnailObserver::Observe` `0x8f5dd84` | `void(this, ThumbnailImage*)` | a card's thumbnail subscription begins → `g_isCardOpen = true`. Once per hover / tab-retarget (a bool, not a counter; only one card is open at a time). The **per-frame** callback is a different function, `ThumbnailImageCallback`, left alone |
| `TabHoverCardController::OnCardClosing` `0xa62c3e0` | `void(this)` | the card's definitive teardown → `g_isCardOpen = false`; Edge's next `StopCapture` then winds the capturer down normally |

Gated on the panel switch `live_preview::g_isActive` (config wire
`livePreviewActive`). Like `theme_follow` it rides its own switch, not the split,
so `control.h` re-enables it in `SetSplitHooksEnabled(false)`. None of the three
functions is identical-code-folded.

## Notes / risks

- **Background-tab cadence.** The hovered tab is usually a backgrounded
  WebContents; the FrameSinkVideoCapturer is what makes it composite at all, so an
  animated page (video, CSS) streams, a static page correctly stays still. How
  smooth a backgrounded tab's stream is cannot be settled statically — confirm by
  hovering a moving page.
- **Cost.** One small-resolution capture for the single hovered tab, only while
  hovering. Bounded.
- **Teardown.** `g_isCardOpen` is cleared on card close, on the feature turning
  off, and in `control.h`'s `Teardown`, so a stale flag never keeps a tab
  capturing; and on DLL unload the hook is removed, so Edge's own `StopCapture`
  resumes.
- **Frame rate.** `ThumbnailCapturer::SetMaxFrameRate` (0x557fe90) is ICF-folded
  with unrelated functions — do **not** hook it. The default cadence is used; a
  frame-rate bump would need the capturer-ctor path, a later pass.

## Fidelity upgrade (not done)

Mirroring the tab's compositor surface (a `SurfaceLayer` on the tab's `SurfaceId`)
into the card would give native frame-rate fidelity, but it is much more code
(layer lifetime, DPI, SurfaceId plumbing) and still needs capture to make a
background tab produce frames — so only worth it if the simple version
disappoints.

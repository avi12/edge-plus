namespace EdgePlus;

// The WM_NCHITTEST answers this cares about (winuser.h's HT* codes).
internal enum HitTest {
  Left = 10,
  Right = 11,
  Top = 12,
  TopLeft = 13,
  TopRight = 14,
  Bottom = 15,
  BottomLeft = 16,
  BottomRight = 17,
  Border = 18,
}

// The panel's width is fixed, so its side borders and corners must not offer a
// horizontal resize cursor either. Windows picks that cursor from the
// WM_NCHITTEST answer, which WindowSubclass rewrites with this: side borders
// become plain border, corners become the top / bottom edges.
internal static class ResizeHandles {
  public static int VerticalOnly(int hit) => (HitTest)hit switch {
    HitTest.Left or HitTest.Right => (int)HitTest.Border,
    HitTest.TopLeft or HitTest.TopRight => (int)HitTest.Top,
    HitTest.BottomLeft or HitTest.BottomRight => (int)HitTest.Bottom,
    _ => hit,
  };
}

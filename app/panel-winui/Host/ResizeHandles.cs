namespace EdgePlus;

// winuser.h's HT* codes, the WM_NCHITTEST answers.
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

// Windows picks the resize cursor from the WM_NCHITTEST answer, so rewriting
// it is how a fixed-width window stops offering a horizontal one.
internal static class ResizeHandles {
  public static int VerticalOnly(int hit) => (HitTest)hit switch {
    HitTest.Left or HitTest.Right => (int)HitTest.Border,
    HitTest.TopLeft or HitTest.TopRight => (int)HitTest.Top,
    HitTest.BottomLeft or HitTest.BottomRight => (int)HitTest.Bottom,
    _ => hit,
  };
}

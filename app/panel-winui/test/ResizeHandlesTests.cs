namespace EdgePlus;

internal static class ResizeHandlesTests {
  private const int Client = 1;
  private const int Caption = 2;

  public static void Run() {
    Expect.Equal((int)HitTest.Border, ResizeHandles.VerticalOnly((int)HitTest.Left), "left");
    Expect.Equal((int)HitTest.Border, ResizeHandles.VerticalOnly((int)HitTest.Right), "right");
    Expect.Equal((int)HitTest.Top, ResizeHandles.VerticalOnly((int)HitTest.TopLeft), "top left");
    Expect.Equal((int)HitTest.Top, ResizeHandles.VerticalOnly((int)HitTest.TopRight),
                 "top right");
    Expect.Equal((int)HitTest.Bottom, ResizeHandles.VerticalOnly((int)HitTest.BottomLeft),
                 "bottom left");
    Expect.Equal((int)HitTest.Bottom, ResizeHandles.VerticalOnly((int)HitTest.BottomRight),
                 "bottom right");
    foreach (int untouched in new[] { Client, Caption, (int)HitTest.Top, (int)HitTest.Bottom }) {
      Expect.Equal(untouched, ResizeHandles.VerticalOnly(untouched), $"{untouched} is untouched");
    }
  }
}

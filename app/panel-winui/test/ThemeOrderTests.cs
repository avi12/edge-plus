using System.Collections.Generic;
using System.Linq;

namespace EdgePlus;

internal static class ThemeOrderTests {
  public static void Run() {
    string[] ids = ["a", "b", "c"];
    Expect.Equal("b,a,c", Join(ThemeOrder.Moved(ids, "b", -1)), "up one");
    Expect.Equal("a,c,b", Join(ThemeOrder.Moved(ids, "b", 1)), "down one");
    Expect.Equal("a,b,c", Join(ThemeOrder.Moved(ids, "a", -1)), "the first stays first");
    Expect.Equal("a,b,c", Join(ThemeOrder.Moved(ids, "c", 1)), "the last stays last");
    Expect.Equal("b,c,a", Join(ThemeOrder.Moved(ids, "a", 10)), "a long move stops at the end");
    Expect.Equal("a,b,c", Join(ThemeOrder.Moved(ids, "x", 1)), "an unknown id moves nothing");

    List<ThemeEntry> themes = [Entry("a"), Entry("b"), Entry("c")];
    Expect.Equal("c,a,b", Join(ThemeOrder.Ordered(themes, ["c", "a", "b"]).Select(t => t.Id)),
                 "the library takes the dragged order");
    Expect.Equal("c,a,b", Join(ThemeOrder.Ordered(themes, ["c", "a"]).Select(t => t.Id)),
                 "a theme the order does not name keeps its place after the named ones");
    Expect.Equal("b,a,c", Join(ThemeOrder.Ordered(themes, ["gone", "b"]).Select(t => t.Id)),
                 "a removed theme in the order is ignored");
    Expect.That(ReferenceEquals(themes[0], ThemeOrder.Ordered(themes, ["a"])[0]),
                "the entries themselves are kept, not copied");

    var theming = new Theming { Themes = themes, Light = "b", Dark = "c" };
    theming.Reorder(["c", "b", "a"]);
    Expect.Equal("c,b,a", Join(theming.Themes.Select(t => t.Id)), "Theming reorders its themes");
    Expect.That(theming.Light == "b" && theming.Dark == "c", "a reorder keeps the assignments");
    var settings = new Settings { Theming = theming };
    Expect.That(settings.ToJson().IndexOf("\"c\"") < settings.ToJson().IndexOf("\"a\""),
                "the file's themes array is written in the new order");
  }

  private static ThemeEntry Entry(string id) => new() { Id = id, Name = id.ToUpperInvariant() };

  private static string Join(IEnumerable<string> ids) => string.Join(',', ids);
}

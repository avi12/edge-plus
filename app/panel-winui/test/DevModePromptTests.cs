using System;
using System.Text.Json.Nodes;

namespace EdgePlus;

internal static class DevModePromptTests {
  public static void Run() {
    var epoch = new DateTime(1601, 1, 1, 0, 0, 0, DateTimeKind.Utc);
    long hundredYears = 100L * 365 * 24 * 60 * 60 * 1_000_000;
    Expect.Equal(hundredYears.ToString(), DevModePrompt.SnoozeEnd(epoch),
                 "the snooze ends a hundred years on, in microseconds since 1601");

    var preferences = (JsonObject)JsonNode.Parse(
        """{"keep":1,"extensions":{"ui":"not an object","settings":{}}}""")!;
    DevModePrompt.SetSnooze(preferences, "42");
    Expect.Equal("42",
                 preferences["extensions"]?["ui"]?["dev_mode_warning_snooze_end_time"]
                     ?.GetValue<string>(),
                 "the snooze lands on its path, replacing what is not an object");
    Expect.That(preferences["extensions"]?["settings"] is JsonObject, "siblings are kept");
    Expect.Equal(1, preferences["keep"]?.GetValue<int>(), "the rest of the file is kept");

    var empty = new JsonObject();
    DevModePrompt.SetSnooze(empty, "7");
    Expect.Equal("7", empty["extensions"]?["ui"]?["dev_mode_warning_snooze_end_time"]
                          ?.GetValue<string>(),
                 "the path is created when missing");

    Expect.Equal("\"C:\\Program Files\\Edge+\\edgeplus.exe\" --quiet-dev-mode-after-exit",
                 DevModePrompt.WaiterCommandLine(@"C:\Program Files\Edge+\edgeplus.exe"),
                 "the waiter is this exe, quoted, with its switch");
  }
}

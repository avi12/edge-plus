namespace EdgePlus;

internal static class SwitchesTests {
  public static void Run() {
    string[] bootLaunch = ["edgeplus.exe", Switches.Background];
    Expect.That(Switches.IsPresent(bootLaunch, Switches.Background), "the boot switch is found");
    Expect.That(!Switches.IsPresent(["edgeplus.exe"], Switches.Background),
                "a plain launch has no boot switch");
    Expect.That(!Switches.IsPresent(bootLaunch, Switches.DevModeWaiter),
                "the boot switch is not the waiter's");

    string[] restart = ["edgeplus.exe", Switches.RestartedFrom, "4242"];
    Expect.Equal("4242", Switches.ValueOf(restart, Switches.RestartedFrom),
                 "a switch's value is the argument after it");
    Expect.That(Switches.ValueOf(bootLaunch, Switches.RestartedFrom) is null,
                "an absent switch has no value");
    Expect.That(Switches.ValueOf(["edgeplus.exe", Switches.RestartedFrom], Switches.RestartedFrom) is null,
                "a switch with nothing after it has none either");
  }
}

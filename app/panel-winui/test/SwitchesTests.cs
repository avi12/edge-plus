namespace EdgePlus;

internal static class SwitchesTests {
  public static void Run() {
    string[] bootLaunch = ["edgeplus.exe", Switches.Background];
    Expect.That(Switches.IsPresent(bootLaunch, Switches.Background), "the boot switch is found");
    Expect.That(!Switches.IsPresent(["edgeplus.exe"], Switches.Background),
                "a plain launch has no boot switch");
    Expect.That(!Switches.IsPresent(bootLaunch, Switches.DevModeWaiter),
                "the boot switch is not the waiter's");
  }
}

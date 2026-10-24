namespace EdgePlus;

// The window's Inject button, offered while auto-inject is off.
internal enum InjectOffer {
  // Not shown: auto-inject is on, Edge is closed, or no switch needs the DLL.
  Hidden,
  // Shown and enabled: Edge is open without the DLL.
  Ready,
  // Shown, disabled: the DLL went in after the button was offered. Stays until
  // Edge restarts without it (Ready again) or the offer ends (Hidden).
  Injected,
}

internal static class InjectOffers {
  // `previous` is what the window showed before this state arrived.
  public static InjectOffer Next(InjectOffer previous, PanelState state) {
    bool isOffered = !state.IsAutoInject && state.IsEdgeRunning && state.IsSplitWanted;
    if (!isOffered) {
      return InjectOffer.Hidden;
    }
    if (!state.IsSplitLoaded) {
      return InjectOffer.Ready;
    }
    return previous == InjectOffer.Hidden ? InjectOffer.Hidden : InjectOffer.Injected;
  }
}

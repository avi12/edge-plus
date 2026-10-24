using System;

namespace EdgePlus;

// The Inject button through a session: Edge opens without the DLL, the button
// injects, Edge restarts, auto-inject comes on, Edge closes.
internal static class InjectOfferTests {
  private static readonly FileVersion s_supported = new(155, 0, 4283, 62);

  public static void Run() {
    EdgeOpenWithoutTheDllOffersTheButton();
    AnInjectionShowsTheButtonDone();
    TheDoneButtonStaysUntilEdgeRestartsWithoutTheDll();
    AutoInjectOnHidesTheButton();
    EdgeClosedHidesTheButton();
    NothingWantedHidesTheButton();
    ADllAlreadyLoadedOffersNothing();
  }

  private static void EdgeOpenWithoutTheDllOffersTheButton() {
    Expect.Equal(InjectOffer.Ready, InjectOffers.Next(InjectOffer.Hidden, Missing()),
                 "Edge open without the DLL offers the button");
    Expect.Equal(InjectOffer.Ready, InjectOffers.Next(InjectOffer.Ready, Missing()),
                 "a refresh with the DLL still missing keeps it ready");
  }

  private static void AnInjectionShowsTheButtonDone() {
    Expect.Equal(InjectOffer.Injected, InjectOffers.Next(InjectOffer.Ready, Loaded()),
                 "the DLL going in after the offer shows the button done");
  }

  private static void TheDoneButtonStaysUntilEdgeRestartsWithoutTheDll() {
    Expect.Equal(InjectOffer.Injected, InjectOffers.Next(InjectOffer.Injected, Loaded()),
                 "a refresh with the DLL loaded keeps the button done");
    Expect.Equal(InjectOffer.Ready, InjectOffers.Next(InjectOffer.Injected, Missing()),
                 "Edge restarted without the DLL offers the button again");
  }

  private static void AutoInjectOnHidesTheButton() {
    foreach (InjectOffer previous in Enum.GetValues<InjectOffer>()) {
      Expect.Equal(InjectOffer.Hidden,
                   InjectOffers.Next(previous, Missing() with {
                     IsAutoInject = true
                   }),
                   $"auto-inject on hides a {previous} button though the DLL is missing");
      Expect.Equal(InjectOffer.Hidden,
                   InjectOffers.Next(previous, Loaded() with {
                     IsAutoInject = true
                   }),
                   $"auto-inject on hides a {previous} button once the DLL is in");
    }
  }

  private static void EdgeClosedHidesTheButton() {
    PanelState closed = Missing() with {
      IsEdgeRunning = false
    };
    foreach (InjectOffer previous in Enum.GetValues<InjectOffer>()) {
      Expect.Equal(InjectOffer.Hidden, InjectOffers.Next(previous, closed),
                   $"Edge closed hides a {previous} button");
    }
    Expect.Equal(InjectOffer.Ready, InjectOffers.Next(InjectOffer.Hidden, Missing()),
                 "Edge reopened without the DLL offers it again");
  }

  private static void NothingWantedHidesTheButton() {
    PanelState unwanted = Missing() with {
      IsSplitWanted = false
    };
    foreach (InjectOffer previous in Enum.GetValues<InjectOffer>()) {
      Expect.Equal(InjectOffer.Hidden, InjectOffers.Next(previous, unwanted),
                   $"no switch wanting the DLL hides a {previous} button");
    }
  }

  // The panel opened onto an Edge that already has the DLL: nothing to offer.
  private static void ADllAlreadyLoadedOffersNothing() {
    Expect.Equal(InjectOffer.Hidden, InjectOffers.Next(InjectOffer.Hidden, Loaded()),
                 "a DLL already loaded never shows the button");
  }

  // Auto-inject off, Edge open on a supported build, a switch wanting the DLL.
  private static PanelState Missing() =>
      new(new Features(), new Theming(), [], IsEdgeRunning: true, IsStartWithWindows: false,
          IsSplitLoaded: false, IsSplitWanted: true, IsAutoInject: false, s_supported,
          s_supported);

  private static PanelState Loaded() => Missing() with { IsSplitLoaded = true };
}

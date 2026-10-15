using System;
using CommunityToolkit.WinUI.Controls;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Windows.Graphics;

namespace FixEdge;

public sealed partial class MainWindow : Window {
  private const int WindowWidth = 540;
  private const int WindowHeight = 460;

  private const string NativeSplitGlyph = "";
  private const string QuietDevModeGlyph = "";

  public MainWindow() {
    InitializeComponent();
    WindowChrome.Apply(this, AppTitleBar);
    AppWindow.Resize(new SizeInt32(WindowWidth, WindowHeight));
    WindowChrome.CenterOnScreen(AppWindow);

    BuildFeatures();
  }

  private void BuildFeatures() {
    BuildNativeSplitCard();
    BuildThemingCard();
    BuildQuietDevModeCard();
    // Push the current desired state (both features + the theme assignment) to
    // an Edge that is already open, injecting the shared DLL if needed.
    SyncDll();
  }

  // The split and the theming both ride one injected DLL. This is the single
  // place that reconciles "what the user wants" with "what Edge is running":
  // the DLL is loaded while either feature is on, and its live behaviour is
  // pushed to it. Every switch and theme change ends here.
  private void SyncDll() {
    if (!NativeSplitPayload.IsCarried) {
      return;
    }
    bool split = FeatureSettings.IsEnabled(FeatureId.NativeSplit, isEnabledByDefault: true);
    bool theming = FeatureSettings.IsEnabled(FeatureId.Theming, isEnabledByDefault: false);
    FixEdgeBridge.Sync(split, theming, ThemeLibrary.Load());
  }

  // The native split is a DLL the panel loads into Edge itself: turning the
  // switch on injects it, off ejects it, live. It is the split now, so it is on
  // by default; a build without the DLL carried simply has no switch.
  private void BuildNativeSplitCard() {
    if (!NativeSplitPayload.IsCarried) {
      return;
    }
    bool isWanted = FeatureSettings.IsEnabled(FeatureId.NativeSplit, isEnabledByDefault: true);
    AddSwitchCard(
        "Split into any grid",
        "Grows Edge's own split past two panes, loaded straight into Edge. Add panes above, "
            + "below, left or right; drag the dividers; Shift+Alt+arrow moves between panes. "
            + "Turning it on injects the module, off removes it — no restart. Open Edge first.",
        NativeSplitGlyph,
        isOn: NativeSplit.IsActive() || isWanted,
        onToggled: isOn => {
          FeatureSettings.SetEnabled(FeatureId.NativeSplit, isOn);
          SyncDll();
        });
  }

  // The theming feature (its own switch, library and light/dark assignment) is
  // built by ThemingCard; it shares the split's DLL, so it needs that carried.
  private void BuildThemingCard() {
    if (!NativeSplitPayload.IsCarried) {
      return;
    }
    IntPtr windowHandle = WinRT.Interop.WindowNative.GetWindowHandle(this);
    var card = new ThemingCard(windowHandle, SyncDll);
    FeatureList.Children.Add(card.Build());
  }

  // Hiding the developer-mode prompt lives entirely in the app: arming the
  // after-exit waiter when the switch turns on (DevModePrompt).
  private void BuildQuietDevModeCard() {
    AddSwitchCard(
        "Hide the developer-mode prompt",
        "Stops Edge asking to turn off developer-mode extensions when it starts. Takes effect "
            + "from Edge's next start.",
        QuietDevModeGlyph,
        isOn: FeatureSettings.IsEnabled(FeatureId.QuietDevMode, isEnabledByDefault: false),
        onToggled: isOn => {
          FeatureSettings.SetEnabled(FeatureId.QuietDevMode, isOn);
          if (isOn) {
            DevModePrompt.Arm();
          }
        });
  }

  // The whole row flips the switch, as in Windows' own settings.
  private void AddSwitchCard(
      string header, string description, string glyph, bool isOn, Action<bool> onToggled) {
    var toggle = new ToggleSwitch {
      OnContent = "",
      OffContent = "",
      MinWidth = 0,
      IsOn = isOn,
    };
    toggle.Toggled += (_, _) => onToggled(toggle.IsOn);
    var card = new SettingsCard {
      Header = header,
      Description = description,
      HeaderIcon = new FontIcon { Glyph = glyph },
      IsClickEnabled = true,
      IsActionIconVisible = false,
      Content = toggle,
    };
    card.Click += (_, _) => toggle.IsOn = !toggle.IsOn;
    FeatureList.Children.Add(card);
  }
}

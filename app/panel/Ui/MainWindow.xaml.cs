using System;
using System.Diagnostics;
using CommunityToolkit.WinUI.Controls;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Windows.ApplicationModel.DataTransfer;
using Windows.Graphics;

namespace FixEdge;

public sealed partial class MainWindow : Window {
  private const int WindowWidth = 540;
  private const int WindowHeight = 560;

  private const string ExtensionsPage = "edge://extensions";

  private const string ConnectedGlyph = "\uE73E";
  private const string DisconnectedGlyph = "\uE946";
  private const string NativeSplitGlyph = "\uF0E2";

  // The host rewrites the status the moment a pane opens or closes; a second
  // is quick enough to read as live and costs one file read.
  private static readonly TimeSpan s_statusPollInterval = TimeSpan.FromSeconds(1);

  private readonly DispatcherTimer _statusPoll = new() { Interval = s_statusPollInterval };

  public MainWindow() {
    InitializeComponent();
    WindowChrome.Apply(this, AppTitleBar);
    AppWindow.Resize(new SizeInt32(WindowWidth, WindowHeight));
    WindowChrome.CenterOnScreen(AppWindow);

    BuildFeatureList();
    BuildNativeSplitCard();
    ExtensionFolder.Text = Paths.Extension;
    RefreshConnection();
    _statusPoll.Tick += (_, _) => RefreshConnection();
    _statusPoll.Start();
    Closed += (_, _) => _statusPoll.Stop();
  }

  // A row per catalog feature: its words, its icon and one switch. A new
  // feature in the extension shows up here with no change to the app.
  private void BuildFeatureList() {
    foreach (Feature feature in Catalog.Features) {
      AddSwitchCard(
          feature.Title, feature.Description, feature.Glyph,
          isOn: FeatureSettings.IsEnabled(feature.Id),
          onToggled: isOn => {
            FeatureSettings.SetEnabled(feature.Id, isOn);
            // The one feature that lives outside Edge: it waits for Edge to close.
            bool isQuietDevModeTurnedOn =
                feature.Id == FeatureId.QuietDevMode.WireName() && isOn;
            if (isQuietDevModeTurnedOn) {
              DevModePrompt.Arm();
            }
          });
    }
  }

  // The native split is not an extension feature: it is a DLL the panel loads
  // into Edge itself, so it gets its own switch that injects or ejects live.
  private void BuildNativeSplitCard() {
    if (!NativeSplitPayload.IsCarried) {
      return;
    }
    bool isWanted = FeatureSettings.IsEnabled(FeatureId.NativeSplit, isEnabledByDefault: false);
    // Re-assert the wanted state against the Edge running now.
    bool shouldInjectIntoRunningEdge =
        isWanted && NativeSplit.IsEdgeRunning && !NativeSplit.IsActive();
    if (shouldInjectIntoRunningEdge) {
      NativeSplit.Enable(Paths.SplitDll);
    }
    AddSwitchCard(
        "Native split into any grid (experimental)",
        "Grows Edge's own split past two panes, loaded straight into Edge - no extension. "
            + "Turning it on injects the module; off removes it, no restart. Open Edge first.",
        NativeSplitGlyph,
        isOn: NativeSplit.IsActive() || isWanted,
        onToggled: isOn => {
          FeatureSettings.SetEnabled(FeatureId.NativeSplit, isOn);
          if (isOn) {
            NativeSplit.Enable(Paths.SplitDll);
          } else {
            NativeSplit.Disable();
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

  private void RefreshConnection() {
    bool isConnected = EdgeConnection.IsConnected;
    SetupNotice.IsOpen = !isConnected;
    ConnectionIcon.Glyph = isConnected ? ConnectedGlyph : DisconnectedGlyph;
    ConnectionCard.Description =
        isConnected ? DescribeConnected() : "The extension is not running in Edge";
  }

  private static string DescribeConnected() {
    SplitStatus? split = EdgeConnection.Split();
    bool hasSplits = split is { Splits: > 0 };
    if (!hasSplits) {
      return "Connected - switches apply instantly";
    }
    string splits = split!.Splits == 1 ? "1 split tab" : $"{split.Splits} split tabs";
    return $"Connected - {split.Panes} panes across {splits}";
  }

  private void OnOpenExtensions(object sender, RoutedEventArgs e) =>
      Process.Start(new ProcessStartInfo(EdgeBrowser.ExecutableName, ExtensionsPage) {
        UseShellExecute = true,
      });

  private void OnCopyFolder(object sender, RoutedEventArgs e) {
    var package = new DataPackage();
    package.SetText(Paths.Extension);
    Clipboard.SetContent(package);
  }
}

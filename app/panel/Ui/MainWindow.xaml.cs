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
  private const string EdgeExecutable = "msedge";

  private const string ConnectedGlyph = "";
  private const string DisconnectedGlyph = "";

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
      var toggle = new ToggleSwitch {
        OnContent = "",
        OffContent = "",
        MinWidth = 0,
        IsOn = FeatureSettings.IsEnabled(feature.Id),
      };
      toggle.Toggled += (_, _) => FeatureSettings.SetEnabled(feature.Id, toggle.IsOn);
      var card = new SettingsCard {
        Header = feature.Title,
        Description = feature.Description,
        HeaderIcon = new FontIcon { Glyph = feature.Glyph },
        IsClickEnabled = true,
        IsActionIconVisible = false,
        Content = toggle,
      };
      // The whole row flips the switch, as in Windows' own settings.
      card.Click += (_, _) => toggle.IsOn = !toggle.IsOn;
      FeatureList.Children.Add(card);
    }
  }

  private void RefreshConnection() {
    bool isConnected = EdgeConnection.IsConnected;
    SetupNotice.IsOpen = !isConnected;
    ConnectionIcon.Glyph = isConnected ? ConnectedGlyph : DisconnectedGlyph;
    ConnectionCard.Description = isConnected ? DescribeConnected() : "The extension is not running in Edge";
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
      Process.Start(new ProcessStartInfo(EdgeExecutable, ExtensionsPage) { UseShellExecute = true });

  private void OnCopyFolder(object sender, RoutedEventArgs e) {
    var package = new DataPackage();
    package.SetText(Paths.Extension);
    Clipboard.SetContent(package);
  }
}

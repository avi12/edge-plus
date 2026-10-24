using System;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace EdgePlus;

// A release that is downloaded and checked, with the button that restarts Edge+
// into it. Hidden until one is ready.
internal sealed class UpdateCard {
  private readonly Button _button;
  private readonly CardFrame _frame;

  public UpdateCard() {
    _button = new Button {
      Content = Wording.UpdateButton,
      Style = StyleKeys.Get(StyleKeys.AccentButton),
    };
    // Disabled at once, so a second click cannot start a second restart.
    _button.Click += (_, _) => {
      _button.IsEnabled = false;
      _button.Content = Wording.UpdateRestarting;
      Clicked?.Invoke();
    };
    _frame = new CardFrame(Wording.UpdateReady(new Version(0, 0)), _button);
    _frame.View.Visibility = Visibility.Collapsed;
  }

  public event Action? Clicked;

  public FrameworkElement View => _frame.View;

  public void Show(Version version) {
    _frame.SetWording(Wording.UpdateReady(version));
    Reset();
    _frame.View.Visibility = Visibility.Visible;
  }

  // The restart failed: the button comes back, so it can be tried again.
  public void Reset() {
    _button.Content = Wording.UpdateButton;
    _button.IsEnabled = true;
  }
}

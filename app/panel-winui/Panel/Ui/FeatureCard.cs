using System;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;

namespace EdgePlus;

// A card with a switch at its end.
internal sealed class FeatureCard {
  private readonly ToggleSwitch _toggle;
  private readonly CardFrame _frame;
  // So Update's write to the switch is not taken for a click.
  private bool _isUpdating;

  public FeatureCard(CardWording wording) {
    _toggle = new ToggleSwitch {
      OnContent = "",
      OffContent = "",
      MinWidth = 0,
    };
    AutomationProperties.SetName(_toggle, wording.Title);
    _toggle.Toggled += (_, _) => {
      if (!_isUpdating) {
        Toggled?.Invoke(_toggle.IsOn);
      }
    };
    _frame = new CardFrame(wording, _toggle);
  }

  public event Action<bool>? Toggled;

  public FrameworkElement View => _frame.View;

  public void Update(bool isOn, bool isLocked, string note) {
    _isUpdating = true;
    _toggle.IsOn = isOn;
    _isUpdating = false;
    _toggle.IsEnabled = !isLocked;
    _frame.SetNote(note);
  }

  public void SetBody(FrameworkElement body) => _frame.SetBody(body);
}

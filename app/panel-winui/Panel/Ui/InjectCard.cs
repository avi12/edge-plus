using System;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace EdgePlus;

// The Inject button, as a card under the Inject automatically switch. When it
// is shown, and whether it can be pressed, is InjectOffers' decision.
internal sealed class InjectCard {
  private readonly Button _button;
  private readonly CardFrame _frame;

  public InjectCard() {
    _button = new Button {
      Content = Wording.InjectButton,
      Style = StyleKeys.Get(StyleKeys.AccentButton),
    };
    // Disabled at once, so a second click cannot queue a second injection.
    _button.Click += (_, _) => {
      _button.IsEnabled = false;
      Clicked?.Invoke();
    };
    _frame = new CardFrame(Wording.InjectReady, _button);
  }

  public event Action? Clicked;

  public FrameworkElement View => _frame.View;

  public void Update(InjectOffer offer) {
    bool isReady = offer == InjectOffer.Ready;
    _frame.SetWording(isReady ? Wording.InjectReady : Wording.Injected);
    _button.IsEnabled = isReady;
  }
}

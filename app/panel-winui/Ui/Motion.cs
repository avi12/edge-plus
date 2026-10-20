using System;
using System.Collections.Generic;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media.Animation;

namespace EdgePlus;

// The panel's motion, and the one place that honours Windows' "Animation
// effects" switch: a panel registered here gets WinUI's own Fluent theme
// transitions while the switch is on and none while it is off, so adding or
// removing a theme is then instant. Refresh re-reads the switch, so a change
// in Windows Settings takes effect live.
internal static class Motion {
  private static readonly List<(Panel Panel, Func<TransitionCollection> Transitions)> s_panels = [];
  private static bool s_isOn = SystemAnimations.IsOn();

  // Children entering and leaving slide and fade in place, and the siblings
  // after them move up or down to make room (a theme row added or removed).
  public static TransitionCollection AddDelete() => [new AddDeleteThemeTransition()];

  // Children that move because something above them grew or shrank glide to
  // their new place (the cards under the theming card as its list changes).
  public static TransitionCollection Reposition() => [new RepositionThemeTransition()];

  public static void Animate(Panel panel, Func<TransitionCollection> transitions) {
    s_panels.Add((panel, transitions));
    Apply(panel, transitions);
  }

  // Must run on the UI thread.
  public static void Refresh() {
    bool isOn = SystemAnimations.IsOn();
    if (isOn == s_isOn) {
      return;
    }
    s_isOn = isOn;
    foreach ((Panel panel, Func<TransitionCollection> transitions) in s_panels) {
      Apply(panel, transitions);
    }
  }

  private static void Apply(Panel panel, Func<TransitionCollection> transitions) =>
      panel.ChildrenTransitions = s_isOn ? transitions() : [];
}

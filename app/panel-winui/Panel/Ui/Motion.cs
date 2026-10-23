using System;
using System.Collections.Generic;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media.Animation;

namespace EdgePlus;

// The one place that honours Windows' "Animation effects" switch: a panel
// registered here gets WinUI's theme transitions only while it is on, and
// Refresh re-reads it live.
internal static class Motion {
  private static readonly List<(Panel Panel, Func<TransitionCollection> Transitions)> s_panels = [];
  private static readonly List<(ListViewBase List, Func<TransitionCollection> Transitions)>
      s_lists = [];
  private static bool s_isOn = SystemAnimations.IsOn();

  public static TransitionCollection AddDelete() => [new AddDeleteThemeTransition()];

  public static TransitionCollection Reposition() => [new RepositionThemeTransition()];

  public static TransitionCollection ListItems() =>
      [new AddDeleteThemeTransition(), new ReorderThemeTransition(),
       new RepositionThemeTransition()];

  public static void Animate(Panel panel, Func<TransitionCollection> transitions) {
    s_panels.Add((panel, transitions));
    Apply(panel, transitions);
  }

  public static void AnimateItems(ListViewBase list, Func<TransitionCollection> transitions) {
    s_lists.Add((list, transitions));
    ApplyItems(list, transitions);
  }

  public static void Forget() {
    s_panels.Clear();
    s_lists.Clear();
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
    foreach ((ListViewBase list, Func<TransitionCollection> transitions) in s_lists) {
      ApplyItems(list, transitions);
    }
  }

  private static void Apply(Panel panel, Func<TransitionCollection> transitions) =>
      panel.ChildrenTransitions = s_isOn ? transitions() : [];

  private static void ApplyItems(ListViewBase list, Func<TransitionCollection> transitions) =>
      list.ItemContainerTransitions = s_isOn ? transitions() : [];
}

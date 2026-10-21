using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;

namespace EdgePlus;

// The rows the cards' bodies are made of, built one way for every card.
internal static class CardRows {
  private const double ColumnGap = 8;
  // A picker wide enough for the longest choice it offers.
  public const double PickerMinWidth = 200;

  // A picker for one Windows mode ("When Windows is light"), the label on the
  // left, the picker on the right; the picker is named after the label for a
  // screen reader unless it has a name already.
  public static Grid Picker(string label, ComboBox picker) {
    var row = new Grid { ColumnSpacing = ColumnGap };
    row.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
    row.ColumnDefinitions.Add(new ColumnDefinition { Width = GridLength.Auto });
    row.Children.Add(new TextBlock {
      Text = label,
      Style = StyleKeys.Get(StyleKeys.CardTitle),
      VerticalAlignment = VerticalAlignment.Center,
    });
    picker.MinWidth = PickerMinWidth;
    bool isUnnamed = string.IsNullOrEmpty(AutomationProperties.GetName(picker));
    if (isUnnamed) {
      AutomationProperties.SetName(picker, label);
    }
    Grid.SetColumn(picker, 1);
    row.Children.Add(picker);
    return row;
  }
}

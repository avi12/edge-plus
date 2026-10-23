using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Automation;
using Microsoft.UI.Xaml.Controls;

namespace EdgePlus;

internal static class CardRows {
  private const double ColumnGap = 8;
  // Wide enough for the longest choice a picker offers.
  public const double PickerMinWidth = 200;

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

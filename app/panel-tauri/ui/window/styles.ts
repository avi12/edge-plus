import type { Style } from "./winui.ts";
import { Application, projectAs, PropertyValue, ResourceDictionary, Style as StyleType, XamlReader } from "./winui.ts";

// The window's styles, parsed at start and merged into the application's
// resources over WinUI's own (XamlControlsResources). Every colour is a
// ThemeResource, so light and dark follow Windows live.
const resourcesXaml = `
<ResourceDictionary
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
    <Style x:Key="FeatureCardStyle" TargetType="Border">
        <Setter Property="Background" Value="{ThemeResource CardBackgroundFillColorDefaultBrush}"/>
        <Setter Property="BorderBrush" Value="{ThemeResource CardStrokeColorDefaultBrush}"/>
        <Setter Property="BorderThickness" Value="1"/>
        <Setter Property="CornerRadius" Value="{ThemeResource ControlCornerRadius}"/>
        <Setter Property="Padding" Value="16,12,16,12"/>
        <Setter Property="MinHeight" Value="68"/>
    </Style>
    <Style x:Key="CardTitleStyle" TargetType="TextBlock" BasedOn="{StaticResource BodyTextBlockStyle}">
        <Setter Property="TextWrapping" Value="Wrap"/>
    </Style>
    <Style x:Key="CardDescriptionStyle" TargetType="TextBlock" BasedOn="{StaticResource CaptionTextBlockStyle}">
        <Setter Property="Foreground" Value="{ThemeResource TextFillColorSecondaryBrush}"/>
        <Setter Property="TextWrapping" Value="Wrap"/>
    </Style>
    <Style x:Key="CardNoteStyle" TargetType="TextBlock" BasedOn="{StaticResource CaptionTextBlockStyle}">
        <Setter Property="Foreground" Value="{ThemeResource SystemFillColorCautionBrush}"/>
        <Setter Property="TextWrapping" Value="Wrap"/>
    </Style>
    <Style x:Key="CardErrorStyle" TargetType="TextBlock" BasedOn="{StaticResource CaptionTextBlockStyle}">
        <Setter Property="Foreground" Value="{ThemeResource SystemFillColorCriticalBrush}"/>
        <Setter Property="TextWrapping" Value="Wrap"/>
    </Style>
    <Style x:Key="BadgeStyle" TargetType="Border">
        <Setter Property="Background" Value="{ThemeResource SubtleFillColorSecondaryBrush}"/>
        <Setter Property="CornerRadius" Value="{ThemeResource ControlCornerRadius}"/>
        <Setter Property="Padding" Value="6,1,6,1"/>
        <Setter Property="VerticalAlignment" Value="Center"/>
    </Style>
    <!-- The row draws its own padding, so it lines up with Edge's own themes
         listed under it. -->
    <Style x:Key="ThemeListItemStyle" TargetType="ListViewItem"
           BasedOn="{StaticResource DefaultListViewItemStyle}">
        <Setter Property="Padding" Value="0"/>
        <Setter Property="MinHeight" Value="36"/>
        <Setter Property="HorizontalContentAlignment" Value="Stretch"/>
    </Style>
    <Style x:Key="DropVeilStyle" TargetType="Grid">
        <Setter Property="Background" Value="{ThemeResource SolidBackgroundFillColorBaseBrush}"/>
    </Style>
    <Style x:Key="DropVeilFrameStyle" TargetType="Rectangle">
        <Setter Property="Stroke" Value="{ThemeResource AccentFillColorDefaultBrush}"/>
        <Setter Property="StrokeThickness" Value="2"/>
    </Style>
    <Style x:Key="BadgeTextStyle" TargetType="TextBlock" BasedOn="{StaticResource CaptionTextBlockStyle}">
        <Setter Property="Foreground" Value="{ThemeResource TextFillColorSecondaryBrush}"/>
    </Style>
</ResourceDictionary>`;

// Each is an x:Key above, or one of WinUI's own.
export const StyleKey = {
  Card: "FeatureCardStyle",
  CardTitle: "CardTitleStyle",
  CardDescription: "CardDescriptionStyle",
  CardNote: "CardNoteStyle",
  CardError: "CardErrorStyle",
  Badge: "BadgeStyle",
  BadgeText: "BadgeTextStyle",
  ThemeListItem: "ThemeListItemStyle",
  DropVeil: "DropVeilStyle",
  DropVeilFrame: "DropVeilFrameStyle",
  AccentButton: "AccentButtonStyle",
  // As Windows Settings sets a section header.
  SectionHeader: "BodyStrongTextBlockStyle",
  Caption: "CaptionTextBlockStyle"
} as const;
type StyleKey = (typeof StyleKey)[keyof typeof StyleKey];

// After Application.create has installed WinUI's own resources, which the
// styles above are based on.
export function installStyles(): void {
  const dictionary = projectAs(XamlReader.load(resourcesXaml), ResourceDictionary);
  Application.current.resources.mergedDictionaries?.append(dictionary);
}

export function style(key: StyleKey): Style {
  return projectAs(Application.current.resources.lookup(PropertyValue.createString(key)), StyleType);
}

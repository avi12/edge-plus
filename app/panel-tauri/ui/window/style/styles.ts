import type { Style } from "../winui.ts";
import {
  Application,
  projectAs,
  PropertyValue,
  ResourceDictionary,
  Style as StyleType,
  XamlReader
} from "../winui.ts";

// The one Style the window keeps in XAML. Every other look is a `<style>` in
// the component that uses it (or ui/window/style/SharedStyles.svelte), but this one
// is a ListView's item container style, set from TypeScript, and it needs
// HorizontalContentAlignment, which CSS has no property for (justify-content
// is a child's own alignment in WinUI). Light and dark follow Windows live.
const themeListItemKey = "ThemeListItemStyle";
const resourcesXaml = `
<ResourceDictionary
    xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
    xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
    <!-- The row draws its own padding, so it lines up with Edge's own themes
         listed under it. -->
    <Style x:Key="${themeListItemKey}" TargetType="ListViewItem"
           BasedOn="{StaticResource DefaultListViewItemStyle}">
        <Setter Property="Padding" Value="0"/>
        <Setter Property="MinHeight" Value="36"/>
        <Setter Property="HorizontalContentAlignment" Value="Stretch"/>
    </Style>
</ResourceDictionary>`;

// After Application.create has installed WinUI's own resources, which the
// style above is based on.
export function installStyles(): void {
  const dictionary = projectAs(XamlReader.load(resourcesXaml), ResourceDictionary);
  Application.current.resources.mergedDictionaries?.append(dictionary);
}

export function themeListItemStyle(): Style {
  return projectAs(Application.current.resources.lookup(PropertyValue.createString(themeListItemKey)), StyleType);
}

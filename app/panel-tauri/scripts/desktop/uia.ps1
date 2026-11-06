# Does one thing to a control of an example's window, the way a user or an
# assistive tool does, through UI Automation: the window is found by the pid
# we started (never by its title), the control by its AutomationId. The result
# is one line of JSON on stdout.
#
#   powershell -NoProfile -File uia.ps1 -ProcessId <pid> -Action <action> `
#     [-Id <automation id>] [-Name <name>] [-Value <text>] [-Item <name>]
#
# A control with no AutomationId is found by -Name instead.
# Actions: read, exists, names, toggles (every toggle: id, name, state),
# list-items (every list item: name, height, inset of its content), pixel (the color Windows drew in the control, at -Value of its width,
# 0.5 by default), invoke, toggle, set-value, set-range, select,
# select-item (a combo box: opens it and picks the item named by -Item),
# focus. No action sends keys or moves the mouse: every one is a UI
# Automation pattern call, so the desktop of whoever runs it stays untouched.
param(
  [Parameter(Mandatory)][int]$ProcessId,
  [Parameter(Mandatory)][string]$Action,
  [string]$Id = '',
  [string]$Name = '',
  [string]$Value = '',
  [string]$Item = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
. "$PSScriptRoot\window-tools.ps1"

# The records below go to test/uia.ts as JSON: their property names are the
# JSON keys, so they stay lower case.

# A toggle control, as the "toggles" action lists it.
class ToggleInfo {
  [string] $id
  [string] $name
  [string] $toggle
  [bool] $enabled

  ToggleInfo([string] $id, [string] $name, [string] $toggle, [bool] $enabled) {
    $this.id = $id
    $this.name = $name
    $this.toggle = $toggle
    $this.enabled = $enabled
  }
}

# A list item, as the "list-items" action lists it; inset is -1 when it has no content.
class ListItemInfo {
  [string] $name
  [double] $height
  [double] $inset

  ListItemInfo([string] $name, [double] $height, [double] $inset) {
    $this.name = $name
    $this.height = $height
    $this.inset = $inset
  }
}

# The color Windows drew at one point of the control, as the "pixel" action reports it.
class PixelInfo {
  [string] $color
  [int] $x
  [int] $y

  PixelInfo([string] $color, [int] $x, [int] $y) {
    $this.color = $color
    $this.x = $x
    $this.y = $y
  }
}

[type] $automation = [System.Windows.Automation.AutomationElement]
[int] $pauseMilliseconds = 150
[double] $defaultPixelFraction = 0.5
[int] $noContentInset = -1

function Find-Window {
  [OutputType([System.Windows.Automation.AutomationElement])]
  param()
  [System.Windows.Automation.PropertyCondition] $condition = New-Object System.Windows.Automation.PropertyCondition(
    $automation::ProcessIdProperty, $ProcessId)
  # A TextBox makes Windows add a text input indicator window to the process,
  # which can be listed first: the example's own window is the WinUI one.
  [System.Windows.Automation.AutomationElementCollection] $windows = $automation::RootElement.FindAll('Children', $condition)
  [System.Windows.Automation.AutomationElement] $window = $windows | Where-Object {
    $_.Current.ClassName -eq 'WinUIDesktopWin32WindowClass' } |
    Select-Object -First 1
  if ($null -eq $window) {
    throw 'the example has no window'
  }
  return $window
}

function Find-Control {
  [OutputType([System.Windows.Automation.AutomationElement])]
  param([string] $AutomationId)
  [System.Windows.Automation.AutomationProperty] $property = $automation::AutomationIdProperty
  [string] $wanted = $AutomationId
  if ($Name -ne '') {
    $property = $automation::NameProperty
    $wanted = $Name
  }
  [System.Windows.Automation.PropertyCondition] $condition = New-Object System.Windows.Automation.PropertyCondition(
    $property, $wanted)
  [System.Windows.Automation.AutomationElement] $found = (Find-Window).FindFirst('Descendants', $condition)
  if ($null -eq $found) {
    throw "no control '$wanted'"
  }
  return $found
}

function Get-Pattern {
  [OutputType([object])]
  param(
    [System.Windows.Automation.AutomationElement] $Element,
    [System.Windows.Automation.AutomationPattern] $Pattern
  )
  [object] $object = $null
  if (-not $Element.TryGetCurrentPattern($Pattern, [ref]$object)) {
    throw "the control '$Id' does not support $($Pattern.ProgrammaticName)"
  }
  return $object
}

# Only the patterns the control supports add a key.
function Read-Control {
  [OutputType([Collections.Specialized.OrderedDictionary])]
  param([System.Windows.Automation.AutomationElement] $Element)
  [Collections.Specialized.OrderedDictionary] $state = [ordered]@{ name = $Element.Current.Name; enabled = $Element.Current.IsEnabled }
  [object] $object = $null
  [System.Windows.Automation.AutomationPattern] $toggle = [System.Windows.Automation.TogglePattern]::Pattern
  if ($Element.TryGetCurrentPattern($toggle, [ref]$object)) {
    $state.toggle = $object.Current.ToggleState.ToString()
  }
  [System.Windows.Automation.AutomationPattern] $range = [System.Windows.Automation.RangeValuePattern]::Pattern
  if ($Element.TryGetCurrentPattern($range, [ref]$object)) {
    $state.range = $object.Current.Value
  }
  [System.Windows.Automation.AutomationPattern] $text = [System.Windows.Automation.ValuePattern]::Pattern
  if ($Element.TryGetCurrentPattern($text, [ref]$object)) {
    $state.value = $object.Current.Value
  }
  [System.Windows.Automation.AutomationPattern] $item = [System.Windows.Automation.SelectionItemPattern]::Pattern
  if ($Element.TryGetCurrentPattern($item, [ref]$object)) {
    $state.selected = $object.Current.IsSelected
  }
  return $state
}

function Select-ComboItem {
  [OutputType([void])]
  param(
    [System.Windows.Automation.AutomationElement] $Element,
    [string] $ItemName
  )
  $expand = Get-Pattern $Element (
    [System.Windows.Automation.ExpandCollapsePattern]::Pattern)
  $expand.Expand()
  Start-Sleep -Milliseconds $pauseMilliseconds
  [System.Windows.Automation.PropertyCondition] $condition = New-Object System.Windows.Automation.PropertyCondition(
    $automation::NameProperty, $ItemName)
  [System.Windows.Automation.AutomationElement] $entry = $automation::RootElement.FindFirst('Descendants', $condition)
  if ($null -eq $entry) {
    throw "the combo box has no item '$ItemName'"
  }
  (Get-Pattern $entry (
    [System.Windows.Automation.SelectionItemPattern]::Pattern)).Select()
  Start-Sleep -Milliseconds $pauseMilliseconds
}

function Invoke-Action {
  [OutputType([object])]
  param()
  if ($Action -eq 'names') {
    [System.Windows.Automation.AutomationElement] $scope = if ($Id -eq '') { Find-Window } else { Find-Control $Id }
    [System.Windows.Automation.AutomationElementCollection] $all = $scope.FindAll('Descendants',
      [System.Windows.Automation.Condition]::TrueCondition)
    return @($all | ForEach-Object { $_.Current.Name } | Where-Object { $_ -ne '' })
  }
  if ($Action -eq 'toggles') {
    [System.Windows.Automation.AutomationPattern] $pattern = [System.Windows.Automation.TogglePattern]::Pattern
    [System.Windows.Automation.AutomationElementCollection] $all = (Find-Window).FindAll('Descendants', [System.Windows.Automation.Condition]::TrueCondition)
    [Collections.Generic.List[ToggleInfo]] $toggles = [Collections.Generic.List[ToggleInfo]]::new()
    foreach ($element in $all) {
      [object] $object = $null
      if ($element.TryGetCurrentPattern($pattern, [ref]$object)) {
        $toggles.Add([ToggleInfo]::new($element.Current.AutomationId, $element.Current.Name,
          $object.Current.ToggleState.ToString(), $element.Current.IsEnabled))
      }
    }
    return $toggles
  }
  if ($Action -eq 'list-items') {
    [System.Windows.Automation.PropertyCondition] $type = New-Object System.Windows.Automation.PropertyCondition(
      $automation::ControlTypeProperty, [System.Windows.Automation.ControlType]::ListItem)
    [System.Windows.Automation.AutomationElementCollection] $items = (Find-Window).FindAll('Descendants', $type)
    [Collections.Generic.List[ListItemInfo]] $listItems = [Collections.Generic.List[ListItemInfo]]::new()
    foreach ($listItem in $items) {
      $rect = $listItem.Current.BoundingRectangle
      [System.Windows.Automation.AutomationElement] $child = $listItem.FindFirst('Descendants', [System.Windows.Automation.Condition]::TrueCondition)
      [double] $inset = if ($null -eq $child) { $noContentInset } else { $child.Current.BoundingRectangle.X - $rect.X }
      $listItems.Add([ListItemInfo]::new($listItem.Current.Name, $rect.Height, $inset))
    }
    return $listItems
  }
  if ($Action -eq 'pixel') {
    [System.Windows.Automation.AutomationElement] $element = Find-Control $Id
    $rect = $element.Current.BoundingRectangle
    [IntPtr] $hwnd = [ExampleWin]::Find([uint32]$ProcessId)
    [System.Drawing.Bitmap] $bitmap = [ExampleWin]::Capture($hwnd)
    $window = [ExampleWin]::Rect($hwnd)
    [double] $fraction = if ($Value -ne '') { [double]$Value } else { $defaultPixelFraction }
    [int] $x = [int]($rect.X + $rect.Width * $fraction - $window.L)
    [int] $y = [int]($rect.Y + $rect.Height / 2 - $window.T)
    [System.Drawing.Color] $color = $bitmap.GetPixel($x, $y)
    return [PixelInfo]::new(('{0:X2}{1:X2}{2:X2}' -f $color.R, $color.G, $color.B), $x, $y)
  }
  if ($Action -eq 'exists') {
    [System.Windows.Automation.PropertyCondition] $condition = New-Object System.Windows.Automation.PropertyCondition(
      $automation::AutomationIdProperty, $Id)
    return $null -ne (Find-Window).FindFirst('Descendants', $condition)
  }
  [System.Windows.Automation.AutomationElement] $element = Find-Control $Id
  switch ($Action) {
    'read' {
      return Read-Control $element
    }
    'invoke' {
      (Get-Pattern $element (
        [System.Windows.Automation.InvokePattern]::Pattern)).Invoke()
    }
    'toggle' {
      (Get-Pattern $element (
        [System.Windows.Automation.TogglePattern]::Pattern)).Toggle()
    }
    'set-value' {
      (Get-Pattern $element (
        [System.Windows.Automation.ValuePattern]::Pattern)).SetValue($Value)
    }
    'set-range' {
      (Get-Pattern $element (
        [System.Windows.Automation.RangeValuePattern]::Pattern)).SetValue(
          [double]$Value)
    }
    'select' {
      (Get-Pattern $element (
        [System.Windows.Automation.SelectionItemPattern]::Pattern)).Select()
    }
    'select-item' {
      Select-ComboItem $element $Item
    }
    'focus' {
      $element.SetFocus()
    }
    default {
      throw "unknown action '$Action'"
    }
  }
  Start-Sleep -Milliseconds $pauseMilliseconds
  return Read-Control $element
}

Invoke-Action | ConvertTo-Json -Compress -Depth 4

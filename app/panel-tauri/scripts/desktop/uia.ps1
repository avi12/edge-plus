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

$automation = [System.Windows.Automation.AutomationElement]
$patterns = [System.Windows.Automation.AutomationPattern]
$pauseMilliseconds = 150

function Find-Window {
  $condition = New-Object System.Windows.Automation.PropertyCondition(
    $automation::ProcessIdProperty, $ProcessId)
  # A TextBox makes Windows add a text input indicator window to the process,
  # which can be listed first: the example's own window is the WinUI one.
  $windows = $automation::RootElement.FindAll('Children', $condition)
  $window = $windows | Where-Object {
    $_.Current.ClassName -eq 'WinUIDesktopWin32WindowClass' } |
    Select-Object -First 1
  if ($null -eq $window) { throw 'the example has no window' }
  $window
}

function Find-Control([string]$AutomationId) {
  $property = $automation::AutomationIdProperty
  $wanted = $AutomationId
  if ($Name -ne '') {
    $property = $automation::NameProperty
    $wanted = $Name
  }
  $condition = New-Object System.Windows.Automation.PropertyCondition(
    $property, $wanted)
  $found = (Find-Window).FindFirst('Descendants', $condition)
  if ($null -eq $found) { throw "no control '$wanted'" }
  $found
}

function Get-Pattern($Element, $Pattern) {
  $object = $null
  if (-not $Element.TryGetCurrentPattern($Pattern, [ref]$object)) {
    throw "the control '$Id' does not support $($Pattern.ProgrammaticName)"
  }
  $object
}

function Read-Control($Element) {
  $state = [ordered]@{ name = $Element.Current.Name; enabled = $Element.Current.IsEnabled }
  $object = $null
  $toggle = [System.Windows.Automation.TogglePattern]::Pattern
  if ($Element.TryGetCurrentPattern($toggle, [ref]$object)) {
    $state.toggle = $object.Current.ToggleState.ToString()
  }
  $range = [System.Windows.Automation.RangeValuePattern]::Pattern
  if ($Element.TryGetCurrentPattern($range, [ref]$object)) {
    $state.range = $object.Current.Value
  }
  $text = [System.Windows.Automation.ValuePattern]::Pattern
  if ($Element.TryGetCurrentPattern($text, [ref]$object)) {
    $state.value = $object.Current.Value
  }
  $item = [System.Windows.Automation.SelectionItemPattern]::Pattern
  if ($Element.TryGetCurrentPattern($item, [ref]$object)) {
    $state.selected = $object.Current.IsSelected
  }
  $state
}

function Select-ComboItem($Element, [string]$Name) {
  $expand = Get-Pattern $Element (
    [System.Windows.Automation.ExpandCollapsePattern]::Pattern)
  $expand.Expand()
  Start-Sleep -Milliseconds $pauseMilliseconds
  $condition = New-Object System.Windows.Automation.PropertyCondition(
    $automation::NameProperty, $Name)
  $entry = $automation::RootElement.FindFirst('Descendants', $condition)
  if ($null -eq $entry) { throw "the combo box has no item '$Name'" }
  (Get-Pattern $entry (
    [System.Windows.Automation.SelectionItemPattern]::Pattern)).Select()
  Start-Sleep -Milliseconds $pauseMilliseconds
}

function Invoke-Action {
  if ($Action -eq 'names') {
    $scope = if ($Id -eq '') { Find-Window } else { Find-Control $Id }
    $all = $scope.FindAll('Descendants',
      [System.Windows.Automation.Condition]::TrueCondition)
    return @($all | ForEach-Object { $_.Current.Name } | Where-Object { $_ -ne '' })
  }
  if ($Action -eq 'toggles') {
    $pattern = [System.Windows.Automation.TogglePattern]::Pattern
    $all = (Find-Window).FindAll('Descendants', [System.Windows.Automation.Condition]::TrueCondition)
    return @($all | ForEach-Object {
      $object = $null
      if ($_.TryGetCurrentPattern($pattern, [ref]$object)) {
        [ordered]@{ id = $_.Current.AutomationId; name = $_.Current.Name
          toggle = $object.Current.ToggleState.ToString(); enabled = $_.Current.IsEnabled }
      }
    })
  }
  if ($Action -eq 'list-items') {
    $type = New-Object System.Windows.Automation.PropertyCondition(
      $automation::ControlTypeProperty, [System.Windows.Automation.ControlType]::ListItem)
    $items = (Find-Window).FindAll('Descendants', $type)
    return @($items | ForEach-Object {
      $rect = $_.Current.BoundingRectangle
      $child = $_.FindFirst('Descendants', [System.Windows.Automation.Condition]::TrueCondition)
      [ordered]@{ name = $_.Current.Name; height = $rect.Height
        inset = if ($null -eq $child) { -1 } else { $child.Current.BoundingRectangle.X - $rect.X } }
    })
  }
  if ($Action -eq 'pixel') {
    $element = Find-Control $Id
    $rect = $element.Current.BoundingRectangle
    $hwnd = [ExampleWin]::Find([uint32]$ProcessId)
    $bitmap = [ExampleWin]::Capture($hwnd)
    $window = [ExampleWin]::Rect($hwnd)
    $fraction = if ($Value -ne '') { [double]$Value } else { 0.5 }
    $x = [int]($rect.X + $rect.Width * $fraction - $window.L)
    $y = [int]($rect.Y + $rect.Height / 2 - $window.T)
    $color = $bitmap.GetPixel($x, $y)
    return [ordered]@{ color = ('{0:X2}{1:X2}{2:X2}' -f $color.R, $color.G, $color.B); x = $x; y = $y }
  }
  if ($Action -eq 'exists') {
    $condition = New-Object System.Windows.Automation.PropertyCondition(
      $automation::AutomationIdProperty, $Id)
    return $null -ne (Find-Window).FindFirst('Descendants', $condition)
  }
  $element = Find-Control $Id
  switch ($Action) {
    'read' { return Read-Control $element }
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
    'select-item' { Select-ComboItem $element $Item }
    'focus' { $element.SetFocus() }
    default { throw "unknown action '$Action'" }
  }
  Start-Sleep -Milliseconds $pauseMilliseconds
  Read-Control $element
}

Invoke-Action | ConvertTo-Json -Compress -Depth 4

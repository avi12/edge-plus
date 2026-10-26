<!-- The text and button looks two or more components share, global so a class
     reads the same wherever it is written (a component's own rules stay in its
     own <style>). -winui-based-on starts from WinUI's own style; every colour
     is a WinUI theme resource, so light and dark follow Windows. -->
<style>
  :global(.card-title) {
    text-wrap: wrap;
    -winui-based-on: BodyTextBlockStyle;
  }

  :global(.card-description) {
    color: var(--text-fill-color-secondary);
    text-wrap: wrap;
    -winui-based-on: CaptionTextBlockStyle;
  }

  :global(.caption) {
    -winui-based-on: CaptionTextBlockStyle;
  }

  :global(.accent-button) {
    -winui-based-on: AccentButtonStyle;
  }
</style>

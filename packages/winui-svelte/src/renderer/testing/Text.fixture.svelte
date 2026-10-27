<script lang="ts">
  // Text written as children, for host.test.ts: static and dynamic parts
  // mixed, an expression on lines of its own, and text a snippet renders (the
  // way a component passes its children on), and a sentence wrapped over
  // indented lines with a run of spaces, and a no-break space HTML would keep.
  interface Props {
    version: string;
    status: string;
    count: number;
  }

  const { version, status, count }: Props = $props();
</script>

{#snippet saved()}Saved {count} themes{/snippet}

<fake-panel>
  <fake-text-block>Edge+ {version} is ready</fake-text-block>
  <fake-text-block>
    {status}
  </fake-text-block>
  <fake-text-block>{@render saved()}</fake-text-block>
  <fake-text-block>
    A sentence long enough   to wrap
    over lines,&nbsp;indented.
  </fake-text-block>
</fake-panel>

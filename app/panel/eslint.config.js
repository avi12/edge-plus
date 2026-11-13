import { svelte } from "eslint-config-avi12/svelte";

export default svelte({ ignores: ["dist/", "src-tauri/", ".winapp/", "ui/protocol.g.ts", ".claude/worktrees/"] });

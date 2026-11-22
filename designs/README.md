# Designs

Pencil (`.pen`) mockups of the HiveWE UI. A file must be open and active in a VS Code tab for the Pencil MCP to work on it.

```
designs/
  current/      What the app looks like today (the baseline)
    main-window.pen     Main window, ribbon, viewport docks, palettes, minimap, tile tools
    object-editor.pen   Object Editor, search, icon and model views, ability lists, import dialog, tooltip editor
    trigger-editor.pen  Trigger Editor, Variable Editor, JASS editor, search
    map-settings.pen    Map info, Scenario Properties, gameplay constants, item tables and drops, new map, settings, shortcuts, unit properties
    tools.pen           Model Editor, Map Protector, Asset Manager
  proposals/    Mockups for planned work, one file per feature: <feature>.pen
```

- New windows and changed layouts start as a proposal, and are built only after approval.
- After a change ships, its frames are updated in `current/`.
- Frames are named `<Window> / <State>`.

## Opening `.pen` files

- The user keeps the Claude chat as a VS Code editor tab in its own editor group, locked with "Lock Group" (set up 2026-11-22).
- `code --reuse-window <file>.pen` then opens the file in the other, unlocked group. The chat stays visible and the `.pen` can be the active document for Pencil.
- Agents never close tabs and never open a `.pen` in a way that hides the chat.
- If the chat group is not locked, ask the orchestrator to tell the user. Do not work around it.

Needs verification: after a VS Code "Reload Window" the Pencil MCP stays disconnected for the running Claude session (`get_app_state` says "A document needs to be open" while the canvas shows). A new session, or an `/mcp` reconnect of `pencil`, is needed.

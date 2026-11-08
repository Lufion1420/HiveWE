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

# Reusable UI Widget Blueprints

Velcryn UI assets now support **User Widget** instances. Create a screen or
component in a standalone `.ui` file, then embed that same file in other
`.ui` assets without copying its layout. This is similar to Unreal Engine's
User Widget composition workflow.

## Editor workflow

1. Open the host `.ui` document in the UI Editor.
2. In **Palette > Panel**, add a **User Widget**. The instance is a transparent
   container and may be resized or anchored like any other UI widget.
3. Select the instance. In **Details > Reusable Widget > Source UI**, enter
   a project-relative source path such as `Assets/UI/Loadout.ui`.
   Press Enter or **Apply / Reload**.
4. Optionally enter an **Event Script**, such as
   `Assets/Scripts/Player.lua`. This redirects embedded buttons to that
   controller script instead of the callback script stored in the source UI.
5. Save the host document. Only the source reference and instance properties
   are saved. The imported children are not duplicated into the host file.

To change the design, open and edit the referenced source `.ui`, save it,
then reload the host document (or click **Apply / Reload** on the instance).
Every screen that uses the source picks up the same updated layout.

An instance can contain further User Widgets; circular references and nesting
deeper than 16 files are rejected. A missing referenced UI fails to load,
rather than silently displaying an incomplete menu. Source paths must be
relative and must not contain `..`.

### Instance geometry and event handling

The source UI's root widgets are placed inside the User Widget's rectangle.
For a full-screen source authored at 1920x1080, use a 1920x1080 instance at
(0, 0) for pixel-identical layout. Smaller reusable components should be
authored relative to their own local origin.

The instance's **Event Script** is optional. When set, every imported
button's `On Click Script` is routed to that script, while the button's
function name is retained. The host scene must contain a running Lua script
instance with matching path and callback function name.

Linked children are visible in the hierarchy, but editing them inside the
host is intentionally disabled. Open the source UI to edit the actual design.

## DuelFPS example

- `Assets/UI/Loadout.ui`: the **one master design** for the loadout selector.
- `Assets/UI/LoadoutLobby.ui`: a User Widget instance of the master that
  routes callbacks to `Assets/Scripts/MainMenu.lua`.
- `Assets/UI/LoadoutInGame.ui`: another instance of the same master that
  routes callbacks to `Assets/Scripts/Player.lua`.

The lobby and gameplay scripts load their respective host assets, but the
visible layout is sourced from the same `Loadout.ui`. Edit **Loadout.ui**
once to change both screens. Each host can later add its own backdrop,
navigation or overlays around the shared component.

## Format

Version 12 of the `ENGINE_UI` format adds widget type `UserWidget` (8).
The serialized widget has the same common fields as other widgets followed by
two quoted strings: the source UI asset path and optional event script.
Versions 1-11 continue to load unchanged. Saving uses version 12.

## Testing

The Vulkan UI interaction test now loads both DuelFPS host assets and checks
that they resolve the same source UI and dispatch button events to their
respective controller scripts. Run the existing RHI/UI smoke tests after
building the editor.

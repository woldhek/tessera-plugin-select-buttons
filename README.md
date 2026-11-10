# Select buttons

A row of buttons for an `input_select` or a `select` in Home Assistant: one button for each option, the current option
lit. Tap a button to choose that option. Made for switching an HDMI input between a few sources, but it works for any
choice of a handful of options: a scene, a mode, a speaker.

[Nederlandse versie](README.nl.md)

- Every option of the entity gets a button, up to six. A wide tile puts them in a row, a tall one stacks them, a square
  one makes a grid.
- The current option is the lit button. When the option changes elsewhere (an automation, the Home Assistant app, a
  remote), the screen follows.
- After a tap the button gets a ring until Home Assistant has the new option; then it lights up.
- Give each button an icon if you like; with **Show the option names** off, only the icons show.
- Turn on **Tap twice to switch** and the first tap asks "Tap again": against switching away by accident.

## What you need

- Tessera 0.4.91 or later.
- An `input_select` helper or a `select` entity in Home Assistant with the options you want, such as
  `Devkit`, `Personal PC` and `Office PC`.

## Set up

1. Add the plugin to a screen in the Tessera app.
2. In Layout, place the **Select buttons** tile from the plugin library, in the size you like (three across is a
   three-way switch).
3. In the inspector, choose the `input_select` or `select` as the tile's entity.
4. Optionally, choose an icon for each option (in the order Home Assistant lists them), turn off **Show the name** or
   **Show the option names**, or turn on **Tap twice to switch**.
5. Build and install the screen.

## Good to know

- The buttons send `input_select.select_option` or `select.select_option`; the screen must be allowed to perform
  actions.
- On a 1x1 tile three keys stand side by side and long names are cut short: give them icons, or use a wider tile.
- An entity with more than six options shows the first six. Tessera's own tile for the entity has them all.
- An option of more than 45 characters arrives cut short, so its button shows but cannot choose it.

## Screens it was tried on

Not on a screen yet. Checked against Tessera's code and drawn on a computer only.

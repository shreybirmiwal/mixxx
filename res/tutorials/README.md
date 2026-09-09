# Tutorial visibility profiles

Edit `visibility_profiles.json` to decide which parts of the loaded Mixxx skin
are on or off for each tutorial. The profile key must match the tutorial ID in
`src/dialog/dlgtutorialhome.cpp`. Off controls are fully hidden, including
native/OpenGL content, but retain their exact space in the default Mixxx layout.

For debugging, open any lesson and use the **Admin: controls** panel. It lists
every parsed skin item with a live checkbox, supports metadata search, and has
**All on** / **All off** controls. This panel is temporary builder UI and does
not appear on the tutorial home page.

Use `widgetStates` for explicit lesson configuration:

```json
"widgetStates": [
  { "objectName": "DecksRight", "visible": false },
  { "objectName": "CrossfaderContainer", "visible": true }
]
```

Every profile begins from the normal restored skin. A missing selector or
`"visible": true` therefore leaves that item on. `"visible": false` turns it
off for that lesson.

The older `hiddenWidgets` shorthand is also supported. A string hides every
widget with that skin `ObjectName`:

```json
"ClockWidget"
```

Use `within` to target a repeated control inside one area or deck:

```json
{ "within": "Deck1_Src", "objectName": "PlayDeck" }
```

Every parsed skin control also exposes resolved metadata, so unnamed controls
are targetable too:

```json
{ "within": "Deck1_Src", "tooltipId": "starrating", "visible": false },
{ "controlKey": "[Channel1],rate", "visible": false },
{ "within": "Deck1_Src", "widgetType": "HotcueButton", "visible": false }
```

Multiple selector fields are combined, so you can be as broad or exact as the
lesson needs. `within` always names the containing skin `ObjectName`.

Useful LateNight names include:

- Deck areas: `DecksLeft`, `DecksRight`, `Deck1_Src`, `Deck2_Src`
- Track controls: `Stars`, `KeyControls1`, `KeyControls2`, `RateContainer`,
  `RateSlider`, `HotcueControls`, `PlayDeck`, `CueDeck`, and `Reverse`
- Loops and jumps: `LoopControls`, `LoopActivate`, `LoopIn`, `LoopOut`,
  `JumpBack`, and `JumpForward`
- Toolbar: `ClockWidget`, `LatencyBatteryContainer`, `LatencyMeterBox`,
  `BatteryBox`, `RecBox`, `BroadcastButton`, `ToolBarSection`
- Main areas: `EffectsRack`, `SamplersRack`, `MicRack`, `AuxRack`,
  `LibraryContainer`, `CrossfaderContainer`

The app restores every item when Back to menu is clicked. An empty
`widgetStates` or `hiddenWidgets` array leaves the normal workspace unchanged.

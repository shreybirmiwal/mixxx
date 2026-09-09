# Tutorial visibility profiles

Edit `visibility_profiles.json` to decide which parts of the loaded Mixxx skin
are hidden for each tutorial. The profile key must match the tutorial ID in
`src/dialog/dlgtutorialhome.cpp`.

Use a string to hide every widget with that skin `ObjectName`:

```json
"ClockWidget"
```

Use `within` to target a repeated control inside one area or deck:

```json
{ "within": "Deck1_Src", "objectName": "PlayDeck" }
```

Useful LateNight names include `ClockWidget`, `DecksLeft`, `DecksRight`,
`EffectsRack`, `SamplersRack`, `MicAuxRack`, `LibraryContainer`,
`CrossfaderContainer`, `PlayDeck`, `CueDeck`, `Reverse`, `LoopActivate`,
`LoopIn`, `LoopOut`, `JumpBack`, and `JumpForward`.

The app restores every hidden widget when Back to menu is clicked. An empty
`hiddenWidgets` array leaves the normal DJ workspace unchanged.

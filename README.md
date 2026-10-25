# Maestro G-2 Rhythm 'N Sound (VST3)

Nachbau des Gibson Maestro Rhythm 'N Sound G-2 nach dem Service Manual (Drawing No. 1).

## Bauen ohne lokale Einrichtung
1. Repo auf GitHub hochladen (alle Dateien, inklusive des Ordners `.github`).
2. Reiter **Actions** oeffnen. Der Workflow `build` baut automatisch fuer Windows und macOS.
3. Nach dem gruenen Haken den Lauf oeffnen und unter **Artifacts** `MaestroG2-Windows` bzw. `MaestroG2-macOS` laden.
4. Zip entpacken, `VST3/Maestro G-2 Rhythm N Sound.vst3` in den VST3-Ordner kopieren:
   - Windows: `C:\Program Files\Common Files\VST3`
   - macOS: `~/Library/Audio/Plug-Ins/VST3` (einmalig: `xattr -dr com.apple.quarantine <Pfad zur .vst3>`)

Zum lokalen Bauen: `cmake -B build && cmake --build build --config Release`.

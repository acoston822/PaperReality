# Upstream Key Items integration

The 2026-11-07 upstream snapshot is PaperBoat develop at
`da70b9ea5c8efded02680865b4da76a9292e4c50` (#280 and #281).

Open **Dev Tools → Save Editor → Key Items** after loading a game.
The native UIKit page shows held items and their slot numbers, a 32-slot
capacity count, searchable available items, and switches to add or remove
one copy. Letter recipients and shared map/key icons have distinct names.
The standard native page renderer supplies UIKit controls, portraits,
search, and controller navigation.

Adding is disabled when all slots are occupied; held items remain removable.
Callbacks recheck save availability, capacity, and slot identity on the game
queue. Edits affect the loaded inventory only. Story flags and rewards are
unchanged; use Letters for delivery progression and save in game to persist.

Run `python3 tests/native-settings/key-items.py` for asset-free production-page
checks. The integration workflow also builds the complete unsigned iOS app
using Xcode 16.4 / iOS SDK 18.5 and syntax-checks UIKit in both diagnostic modes.
It merges upstream on `merge/upstream-2026-11-07`, never force-resets main,
and does not publish a release or tag.

On a device, verify the page opens with and without a save, searches letters,
shows portraits with your selected texture pack, adds/removes an item, and
reflects those edits in the game inventory and after saving/reloading.

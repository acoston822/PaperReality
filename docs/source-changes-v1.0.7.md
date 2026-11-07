# PaperReality 1.0.7 source integration audit

Audited main at `37e5685ee5ef6519a3c95a6395a07d3648202be5`.

| Source | Exact commit | Included in main |
| --- | --- | --- |
| v1.0.3 | `48b25a7b7e9c20b263f57fb30c3884a031eb01be` | Yes, ancestor |
| v1.0.4 | `926b73159d9fb3d7c1bf565a1cd2916e4aa565ab` | Yes, ancestor |
| v1.0.5 | `39ba92a71b2083f4fd4dee63515e5c9c583bcdae` | Yes, ancestor |
| v1.0.6 | `c4e2738ba03bf0e0e993568f8d0ede1dd63b4487` | Yes, ancestor |
| Final native Key Items change | `b00291d61219549a1876a7a80c84c48b8d614afe` | Yes, ancestor |
| PaperBoat upstream snapshot | `da70b9ea5c8efded02680865b4da76a9292e4c50` | Yes, ancestor |

GitHub compare reports zero commits behind for every source above against main.

Compared with v1.0.6, application code changes are confined to the Save Editor.
Its existing native editor remains in place, with the new Key Items page and
formatting applied. The other differences are CI, documentation, repository
instructions, README credits and the desktop Info.plist. The engine, UIKit
renderer, touch/controller implementation, rendering fixes and libultraship pin
are preserved.

The release preparation changes only the project version, release workflow and
release documentation. It does not revert or replace earlier release history.
The release publisher verifies source integration into main before creating the
matching v1.0.7 tag and unsigned IPA asset. Older release tags and assets remain
at their existing source commits.

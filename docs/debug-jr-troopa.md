# Jr. Troopa black-screen diagnostic build

Reported on iOS Actions build 32, without AirPlay, with Alternate Assets both
enabled and disabled. Music continued and the port menu could still open.
The supplied cumulative Paperboat.log has no watchdog stall or shader/pipeline
failure report identifying the cause. It records console reset commands and
asset toggles during the affected sessions.

This branch adds temporary `[battle-start]` entries to identify which battle
startup step completes: resource loading, stage shape/models/collision,
camera/actor initialization, and fade-in completion. It logs startup substate
changes and up to three wait reports per substate (300, 600 and 900 game ticks),
including the camera/player/first enemy script status.

These are diagnostics, not a confirmed fix. Gameplay decisions and transitions
are unchanged. Removed AirPlay diagnostics remain removed.

Install this branch's IPA, retry the first Jr. Troopa battle, leave the black
screen visible for about 15 seconds, then export the current Paperboat.log.
Keep the pre-battle save so a later fix can be checked at the same point.

Do not include ROMs, generated game archives, saves, photos or user-uploaded logs
in this repository or its Actions artifacts.

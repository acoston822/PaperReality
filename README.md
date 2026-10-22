# PaperReality
*An iOS port of Paper Mario 64, built on HarbourMasters' PaperBoat*

PaperReality is a community iOS build based on [PaperBoat](https://github.com/HarbourMasters/PaperBoat), the Harbour Masters port of Paper Mario 64. It is not an official Harbour Masters project. Please don't ask the Harbour Masters team for support with PaperReality, and do not report PaperReality issues to them.

Everything here mirrors PaperBoat's code and files; the changes made for iOS are listed in [docs/CHANGES.md](docs/CHANGES.md), and [docs/UPSTREAM.md](docs/UPSTREAM.md) explains how PaperBoat's updates are merged in.

Developers (of the original port):
* Bass3l
* JeodC
* KiritoDv
* Caladius

*We do not condone piracy.*

# Quick Start

PaperReality does not include any copyrighted assets. You are required to provide a supported copy of the game.

### 1. Verify your ROM dump
US SHA1 Hash: `3837f44cda784b466c9a2d99df70d77c322b97a0`
You can verify you have dumped a supported copy of the game by using the compatibility checker at https://paperboat.equipment/.

### 2. Install the app
Builds are unsigned `.ipa` files produced by this repository's GitHub Actions workflow (`Build unsigned iOS IPA`). Open the workflow run you want and download the `PaperReality-unsigned-ipa-bN` artifact. Sign and install it with the tool of your choice (for example AltStore, Sideloadly or your own developer certificate).

### 3. Launch the game
* On first launch, PaperReality asks you to import your copy of the game. Choose **Open Files** and pick your ROM in the Files app.
* The app places it in the right folder and generates the game's asset archive (`.o2r`) on the device.

### 4. Play!

Congratulations, you are now sailing with PaperReality! Have fun!

# Controls

Touch controls are on by default. You can move them, resize them and change their opacity from the settings menu, and the on-screen buttons give haptic feedback. When a game controller is connected, the touch controls hide automatically and come back when it disconnects (this can be turned off in the touch settings). The `Select`/`Back` button on a controller opens the menu.

# Custom Assets

Custom assets are packed in `.o2r` or `.otr` files. In the **Mods** menu, tap **Add Files** and pick your files in the Files app; they are copied to the app's `mods` folder. Turn on **Alternate Assets** and restart the app to use texture packs.

If you're interested in creating and/or packing your own custom asset `.o2r`/`.otr` files, check out the following tools:
* [**retro - OTR and O2R generator**](https://github.com/HarbourMasters64/retro)
* [**fast64 - Blender plugin (Note that PM64 is not fully supported at this time)**](https://github.com/HarbourMasters/fast64)

# Development

### Building
Pushing to any branch other than `develop` runs the `Build unsigned iOS IPA` workflow, which builds the app and uploads the IPA. To build locally on a Mac, follow the iOS notes in [docs/android-ios-web.md](docs/android-ios-web.md).

The multi-platform `Build` and desktop `Release` workflows are temporarily paused
in this repository. The IPA workflow enforces that pause and cancels their pending
runs; existing build history is retained. To resume them, remove its
`pause-other-builds` job, then re-enable both workflows in GitHub Actions. UIKit
simulator checks are manual-only, for requested debugging or renders; routine
pushes do not run them. Their existing cleanup remains available.

<a href="https://github.com/Kenix3/libultraship/">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="./docs/poweredbylus.darkmode.png">
    <img alt="Powered by libultraship" src="./docs/poweredbylus.lightmode.png">
  </picture>
</a>

# Special Thanks:

This wouldn't have been possible without your amazing work:

* [Harbour Masters and the PaperBoat team](https://github.com/HarbourMasters/PaperBoat), whose port this is built on
* [Kenix3 and the libultraship contributors](https://github.com/Kenix3/libultraship/)
* [JeodC](https://github.com/JeodC/libultraship), whose libultraship branch (`lus-converge`) this build uses
* [The Paper Mario decomp team](https://github.com/pmret/papermario)
* [The Paper Mario DX team](https://github.com/bates64/papermario-dx)

# License and AI Disclosure

PaperReality keeps PaperBoat's [CC0 1.0 license](LICENSE).

PaperBoat accepts pull requests that use AI, within the HarbourMasters [AI Policy](https://github.com/HarbourMasters/code-of-conduct/blob/main/AI_POLICY.md). The iOS changes in PaperReality were written with the help of Claude (Anthropic) under the direction of the maintainer.

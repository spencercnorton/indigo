<h1 align="center">Indigo</h1>

<p align="center">
  <strong>A fork of Swiss for the Nintendo GameCube with the interface rebuilt.</strong><br>
  An animated Home, a poster library and game details, drawn in the console's own visual language.
</p>

<p align="center">
  <a href="https://norvitech.com"><img alt="NorviTech Suite" src="https://img.shields.io/badge/NorviTech-Suite-FD8024.svg"></a>
  <a href="https://github.com/spencercnorton/indigo/tags"><img alt="Latest release" src="https://img.shields.io/github/v/tag/spencercnorton/indigo?label=release&sort=semver"></a>
  <a href="#install"><img alt="Build from source" src="https://img.shields.io/badge/install-from%20source-2D2D2D.svg"></a>
  <a href="LICENSE"><img alt="Licence" src="https://img.shields.io/badge/licence-GPL--2.0--or--later-blue.svg"></a>
  <a href="https://buy.stripe.com/8x26oH2U44f65TRe574wM04"><img alt="Donate" src="https://img.shields.io/badge/donate-Stripe-635bff.svg?logo=stripe&logoColor=white"></a>
</p>

<p align="center">
  <img alt="The Indigo Home screen: a glass cube turning between its Library, Source, Settings and System faces on a dark starfield, the Library face showing a GameCube controller, with only the face you are on named underneath." src="docs/screenshots/home.png" width="640">
</p>

Captured in the Dolphin emulator. The library and game detail pictures show a real poster pack and cheat file in use; box art belongs to its publishers.

Indigo is an unofficial fork of [Swiss](https://github.com/emukidid/swiss-gc),
the homebrew utility that boots and patches games on a Nintendo GameCube. The
fork changes one thing: what you look at. Everything underneath — the device
handlers, the patch engine, the loader — is upstream's work, and this fork
does not try to improve it. It is for people who use Swiss daily on real
hardware and want it to feel like it belongs on the console.

## What it does

**Home is an animated cube.** Four faces, one destination each, rotating in
both axes. Each face carries its own emblem, and only the face you are on is
named, under the cube. The Library face is a GameCube controller that
mirrors yours: its sticks lean with your sticks and its buttons light as you
press them, and when you leave it alone it plays by itself. The GameCube's own
interface language — the idle cube, the typeface, the palette — is the
reference, not a desktop launcher.

**The library retains its posters.** A grid over whatever device you booted
from, with artwork kept across navigation rather than re-read per frame, and
your selection restored when you come back from a game's details. Cover art
comes from a pack you build yourself; without one, each game gets a generated
card.

<p align="center">
  <img alt="The game library: GameCube box art in a carousel, the selected cover raised with its title and publisher below it." src="docs/screenshots/library.png" width="640">
</p>

**Game details are a surface, not a dialogue.** Artwork, last played, save
data, cheats and the boot options for that title in one place, with the same
controller grammar as every other screen.

<p align="center">
  <img alt="The game detail screen for LEGO Star Wars II: Y opens the cheat browser, three cheats are switched on and one switched back off, and the detail screen then reads 2 of 130 enabled." src="docs/screenshots/game-detail.png" width="640">
</p>

**Cheats read clearly.** The cheat browser shows per-cheat state plainly, and
the runtime handling around it is stricter about what it will apply.

**Settings and System were rebuilt to match.** Consistent navigation,
consistent focus, and no screen that still looks like the old list.

## Install

### Any platform — from source

The build runs in the same container image the project's CI uses, so no
toolchain is installed on your machine:

```bash
git clone https://github.com/spencercnorton/indigo.git
cd indigo
docker run --rm -u "$(id -u):$(id -g)" -v "$PWD:/work" -w /work \
  ghcr.io/extremscorner/libogc2 make dev
# writes cube/swiss/swiss.dol inside this folder, on your computer
```

There is no published binary release and no installer; this is a source fork
of a homebrew utility.

The release packaging targets (`make dist` and friends) are not supported
here: they need prebuilt tools and device firmware images that this fork does
not redistribute. `make dev` builds the executable, which is what the fork
changes.

### On the console

`cube/swiss/swiss.dol` is where the build leaves the file, not a path on your
SD card. On the card, Indigo takes the place of the file your loader already
boots, under that file's name. Your Swiss settings carry over.

- **PicoBoot** boots `ipl.dol` from the root of the card. Rename that file to
  `z.dol`, then copy `swiss.dol` to the root as `ipl.dol`. PicoBoot starts
  `z.dol` when Z is held at power-on, so stock Swiss stays one button away.
- **PicoBoot with no `ipl.dol` on the card** has Swiss flashed onto the Pico
  itself. Flash PicoBoot's standard firmware (see its
  [installation guide](https://support.webhdx.dev/gc/picoboot/installation-guide)),
  then do the step above.
- **Another loader that boots a `.dol` from the card by name**: replace that
  file the same way, keeping its name.
- **GC Loader, and other loaders that boot a disc image**: start `swiss.dol`
  from Swiss's file browser. This fork does not build `boot.iso` or the other
  packaged formats.

With In-Game Reset set to **Apploader**, a reset returns to the Swiss inside
`/swiss/patches/apploader.img`, not to Indigo; `make dev` does not rebuild
that file.

## Documentation

- [`CHANGELOG.md`](CHANGELOG.md) — what each release contains.
- [`NOTICE`](NOTICE) — upstream provenance and the third-party components in this tree.
- [`docs/screenshots/`](docs/screenshots) — the pictures above, captured in Dolphin.
- Upstream [Swiss documentation](https://github.com/emukidid/swiss-gc) covers every device handler, patch and boot option; none of it changed here.

## Contributing and support

- Bugs and feature requests: [open an issue](https://github.com/spencercnorton/indigo/issues/new/choose). Questions: [Discussions](https://github.com/spencercnorton/indigo/discussions). Do not report fork issues to the upstream project.
- Security reports: [private vulnerability reporting](https://github.com/spencercnorton/indigo/security/advisories/new) — see [SECURITY.md](SECURITY.md). There is no e-mail address; that is deliberate.
- Pull requests are welcome; read [CONTRIBUTING.md](CONTRIBUTING.md) first — this repository is a release mirror, and accepted changes ship in the next tagged release.
- If Indigo saves you time, you can [support its development](https://buy.stripe.com/8x26oH2U44f65TRe574wM04).

## Development

```bash
docker run --rm -u "$(id -u):$(id -g)" -v "$PWD:/work" -w /work \
  ghcr.io/extremscorner/libogc2 make dev      # what CI builds
buildtools/check_whitespace.sh
```

## Licence

[GPL-2.0-or-later](LICENSE) © Spencer Norton

Indigo is a modified version of [Swiss](https://github.com/emukidid/swiss-gc)
(© emukidid and the Swiss contributors, GPL-2.0-or-later) and inherits that
licence. Provenance, the modified surface and the third-party components in
this tree are recorded in [`NOTICE`](NOTICE). This fork is unofficial and is
not endorsed by or affiliated with the Swiss project.

Indigo's interface was built with [Claude Code](https://claude.com/claude-code).

---

<p align="center">
  <a href="https://norvitech.com"><img alt="Part of the NorviTech Suite — open-source apps for the Linux desktop and the self-hosted stack" src="https://norvitech.com/assets/banner.svg" width="640"></a>
</p>

<p align="center">
  <a href="https://github.com/spencercnorton/helios">Helios</a> ·
  <a href="https://github.com/spencercnorton/bitagent">BitAgent</a> ·
  <a href="https://github.com/spencercnorton/xnote">XNote</a> ·
  <a href="https://github.com/spencercnorton/xnote-placement">XNote Placement</a> ·
  <a href="https://github.com/spencercnorton/snipsnap">SnipSnap</a> ·
  <a href="https://github.com/spencercnorton/indigo">Indigo</a> ·
  <a href="https://norvitech.com">norvitech.com</a>
</p>

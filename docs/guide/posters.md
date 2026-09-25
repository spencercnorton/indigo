[Indigo guide](README.md) › Posters

# Posters

Posters are the box art in the [Library](library.md) and on each game's
[details](game-details.md). They come from one file on your card,
`/swiss/ui/posters.pak`. It's optional: without it, each game shows its disc
banner and its six-character game ID.

<p align="center">
  <img alt="1080° Avalanche in the Library twice: on the left without a poster pack, a card with its disc banner and game ID, GTEE01; on the right with one, its box art." src="images/posters-compare.png" width="640">
</p>

## Download a pack

Ready-made packs are made from GameTDB's cover art:

- [Posters: USA & Japan](https://indigo.norvitech.com/indigo-posters-ntsc.zip) (NTSC)
- [Posters: Europe & Australia](https://indigo.norvitech.com/indigo-posters-pal.zip) (PAL)

Unzip the one you want into the root of the card; it holds
`swiss/ui/posters.pak`. Indigo reads one pack at a time, so pick the region
most of your games are from. Checksums and details are at
[indigo.norvitech.com](https://indigo.norvitech.com).

A game gets its poster when its game ID is in the pack. Games from the other
region, and the few games GameTDB has no art for, keep their banner card.

## Build your own pack

You can make a pack from your own cover images, from this repository:

1. Put the front covers in one folder, each named after its game ID, such as
   `GMSE01.png` or `GALE01.jpg`. They should be at least 192×256; Indigo
   crops them to 3:4 from the center.
2. Build the pack. You need Python with Pillow, and Docker for the texture
   converter:

   ```bash
   python3 -m pip install pillow
   docker pull ghcr.io/extremscorner/libogc2@sha256:903b442dfd18cab00b5958726f70b17d95b0cf40c15d01b11e841825489dbe3d
   python3 buildtools/ui/poster_pack.py --covers ~/covers --out posters.pak
   ```

   Files that aren't named by a game ID are skipped and listed.
3. Copy `posters.pak` to `/swiss/ui/posters.pak` on the card.

A pack holds up to 1,024 covers.

## Where the game ID comes from

Every GameCube disc has a six-character ID: four for the game and region, two
for the publisher. Super Mario Sunshine for North America is `GMSE01`. A game
without a poster shows its ID on its card, and you can look up any game's ID
on [GameTDB](https://www.gametdb.com/).

---

<p align="center"><a href="system.md">← System</a> · <a href="troubleshooting.md">Troubleshooting →</a></p>

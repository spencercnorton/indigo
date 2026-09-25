[Indigo guide](README.md) › Library

# Library

The Library shows the games on your card as a row of posters. On Home, turn
the cube to **Library** and press A.

<p align="center">
  <img alt="The Library: moving right through the James Bond games to 1080° Avalanche, each cover raised in turn with its title and publisher below; A opens its details, B returns to the Library, and 1080° Avalanche is still selected." src="images/library-browse.png" width="640">
</p>

## Moving around

- **Left and Right** move through your games. The game in the middle is
  raised, with its title and publisher underneath, as the disc's own banner
  gives them.
- **A** opens the game's [details](game-details.md), where you launch it.
- **B** goes back to Home.

The Library remembers where you were. Come back from a game's details, or
from Home, and the same game is still selected.

## Set up the games folder

The Library shows the games in one folder, `/games` at the root of the card
(or of whichever device you chose on [Source](source.md)). Put each game in
its own folder, or put the disc images there directly:

```text
/games/Super Mario Sunshine [GMSE01]/game.iso
/games/Super Mario Sunshine.iso
```

Disc images end in `.iso`, `.gcm`, `.tgc` or `.fdi`, in capitals or not.
Keep nothing else in `/games`: any other file (a text file, a cover image)
or an empty folder turns the Library back into Swiss's plain file list.

<p align="center">
  <img alt="Swiss's plain file list of the games folder, one game per row with its banner, name and region flag, which Indigo shows instead of the Library when the folder holds something other than games." src="images/library-file-list.png" width="640">
</p>

If you see the list above instead of posters, look in `/games` for the file
that isn't a game. Or turn on **Hide unknown file types** in Settings ›
Setup › Library, which hides stray files so the Library can take over.

Games on two discs appear twice, once for each disc.

## Posters

The box art comes from a poster pack, which is optional. Without one, each
game shows its disc banner and its six-character game ID, such as `GMSE01`:

<p align="center">
  <img alt="The Library without a poster pack: 1080° Avalanche is a card with its disc banner and its game ID, GTEE01, instead of box art." src="images/library-no-posters.png" width="640">
</p>

Download a ready-made pack or build your own: see [Posters](posters.md).

## Read the games again

Indigo reads the games when it opens a device. If what's on it changes while
Indigo is running, for example when you swap the game disc, turn to
**Source** on Home and choose **Refresh Library**.

---

<p align="center"><a href="home.md">← Home</a> · <a href="game-details.md">Game details →</a></p>

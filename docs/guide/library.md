[Indigo guide](README.md) › Library

# Library

The Library shows the games on your card as posters: in a row, in a column
or in a grid, as you choose. On Home, turn the cube to **Library** and press
A.

<p align="center">
  <img alt="The Library: moving right through the James Bond games to 1080° Avalanche, each cover raised in turn with its title and publisher below; A opens its details, B returns to the Library, and 1080° Avalanche is still selected." src="images/library-browse.png" width="640">
</p>

## Choose a layout

**Library Layout**, the first row of [Settings](settings.md) › Setup ›
Library, sets how the Library lays out your games. Left and Right on it step
through the three, and the line above the buttons says how each one moves.

<p align="center">
  <img alt="Settings, Setup, Library: Library Layout is the first row. Right steps it from Horizontal to Vertical to Grid, and the line above the buttons reads A row of covers; Left and Right move (default), then A column of covers; Up and Down move, then Rows of five covers; every direction moves." src="images/library-layout-setting.png" width="640">
</p>

- **Horizontal**, the default: a row of covers. The game in the middle is
  raised, with its title and publisher underneath, as the disc's own banner
  gives them.
- **Vertical**: a column of covers down the left of the screen, turning like
  a wheel. The selected cover is large, with its title, publisher and game ID
  beside it.
- **Grid**: rows of five covers, three rows on screen. The selected cover is
  lit and a little larger, and its title and publisher are shown above the
  controls.

<p align="center">
  <img alt="The Vertical layout: the selected cover large at the left, the covers before and after it tipped back above and below, and its title, publisher, game ID and size beside it. Down moves through 007: Everything or Nothing, From Russia With Love, NightFire and 1080° Avalanche; Right jumps nine games to The Legend of Zelda: Ocarina of Time, and Down moves on to The Wind Waker." src="images/library-vertical.png" width="640">
</p>

<p align="center">
  <img alt="The Grid layout: five covers across and three rows on screen, the selected cover lit by a bright frame and a little larger, with its title and publisher above the controls. The frame slides right along the row, the rows scroll as it moves down and back up, and R moves three rows at a time." src="images/library-grid.png" width="640">
</p>

Every layout wraps round: after the last game comes the first.

## Moving around

| To move | Horizontal | Vertical | Grid |
| --- | --- | --- | --- |
| To the next or previous game | Left, Right | Up, Down | Left, Right |
| To the game above or below | | | Up, Down |
| A page at a time | Up, Down, L, R | Left, Right, L, R | L, R |

The control stick moves along the layout too, and in the grid in every
direction. A page is nine games, or three rows of the grid; it stops at the
first or last game (or row) before it wraps round.

In the grid, Left and Right run on from the end of one row to the start of
the next. Up and Down keep to the column. The last row can be short: from a
column it doesn't have, Up or Down lands on its last game.

- **A** opens the game's [details](game-details.md), where you launch it.
- **Y** opens the game's [own settings](game-details.md#this-games-own-settings),
  as X does on its details. When you leave them you are on the game's
  details, and B takes you back to the Library. Y never starts the game,
  even with **Boot without prompts** on.
- **B** goes back to Home.
- A small **sliders mark** in the corner of a cover means that game has
  [settings of its own](game-details.md#this-games-own-settings).

<p align="center">
  <img alt="Y on 007: Agent Under Fire in the grid opens its own settings. A on Force Video Mode lists the modes; 480p is chosen, the row is marked Custom and the top right reads 1 custom. B shows the game's details, whose Settings line reads 1 custom and Force Video Mode: 480p; B again returns to the grid with the same game selected." src="images/library-game-settings.png" width="640">
</p>

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

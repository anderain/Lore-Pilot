# font-packer

Packs 8x8 1bpp ASCII art into a Palm OS record database (`.pdb`), and emits a
matching C header so the reader can verify the database byte-for-byte on the
device.

Target device: **Pilot Professional, Palm OS 2.0, 1MB, 160x160 monochrome**.

## Usage

```powershell
# default: reads src/atlas-8x8.txt and the local font,
# writes out/LPT08A.pdb and reader/Src/GlyphAtlas.h
node font-packer/src/pack.mjs

# all options
node font-packer/src/pack.mjs `
  --in font-packer/src/atlas-8x8.txt `
  --pdb font-packer/out/LPT08A.pdb `
  --header reader/Src/GlyphAtlas.h `
  --preview font-packer/out/preview.txt `
  --svg font-packer/out/preview.svg `
  --font fusion-pixel-8px-monospaced-zh_hans.ttf `
  --name LPT08A --verify --selftest
```

The packer has no dependencies; Node 18+ is enough. It needs the 8px pixel
font at build time — see [Font source](#font-source).

Outputs:

| File | Purpose |
|---|---|
| `out/LPT08A.pdb` | 510-byte record database for the device |
| `out/preview.txt` | ASCII + hex preview, including the phrase at 1x and 2x |
| `out/preview.svg` | large visual preview (open in any browser) |
| `reader/Src/GlyphAtlas.h` | packed-in answer key for `GlyphTest.c` |

`--verify` re-parses the written PDB independently. `--selftest` also runs four
negative cases (bad magic, truncated file, bad record offset, flipped glyph
byte) and fails if any of them is accepted.

## Atlas format

```
// comments start with //
@text 诸天气荡荡，我道日兴隆

@book 0
........
.######.
.#....##
.#....##
.#....##
.#....##
.#....##
.######.
```

- `@name code`; `code` is optional and defaults to the next free index.
- Codes must be contiguous `0..N-1` (Phase A maps record `i+1` to code `i`).
- Exactly 8 rows per glyph, exactly 8 characters per row, only `#` and `.`.
- `#` is ink, `.` is paper. Bits are MSB-first within each byte.
- `@text <string>` rasterizes each unique character with the bundled 8px pixel
  font (Fusion Pixel 8px Mono zh-Hans) and appends it after the art glyphs.
  The first occurrence order defines both the glyph codes and the phrase
  sequence. Only one `@text` block is supported for now.

### How the font is rasterized

`src/ttf.mjs` is a small, dependency-free TrueType reader:

- looks up Unicode → glyph id through `cmap` format 12 (falls back to format 4);
- reads simple glyph contours from `glyf`/`loca`;
- samples the pixel center of every cell on an `unitsPerEm/8` grid
  (800/8 = 100 units for this font), using the even-odd rule;
- rejects composite glyphs and non-BMP code points.

No anti-aliasing, no hinting: this font is already an 8px bitmap design, so
point-in-polygon sampling reproduces its intended pixels exactly. The output for
`诸天气荡荡，我道日兴隆` is:

```
诸  00 4A 1C C8 7E 50 6E 4E      我  00 1A 68 FE 28 FA 24 6A
天  00 7C 10 FE 10 10 28 C6      道  00 94 3E 08 D4 5C 54 BE
气  00 40 7E B8 7C 04 04 02      日  00 7E 42 42 7E 42 42 7E
荡  00 28 FE 9C 04 BE 16 AA      兴  00 22 92 44 00 FE 00 C6
，  00 00 00 00 00 60 20 40      隆  00 CC D4 88 F6 DC 88 BE
```

## PDB layout

Database `LPT08A`, type `GLYF`, creator `LPRD`, Palm epoch timestamps,
big-endian throughout.

| Record | Contents |
|---|---|
| 0 | 32-byte font header |
| 1..N | one glyph, 8 bytes |
| N+1 | optional phrase record (when header `textRecord` is non-zero) |

Font header (32 bytes):

| Offset | Type | Field | 8x8 value |
|---|---|---|---|
| 0 | u32 | magic `LPG1` | |
| 4 | u16 | version | 1 |
| 6 | u16 | cellW | 8 |
| 8 | u16 | cellH | 8 |
| 10 | u16 | bytesPerGlyph | 8 |
| 12 | u16 | glyphCount | 21 |
| 14 | u16 | layout | 0 (one glyph per record) |
| 16 | u16 | glyphsPerRecord | 0 |
| 18 | u16 | flags | bit0 MSB-first, bit1 checksum valid |
| 20 | u16 | firstCode | 0 |
| 22 | u16 | compression | 0 (none) |
| 24 | u16 | dataChecksum | sum of all glyph bytes |
| 26 | u16 | packing | 0 (row aligned) |
| 28 | u32 | textRecord | record index of the phrase, 0 if none |

Phrase record (48 bytes for 11 characters):

| Offset | Type | Field |
|---|---|---|
| 0 | u16 | version (1) |
| 2 | u16 | count |
| 4 | u16 | glyphIndex[count] |
| 4 + 2*count | u16 | codePoint[count] (UTF-16, informational) |

Readers that only know glyph records can ignore `textRecord`; the glyph count
stays in the header and the text record is simply the last record.

File layout: 78-byte PDB header, 23 record entries x 8 bytes, then records.
For the default atlas that is 510 bytes, with the header record at 262 and the
phrase record at 462.

## Planned 12x12 extension

The header already carries `cellW`, `cellH`, `bytesPerGlyph`, `layout`,
`glyphsPerRecord`, `compression` and `packing`, so 12x12 does not change the
on-device reader interface:

- 12x12 row-aligned = 24 bytes/glyph; 3755 common characters = ~90KB.
- `layout=1` with 256 glyphs per record (6KB page), one page resident at a time.
- Record 0 would carry a 94-row GB2312 map after the 32-byte header.
- `packing=1` (18 bytes/glyph) is reserved for later if 1MB devices need it.

## Development helpers

`tools/dump-prc.mjs` audits a built `.prc`:

```powershell
node font-packer/tools/dump-prc.mjs font-packer/out/LorePilotReader.prc
```

It prints the resource table, checks the expected resource IDs, and lists every
`trap #$Axxx` found in the `code` resources, flagging anything at or above
`0xA307` (the first Palm OS 3.0 trap) as unsafe on 2.0. The current build has
no such trap, and the phrase page uses the same 1.0 drawing path as the
glyph atlas.

## GB2312 paged font

`src/pack-gb2312.mjs` builds the reader databases from the matching pixel font.
Two cell sizes are supported; each produces a `basic` and a `full` variant.

```powershell
# 8x8, symbols + level-1 hanzi, A1-D7, 3 pages, 50,100 bytes -> LPF08B.pdb
node font-packer/src/pack-gb2312.mjs --level basic

# 8x8, adds level-2 hanzi, A1-F7, 4 pages, 65,782 bytes -> LPF08F.pdb
node font-packer/src/pack-gb2312.mjs --level full

# 12x12, A1-D7, 11 pages, 137,428 bytes -> LPF12B.pdb
node font-packer/src/pack-gb2312.mjs --level basic --cell 12

# 12x12, A1-F7, 16 pages, 197,062 bytes -> LPF12F.pdb
node font-packer/src/pack-gb2312.mjs --level full --cell 12

# hanzi-only variants (B0-D7, no Chinese punctuation) -> LPF08H.pdb / LPF12H.pdb
node font-packer/src/pack-gb2312.mjs --level hanzi
node font-packer/src/pack-gb2312.mjs --level hanzi --cell 12
```

| Option | Meaning |
|---|---|
| `--level basic` / `1` | A1-D7: symbols + level-1 hanzi, tag `B` |
| `--level full` / `2` | A1-F7: adds 3,008 level-2 hanzi, tag `F` |
| `--level hanzi` | B0-D7: hanzi only, tag `H` |
| `--cell 8` | 8x8, 8 bytes/glyph, 2048 glyphs/page, name part `08` |
| `--cell 12` | 12x12, 24 bytes/glyph, 512 glyphs/page, name part `12` |
| `--pdb`, `--name` | `LPF08B/F/H` for 8x8, `LPF12B/F/H` for 12x12 |

### Naming convention

All paged font databases use **`LPF<cell><level>`**:

| Name | Cell | Range | Glyphs | Pages | Size |
|---|---|---|---:|---:|---:|
| `LPF08B` | 8x8 | A1-D7 | 4371 | 3 | 50,100 B |
| `LPF08F` | 8x8 | A1-F7 | 7379 | 4 | 65,782 B |
| `LPF08H` | 8x8 | B0-D7 | 3755 | 2 | 33,728 B |
| `LPF12B` | 12x12 | A1-D7 | 4648 | 11 | 137,428 B |
| `LPF12F` | 12x12 | A1-F7 | 7620 | 16 | 197,062 B |
| `LPF12H` | 12x12 | B0-D7 | 3755 | 8 | 100,592 B |

- `LPF` = LorePilot Font;
- cell is zero-padded to two digits so lexicographic order matches size;
- `B` = basic (with Chinese punctuation), `F` = full, `H` = hanzi only;
- the file name and the Palm database name are always identical;
- the reader sorts the font list by database name, so the order on screen is
  `LPF08B`, `LPF08F`, `LPF08H`, `LPF12B`, `LPF12F`, `LPF12H`.

The 8x8 atlas used by the `Glyph Test` form is a layout-0 test database, not a
paged font, so it uses a distinct prefix: **`LPT08A`** (`LPT` = test).

Each run also regenerates the sample answer header for its cell size:

- 8x8: `reader/Src/GbSamples.h` (`kGbSampleCodes`, `kGbSampleBytes[][8]`);
- 12x12: `reader/Src/GbSamples12.h` (`kGbSampleCodes12`, `kGbSampleBytes12[][24]`).

`reader/Src/GbText.h` is cell-independent: the full Qianziwen (1,250 units =
1,000 hanzi + 250 punctuation), 40 units per page.

### Paged layout (version 2)

| Record | Contents |
|---|---|
| 0 | 48-byte header |
| 1..N | one page: 2048 slots x 8 bytes (8x8) or 512 slots x 24 bytes (12x12) |

Header fields used by the reader:

| Offset | Type | Field | 8x8 basic | 8x8 full | 12x12 basic | 12x12 full |
|---|---|---|---|---|---|---|
| 0 | u32 | magic `LPG1` | | | | |
| 4 | u16 | version | 2 | 2 | 2 | 2 |
| 6/8/10 | u16 | cellW / cellH / bytesPerGlyph | 8/8/8 | 8/8/8 | 12/12/24 | 12/12/24 |
| 12 | u16 | glyphCount (informational) | 4371 | 7379 | 4648 | 7620 |
| 14 | u16 | layout | 1 | 1 | 1 | 1 |
| 16 | u16 | glyphsPerRecord | 2048 | 2048 | 512 | 512 |
| 18 | u16 | flags (bit0 = MSB-first) | 1 | 1 | 1 | 1 |
| 32/34 | u16 | gbFirstRow / gbLastRow | A1/D7 | A1/F7 | A1/D7 | A1/F7 |
| 36/38 | u16 | gbFirstCol / gbLastCol | A1/FE | A1/FE | A1/FE | A1/FE |
| 40/42 | u16 | gbRows / gbCols | 55/94 | 87/94 | 55/94 | 87/94 |
| 44 | u16 | gbPages | 3 | 4 | 11 | 16 |
| 46 | u16 | gbPageGlyphs | 2048 | 2048 | 512 | 512 |
| 28 | u32 | extrasRecord | 4 | 5 | 12 | 17 |

The optional extras record holds glyphs that the dense grid cannot address:

- GBK-only characters used by the text (7 in the Qianziwen: `凊磻乂俶鹍輶烝`);
- level-2 hanzi when the `basic` PDB is installed but the text uses them
  (78 in the Qianziwen).

Its layout is `u16 count`, then `count x (u16 code, bytesPerGlyph bytes)`.
The reader copies it once at open time and looks up codes outside the grid.

Address mapping is dense and O(1), so no per-row table is stored:

```
hi = code >> 8; lo = code & 0xFF
index = (hi - gbFirstRow) * gbCols + (lo - gbFirstCol)
page  = index / gbPageGlyphs
off   = (index % gbPageGlyphs) * bytesPerGlyph
record = 1 + page
```

`basic` and `full` share the same formula and start at A1, so the reader does
not branch on level; it only reads `cellW`, `gbLastRow`, `gbCols`, `gbPages` and
`gbPageGlyphs` from the header. The reader caches three pages (LRU): 48KB for
8x8, 36KB for 12x12, both fine on a 1MB Pilot Professional and below the 64KB
single-chunk limit. The extras record is at most 128 glyphs and is copied into a
fixed table at open time, so it costs no page cache.

### Qianziwen view in the reader

`GbTest` shows one 40-unit page at a time, so page turns land on sentence
boundaries (8 four-character lines). Layout follows the cell size:

| Cell | 1x | 2x |
|---|---|---|
| 8x8 | 20 per line, 2 lines (y 39..55) | 10 per line, 4 lines (y 58..122) |
| 12x12 | 13 per line, 4 lines (y 36..84) | 6 per line, 2 lines, first 12 units (y 84..132) |

The bottom bar is `Prev` / `Next` / `Load` / `Back`; tapping any glyph shows its
GB code and the first 8 data bytes; the status line shows the database name,
cell size, glyph count, page count, sample result and `P n/32` plus the unit
range.

### Coverage measured from the bundled font

- 3,755 level-1 hanzi: all present.
- 3,008 level-2 hanzi: all present.
- 616 symbol slots in A1-A9: 556 present; the missing ones are math/sequence
  symbols (e.g. `≤`, `⊙`, `№`), not Chinese punctuation.
- Chinese punctuation used by the demo (`，。`) is present.
- The full Qianziwen (1,000 hanzi + 250 punctuation) encodes to 1,250 GB2312
  units; 32 pages of 40 units each. Both `basic` and `full` render all 1,250
  units with no blanks.

### Qianziwen view in the reader

`GbTest` shows one 40-unit page at a time, so page turns land on sentence
boundaries (8 four-character lines). Layout follows the cell size:

| Cell | 1x | 2x |
|---|---|---|
| 8x8 | 20 per line, 2 lines (y 39..55) | 10 per line, 4 lines (y 58..122) |
| 12x12 | 13 per line, 4 lines (y 36..84) | 6 per line, 2 lines, first 12 units (y 84..132) |

The bottom bar is `Prev` / `Next` / `Load` / `Back`; tapping any glyph shows its
GB code and the first 8 data bytes; the status line shows the database name,
cell size, glyph count, page count, sample result and `P n/32` plus the unit
range.


## Font source

Two pixel fonts are used, one per cell size:

| Font | Cell | unitsPerEm | Used by |
|---|---|---|---|
| `fusion-pixel-8px-monospaced-zh_hans.ttf` | 8x8 | 800 | `pack.mjs` (`@text`), `pack-gb2312.mjs --cell 8` |
| `fusion-pixel-12px-monospaced-zh_hans.ttf` | 12x12 | 1200 | `pack-gb2312.mjs --cell 12` |

Both are monospaced pixel designs: `unitsPerEm / cell = 100` units per pixel,
so point-in-polygon sampling at the cell centre reproduces the intended pixels
exactly.

**The fonts are not distributed with this repository.** Download
"Fusion Pixel 8px/12px Mono zh-Hans" from
<https://github.com/TakWolf/fusion-pixel-font/releases>, then place the TTFs at
the repository root or pass `--font <path>`:

```powershell
node font-packer/src/pack-gb2312.mjs --level basic --cell 12 `
  --font C:\fonts\fusion-pixel-12px-monospaced-zh_hans.ttf
```

If the file is missing, both packers stop with a `NO_FONT` error that repeats
the download URL. The 12x12 packer also rejects a font whose grid is not
100 units per pixel, so an 8px font cannot silently produce a 12x12 database.

License: SIL Open Font License 1.1 (see the font's own `LICENSE-OFL` file).
The packer only needs the TTF at build time; nothing font-related goes into the
PRC or PDB except the resulting bytes.

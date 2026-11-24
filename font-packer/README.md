# font-packer

Packs 8x8 1bpp ASCII art into a Palm OS record database (`.pdb`), and emits a
matching C header so the reader can verify the database byte-for-byte on the
device.

Target device: **Pilot Professional, Palm OS 2.0, 1MB, 160x160 monochrome**.

## Usage

```powershell
# default: reads src/atlas-8x8.txt and the local font,
# writes out/* and reader/Src/GlyphAtlas.h
node font-packer/src/pack.mjs

# all options
node font-packer/src/pack.mjs `
  --in font-packer/src/atlas-8x8.txt `
  --pdb font-packer/out/LPGlyph8.pdb `
  --header reader/Src/GlyphAtlas.h `
  --preview font-packer/out/preview.txt `
  --svg font-packer/out/preview.svg `
  --font fusion-pixel-8px-monospaced-zh_hans.ttf `
  --name LPGlyph8 --verify --selftest
```

The packer has no dependencies; Node 18+ is enough. It needs the 8px pixel
font at build time — see [Font source](#font-source).

Outputs:

| File | Purpose |
|---|---|
| `out/LPGlyph8.pdb` | 510-byte record database for the device |
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

Database `LPGlyph8`, type `GLYF`, creator `LPRD`, Palm epoch timestamps,
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

`src/pack-gb2312.mjs` builds a second, independent database from the same TTF:

```powershell
# symbols + level-1 hanzi, A1-D7, 3 pages, 50,100 bytes
node font-packer/src/pack-gb2312.mjs --level basic

# symbols + level-1 + level-2 hanzi, A1-F7, 4 pages, 65,782 bytes
node font-packer/src/pack-gb2312.mjs --level full
```

| Option | Meaning |
|---|---|
| `--level basic` / `1` | A1-D7: 4,371 glyphs (616 symbols + 3,755 level-1 hanzi) |
| `--level full` / `2` | A1-F7: 7,379 glyphs (adds 3,008 level-2 hanzi) |
| `--level hanzi` | B0-D7: 3,755 hanzi only, no Chinese punctuation |
| `--pdb`, `--name` | default `LPGB2312B.pdb` / `LPGB2312B`, `LPGB2312F.pdb` / `LPGB2312F` |

Outputs `LPGB2312B.pdb` and/or `LPGB2312F.pdb`, plus two generated headers for
the reader:

- `reader/Src/GbSamples.h` — 10 sample characters with their expected 8 bytes
  (`天地玄黄宇宙洪荒日月`), used for on-device byte-for-byte verification;
- `reader/Src/GbText.h` — the full Qianziwen (1,250 units = 1,000 hanzi +
  250 punctuation marks), encoded as GB2312 bytes with 40 units per page.

`--text-file` selects the UTF-8 source; the default is
`font-packer/data/qianziwen.txt`. Text is read line by line and all whitespace
is ignored, so the file can be laid out for editing.

### Paged layout (version 2)

| Record | Contents |
|---|---|
| 0 | 48-byte header |
| 1..N | one page, 2048 glyph slots x 8 bytes = 16,384 bytes |

Header fields used by the reader:

| Offset | Type | Field | basic | full |
|---|---|---|---|---|
| 0 | u32 | magic `LPG1` | | |
| 4 | u16 | version | 2 | 2 |
| 6/8/10 | u16 | cellW / cellH / bytesPerGlyph | 8 / 8 / 8 | 8 / 8 / 8 |
| 12 | u16 | glyphCount (informational) | 4371 | 7379 |
| 14 | u16 | layout | 1 (paged) | 1 |
| 16 | u16 | glyphsPerRecord | 2048 | 2048 |
| 18 | u16 | flags (bit0 = MSB-first) | 1 | 1 |
| 32/34 | u16 | gbFirstRow / gbLastRow | A1 / D7 | A1 / F7 |
| 36/38 | u16 | gbFirstCol / gbLastCol | A1 / FE | A1 / FE |
| 40/42 | u16 | gbRows / gbCols | 55 / 94 | 87 / 94 |
| 44 | u16 | gbPages | 3 | 4 |
| 46 | u16 | gbPageGlyphs | 2048 | 2048 |
| 28 | u32 | extrasRecord | 4 | 5 |

The optional extras record holds glyphs that the dense grid cannot address:

- GBK-only characters used by the text (7 in the Qianziwen: `凊磻乂俶鹍輶烝`);
- level-2 hanzi when the `basic` PDB is installed but the text uses them
  (78 in the Qianziwen).

Its layout is `u16 count`, then `count x (u16 code, 8 bytes)`. The reader copies
it once at open time and looks up codes that fall outside the dense grid.

Address mapping is dense and O(1), so no per-row table is stored:

```
hi = code >> 8; lo = code & 0xFF
index = (hi - gbFirstRow) * gbCols + (lo - gbFirstCol)
page  = index / gbPageGlyphs
off   = (index % gbPageGlyphs) * 8
record = 1 + page
```

`basic` and `full` share the same formula and start at A1, so the reader does
not branch on level; it only reads `gbLastRow`, `gbCols`, `gbPages` and
`gbPageGlyphs` from the header. The reader caches three 16KB pages (LRU), which
fits the 1MB Pilot Professional and avoids the 64KB single-chunk limit. The
extras record is small (at most 128 glyphs) and is copied into a fixed table at
open time, so it costs no page cache.

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
boundaries (8 four-character lines):

- 1x: 20 units per line, 2 lines;
- 2x: 10 units per line, 4 lines;
- bottom bar `Prev` / `Next` / `Load` / `Back`;
- tapping any glyph shows its GB code and 8 bytes;
- the status line shows `P n/32` plus the unit range.


## Font source

`fusion-pixel-8px-monospaced-zh_hans.ttf` is the 8px pixel font used for
`@text` blocks. It is monospaced with an 800-unit em square, so each glyph maps
to an exact 8x8 cell.

**The font is not distributed with this repository.** Download
"Fusion Pixel 8px Mono zh-Hans" from
<https://github.com/TakWolf/fusion-pixel-font/releases>, then place the TTF at
the repository root or pass `--font <path>`:

```powershell
node font-packer/src/pack.mjs --font C:\fonts\fusion-pixel-8px-monospaced-zh_hans.ttf
```

If the file is missing, both packers stop with a `NO_FONT` error that repeats
the download URL.

License: SIL Open Font License 1.1 (see the font's own `LICENSE-OFL` file).
The packer only needs the TTF at build time; nothing font-related goes into the
PRC or PDB except the resulting bytes.

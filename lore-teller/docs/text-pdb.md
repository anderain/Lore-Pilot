# text PDB format (`LPBK`)

Produced by `lore-teller`, consumed by the Palm reader. All integers are
big-endian. The file name and the Palm database name are identical:
`LPB_<NAME>_<NN>.pdb`, where `<NN>` is the 1-based part number (max 99).

- type `'LPBK'`, creator `'LPRD'`, PDB version 1
- record 0: 64-byte header
- record `1..N`: one page per record; **record index = page offset within the part**

## Record 0 (64 bytes)

| Offset | Type | Field | Notes |
|---|---|---|---|
| 0 | u32 | magic | `'LPTX'` |
| 4 | u16 | version | 1 |
| 6 | u16 | flags | bit 0 reserved, currently 0 |
| 8 | u16 | cell | 8 or 12 (body glyph size) |
| 10 | u16 | charsPerLine | 20 (8px) or 13 (12px), informational |
| 12 | u16 | linesPerPage | 17 (8px) or 11 (12px) |
| 14 | u16 | headerRows | 8 |
| 16 | u16 | footerRows | 8 (text rows only) |
| 18 | u16 | partIndex | 0-based |
| 20 | u16 | partCount | total parts for this book |
| 22 | u16 | encoding | 1 = GB2312 body |
| 24 | u32 | firstPage | global page number of this part's first page (1-based) |
| 28 | u32 | pageCount | pages in this part |
| 32 | u32 | totalPages | pages in the whole book |
| 36 | u32 | textBytes | reserved (0) |
| 40 | u16 | pageRecordStart | 1 |
| 42 | u16 | progressRows | 2 (border + fill); 0 = old layout |
| 44 | char[17] | displayName | 16 chars + NUL, ASCII alnum only |

## Record 1..N (one page)

```
u16 lineCount
u16 reserved
u16 lineEnd[lineCount]   // cumulative byte offsets into the text payload
u16 textLen
u8  text[textLen]        // GB2312 double bytes
```

- Line `i` occupies `text[(i == 0 ? 0 : lineEnd[i-1]) .. lineEnd[i])`.
- A line shorter than `charsPerLine` is not padded: horizontal position is
  implied by the glyph order; the renderer just draws them left to right.
- An empty line has `lineEnd[i] == lineEnd[i-1]`, i.e. zero bytes.
- Maximum body per page: 17 lines x 20 chars x 2 bytes = 680 bytes (8px),
  11 x 13 x 2 = 286 bytes (12px). Well below the 64KB record limit.

## Body encoding

The body is pure GB2312 with one character per cell:

- ASCII `0x21..0x7E` and space are converted to fullwidth forms
  (`U+FF01..U+FF5E`, `U+3000`) before encoding, so every cell is exactly one
  double-byte code;
- characters outside GB2312 are replaced with `U+FF1F` (fullwidth `？`) and
  reported to the user at generation time;
- no ASCII bytes and no ESC sequences appear in the body.

## Page geometry (160x160)

| region | rows | style |
|---|---|---|
| header | y 0..7 (8) | black band, white 6x8 ASCII: displayName + page/total |
| body | y 8..149 (142) | white, glyph font, vertically centred |
| footer | y 150..157 (8) | black band, white 6x8 ASCII: PREV MENU NEXT |
| progress border | y 158 | white line across the full width |
| progress fill | y 159 | white up to `160 * page / total` |

| cell | lines per page | content height | top offset | first row |
|---|---|---|---|---|
| 8 | 17 | 136 | (142-136)/2 = 3 | y = 11 |
| 12 | 11 | 132 | (142-132)/2 = 5 | y = 13 |

Body height is `160 - headerRows - footerRows - progressRows`. A page with
fewer lines is centred in that band, so short pages stay balanced.

`progressRows` (offset 42) is 2 for new files. Readers must treat 0 as the old
layout: body 8..151, no progress bar, and 18 / 12 lines per page.

## Splitting

Volumes are filled with whole pages (never split a page), `pagesPerPart` at a
time. Each volume is **standalone**: it can be installed, opened and deleted
without the other volumes of the book, which is the point on a 1 MB Palm.

A volume's header still carries `partIndex`, `partCount`, `firstPage`,
`pageCount` and `totalPages`, so the reader can show a global page number and
global progress even though it only loaded one file.

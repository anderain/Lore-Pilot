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
| 12 | u16 | linesPerPage | 18 (8px) or 12 (12px) |
| 14 | u16 | headerRows | 8 |
| 16 | u16 | footerRows | 8 |
| 18 | u16 | partIndex | 0-based |
| 20 | u16 | partCount | total parts for this book |
| 22 | u16 | encoding | 1 = GB2312 body |
| 24 | u32 | firstPage | global page number of this part's first page (1-based) |
| 28 | u32 | pageCount | pages in this part |
| 32 | u32 | totalPages | pages in the whole book |
| 36 | u32 | textBytes | reserved (0) |
| 40 | u16 | pageRecordStart | 1 |
| 42 | u16 | reserved | 0 |
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
- Maximum body per page: 20 lines x 20 chars x 2 bytes = 800 bytes (8px),
  12 x 13 x 2 = 312 bytes (12px). Well below the 64KB record limit.

## Body encoding

The body is pure GB2312 with one character per cell:

- ASCII `0x21..0x7E` and space are converted to fullwidth forms
  (`U+FF01..U+FF5E`, `U+3000`) before encoding, so every cell is exactly one
  double-byte code;
- characters outside GB2312 are replaced with `U+FF1F` (fullwidth `？`) and
  reported to the user at generation time;
- no ASCII bytes and no ESC sequences appear in the body.

## Page geometry (160x160)

| cell | body rows | row height | chars/line | line width |
|---|---|---|---|---|
| 8 | y 8..151 | 8 | 20 | 160 |
| 12 | y 8..151 | 12 | 13 | 156 (4px right margin) |

Header (y 0..7) and footer (y 152..159) use the 6x8 `FontHybird` ASCII font.

## Splitting

Parts are filled with whole pages (never split a page). Each part's header
carries `partIndex`, `partCount`, `firstPage`, `pageCount`, `totalPages`, so
the reader can display global page numbers across parts.

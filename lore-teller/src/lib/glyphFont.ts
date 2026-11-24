// Minimal reader for the paged glyph PDBs produced by font-packer.
// Supports 8x8 (8 bytes/glyph, 2048/page) and 12x12 (24 bytes/glyph, 512/page)
// plus the small "extras" record for characters outside the dense grid.

export interface GlyphFont {
  name: string;
  cell: number;
  bytesPerGlyph: number;
  rowBytes: number;
  pageGlyphs: number;
  firstRow: number;
  lastRow: number;
  firstCol: number;
  lastCol: number;
  cols: number;
  pageCount: number;
  pages: Uint8Array[];
  extras: Map<number, Uint8Array>;
}

const HEADER_LEN = 48;

function be16(b: Uint8Array, o: number): number {
  return (b[o] << 8) | b[o + 1];
}

function be32(b: Uint8Array, o: number): number {
  return ((b[o] << 24) | (b[o + 1] << 16) | (b[o + 2] << 8) | b[o + 3]) >>> 0;
}

export function parseGlyphFont(buffer: ArrayBuffer, name: string): GlyphFont | null {
  const b = new Uint8Array(buffer);
  if (b.length < 78) return null;
  const numRecords = be16(b, 76);
  if (numRecords < 2) return null;

  const offsets: number[] = [];
  for (let i = 0; i < numRecords; i++) offsets.push(be32(b, 78 + i * 8));

  const header = b.subarray(offsets[0], offsets[0] + HEADER_LEN);
  if (header[0] !== 0x4c || header[1] !== 0x50 || header[2] !== 0x47 || header[3] !== 0x31) {
    return null; // 'LPG1'
  }
  if (be16(header, 4) !== 2) return null;       // version
  if (be16(header, 14) !== 1) return null;      // paged layout

  const cell = be16(header, 6);
  const cellH = be16(header, 8);
  const bytesPerGlyph = be16(header, 10);
  if (cell !== cellH || (cell !== 8 && cell !== 12)) return null;
  if (bytesPerGlyph !== cellH * Math.ceil(cell / 8)) return null;

  const pageCount = be16(header, 44);
  const pageGlyphs = be16(header, 46);
  const extrasRecord = be32(header, 28) >>> 0;
  const pages: Uint8Array[] = [];
  for (let i = 0; i < pageCount; i++) {
    const start = offsets[1 + i];
    const end = (i + 1 < pageCount && 2 + i < offsets.length ? offsets[2 + i] : b.length);
    pages.push(b.subarray(start, Math.min(end, start + pageGlyphs * bytesPerGlyph)));
  }

  const extras = new Map<number, Uint8Array>();
  if (extrasRecord && extrasRecord < offsets.length) {
    const start = offsets[extrasRecord];
    const end = extrasRecord + 1 < offsets.length ? offsets[extrasRecord + 1] : b.length;
    const rec = b.subarray(start, end);
    const count = be16(rec, 0);
    const item = 2 + bytesPerGlyph;
    for (let i = 0; i < count; i++) {
      const base = 2 + i * item;
      if (base + item > rec.length) break;
      extras.set(be16(rec, base), rec.subarray(base + 2, base + item));
    }
  }

  return {
    name,
    cell,
    bytesPerGlyph,
    rowBytes: Math.ceil(cell / 8),
    pageGlyphs,
    firstRow: be16(header, 32),
    lastRow: be16(header, 34),
    firstCol: be16(header, 36),
    lastCol: be16(header, 38),
    cols: be16(header, 42),
    pageCount,
    pages,
    extras,
  };
}

/** Return the glyph bytes for a GB2312 double-byte code, or null. */
export function glyphFor(font: GlyphFont, code: number): Uint8Array | null {
  const hi = (code >> 8) & 0xff;
  const lo = code & 0xff;
  if (hi >= font.firstRow && hi <= font.lastRow && lo >= font.firstCol && lo <= font.lastCol) {
    const index = (hi - font.firstRow) * font.cols + (lo - font.firstCol);
    const page = Math.floor(index / font.pageGlyphs);
    if (page >= font.pages.length) return null;
    const off = (index % font.pageGlyphs) * font.bytesPerGlyph;
    return font.pages[page].subarray(off, off + font.bytesPerGlyph);
  }
  return font.extras.get(code) ?? null;
}

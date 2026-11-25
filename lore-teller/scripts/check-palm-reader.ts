// Re-implement reader/Src/ReaderForm.c in TypeScript and compare its output
// with the web renderer (src/lib/render.ts). Any pixel difference means the
// Palm reader and the preview have drifted apart.
//
//   npx tsx scripts/check-palm-reader.ts <LPB_*.pdb> <LPF*.pdb> [pageIndex]

import fs from 'node:fs';
import path from 'node:path';
import {
  FOOTER_GOTO_X,
  FOOTER_LABEL_GOTO,
  FOOTER_LABEL_NEXT,
  FOOTER_LABEL_PREV,
  FOOTER_LABEL_SHELF,
  FOOTER_NEXT_X,
  FOOTER_PREV_X,
  FOOTER_SHELF_X,
  renderScreen,
  SCREEN_H,
  SCREEN_W,
} from '../src/lib/render';
import { uiGlyphRows } from '../src/fonts/uiGlyphs';
import { parseGlyphFont, glyphFor } from '../src/lib/glyphFont';
import { HYBRID_FONT_BYTES, HYBRID_FONT_HEIGHT } from '../src/fonts/hybrid6x8';

const dbFile = process.argv[2];
const fontFile = process.argv[3];
const pageArg = Number(process.argv[4] ?? 0);

if (!dbFile || !fontFile) {
  console.error('usage: npx tsx scripts/check-palm-reader.ts <LPB_*.pdb> <LPF*.pdb> [page]');
  process.exit(1);
}

// ---------------------------------------------------------------- LPBK read
const raw = fs.readFileSync(dbFile);
const db = new Uint8Array(raw.buffer, raw.byteOffset, raw.byteLength);
const u16 = (o: number) => (db[o] << 8) | db[o + 1];
const u32 = (o: number) => ((db[o] << 24) | (db[o + 1] << 16) | (db[o + 2] << 8) | db[o + 3]) >>> 0;
const recCount = u16(76);
const recOff = (i: number) => u32(78 + i * 8);

const hdr = db.subarray(recOff(0), recOff(0) + 64);
const hU16 = (o: number) => (hdr[o] << 8) | hdr[o + 1];
const hU32 = (o: number) => ((hdr[o] << 24) | (hdr[o + 1] << 16) | (hdr[o + 2] << 8) | hdr[o + 3]) >>> 0;

let progressRows = hU16(42);
if (progressRows > 4) progressRows = 0;

const header = {
  cell: hU16(8),
  charsPerLine: hU16(10),
  headerRows: hU16(14) || 8,
  footerRows: hU16(16) || 8,
  progressRows,
  partIndex: hU16(18),
  partCount: hU16(20),
  firstPage: hU32(24),
  pageCount: hU32(28),
  totalPages: hU32(32),
  displayName: String.fromCharCode(...hdr.subarray(44, 60)).replace(/\0.*$/, ''),
};

// ------------------------------------------------------------ page decode
const page = pageArg;
const rec = db.subarray(recOff(1 + page), recOff(1 + page + 1));
const pU16 = (o: number) => (rec[o] << 8) | rec[o + 1];
const lineCount = pU16(0);
const starts: number[] = [];
const ends: number[] = [];
let prev = 0;
for (let i = 0; i < lineCount; i++) {
  const end = pU16(4 + i * 2);
  starts.push(prev);
  ends.push(end);
  prev = end;
}
const textLen = Math.min(pU16(4 + lineCount * 2), prev);
const text = rec.subarray(6 + lineCount * 2, 6 + lineCount * 2 + textLen);

// ------------------------------------------------------- ReaderForm.c port
const W = SCREEN_W;
const H = SCREEN_H;
const bits = new Uint8Array(W * H); // 1 = black

const bodyTop = header.headerRows;
const bodyBottom = H - (header.footerRows + header.progressRows);
const bodyHeight = bodyBottom - bodyTop;
const perLine = Math.min(header.charsPerLine || Math.floor(W / header.cell), Math.floor(W / header.cell));

function drawLine(x0: number, y: number, x1: number, black: boolean) {
  if (y < 0 || y >= H) return;
  const a = Math.max(0, Math.min(x0, x1));
  const b = Math.min(W - 1, Math.max(x0, x1));
  for (let x = a; x <= b; x++) bits[y * W + x] = black ? 1 : 0;
}

function fillBand(top: number, rows: number, black: boolean) {
  for (let y = top; y < top + rows && y < H; y++) drawLine(0, y, W - 1, black);
}

// 6x8 ASCII, horizontal runs per row, same as UiFontDrawGlyph.
function asciiGlyph(x: number, y: number, ch: number, black: boolean) {
  if (ch < 0x20 || ch > 0x7e) return;
  const base = ch * HYBRID_FONT_HEIGHT;
  for (let row = 0; row < HYBRID_FONT_HEIGHT; row++) {
    const bitsRow = HYBRID_FONT_BYTES[base + row];
    let c = 0;
    while (c < 6) {
      if ((bitsRow & (0x80 >> c)) === 0) {
        c++;
        continue;
      }
      const start = c;
      while (c < 6 && (bitsRow & (0x80 >> c)) !== 0) c++;
      drawLine(x + start, y + row, x + c - 1, black);
    }
  }
}

function asciiText(x: number, y: number, s: string, black: boolean) {
  let cx = x;
  for (const ch of s) {
    asciiGlyph(cx, y, ch.charCodeAt(0), black);
    cx += 6;
  }
}

// Built-in Chinese glyphs, read from the same generated table the web preview
// uses, so this check tracks the app exactly.
function uiGlyph(x: number, y: number, code: number, black: boolean) {
  const rows = uiGlyphRows(code);
  if (rows === null) return;
  for (let row = 0; row < 8; row++) {
    const bitsRow = rows[row];
    let c = 0;
    while (c < 8) {
      if ((bitsRow & (0x80 >> c)) === 0) {
        c++;
        continue;
      }
      const start = c;
      while (c < 8 && (bitsRow & (0x80 >> c)) !== 0) c++;
      drawLine(x + start, y + row, x + c - 1, black);
    }
  }
}

function uiGlyphsText(x: number, y: number, codes: readonly number[], black: boolean) {
  let cx = x;
  for (const code of codes) {
    uiGlyph(cx, y, code, black);
    cx += 8;
  }
}

// Body.
const fontRaw = fs.readFileSync(fontFile);
const font = parseGlyphFont(
  fontRaw.buffer.slice(fontRaw.byteOffset, fontRaw.byteOffset + fontRaw.byteLength) as ArrayBuffer,
  path.basename(fontFile),
);
if (!font) throw new Error('cannot parse font');

for (let y = bodyTop; y < bodyBottom; y++) drawLine(0, y, W - 1, false);

const maxLines = Math.floor(bodyHeight / header.cell);
const used = Math.min(lineCount, maxLines);
const firstY = bodyTop + Math.floor((bodyHeight - used * header.cell) / 2);

for (let i = 0; i < used; i++) {
  const start = starts[i];
  const len = ends[i] - starts[i];
  for (let col = 0; col < perLine; col++) {
    if (col * 2 + 1 >= len) break;
    const off = start + col * 2;
    if (off + 1 >= textLen) break;
    const code = (text[off] << 8) | text[off + 1];
    const glyph = glyphFor(font, code);
    if (!glyph) continue;
    const gx = col * header.cell;
    const gy = firstY + i * header.cell;
    for (let row = 0; row < header.cell; row++) {
      for (let bit = 0; bit < header.cell; bit++) {
        if ((glyph[row * font.rowBytes + (bit >> 3)] & (0x80 >> (bit & 7))) === 0) continue;
        const px = gx + bit;
        const py = gy + row;
        if (px < 0 || px >= W || py < 0 || py >= H) continue;
        bits[py * W + px] = 1;
      }
    }
  }
}

// Header.
fillBand(0, bodyTop, true);
asciiText(0, 0, header.displayName, false);
const pageLabel = `${header.firstPage + page}/${header.totalPages}`;
asciiText(W - pageLabel.length * 6, 0, pageLabel, false);

// Footer.
fillBand(bodyBottom, header.footerRows + header.progressRows, true);
uiGlyphsText(FOOTER_PREV_X, bodyBottom, FOOTER_LABEL_PREV, false);
uiGlyphsText(FOOTER_GOTO_X, bodyBottom, FOOTER_LABEL_GOTO, false);
uiGlyphsText(FOOTER_SHELF_X, bodyBottom, FOOTER_LABEL_SHELF, false);
uiGlyphsText(FOOTER_NEXT_X, bodyBottom, FOOTER_LABEL_NEXT, false);

if (header.progressRows >= 2) {
  const borderY = bodyBottom + header.footerRows;
  drawLine(0, borderY, W - 1, false);
  const total = header.totalPages || 1;
  const globalPage = Math.min(header.firstPage + page, total);
  const filled = Math.round((W * globalPage) / total);
  if (filled > 0) drawLine(0, borderY + 1, filled - 1, false);
}

// ------------------------------------------------------------- web compare
// Decode the GB2312 bytes to characters for the web renderer.
const decoder = new TextDecoder('gbk');
const pageLines = Array.from({ length: lineCount }, (_, i) =>
  decoder.decode(text.subarray(starts[i], ends[i])),
);

const webBits = renderScreen(
  {
    displayName: header.displayName,
    page: { lines: pageLines },
    pageNumber: header.firstPage + page,
    totalPages: header.totalPages,
  },
  font,
  header.cell as 8 | 12,
);

let diff = 0;
const diffs: string[] = [];
for (let y = 0; y < H; y++) {
  for (let x = 0; x < W; x++) {
    const palm = bits[y * W + x];
    const web = webBits[y * W + x];
    if (palm !== web) {
      diff++;
      if (diffs.length < 20) diffs.push(`(${x},${y}) palm=${palm} web=${web}`);
    }
  }
}

console.log(`${dbFile} page ${page + 1}`);
console.log(`  cell ${header.cell}, ${perLine} chars/line, body ${bodyTop}..${bodyBottom - 1}, ` +
            `footer ${bodyBottom}..${bodyBottom + header.footerRows + header.progressRows - 1}`);
if (diff === 0) {
  console.log('  OK: Palm layout matches the web preview exactly');
} else {
  console.log(`  ${diff} differing pixel(s)`);
  for (const d of diffs) console.log(`    ${d}`);
  process.exitCode = 1;
}

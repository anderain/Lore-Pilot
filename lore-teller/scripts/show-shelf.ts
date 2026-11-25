// Render the bookshelf exactly as reader/Src/Shelf.c does and print it as
// ASCII art, so the built-in glyphs and button layout can be checked without a
// device.
//
//   npx tsx scripts/show-shelf.ts

import { uiGlyphRows } from '../src/fonts/uiGlyphs';

const W = 160;
const H = 160;
const bits = new Uint8Array(W * H); // 1 = black

// --- Shelf.c geometry -------------------------------------------------------
const READ = { x: 2, y: 74, w: 46, h: 13 };
const DELETE_BOOK = { x: 52, y: 74, w: 30, h: 13 };
const DELETE_FONT = { x: 2, y: 135, w: 30, h: 13 };
const BOOKS_LABEL = { x: 2, y: 16 };
const FONTS_LABEL = { x: 2, y: 88 };

const LABEL_BOOKS = [0xcae9, 0xbcdc];
const LABEL_FONTS = [0xd7d6, 0xcce5];
const LABEL_READ = [0xd4c4, 0xb6c1];
const LABEL_DELETE = [0xc9be, 0xb3fd];

function line(x0: number, y: number, x1: number, black: boolean) {
  if (y < 0 || y >= H) return;
  for (let x = Math.max(0, Math.min(x0, x1)); x <= Math.min(W - 1, Math.max(x0, x1)); x++) {
    bits[y * W + x] = black ? 1 : 0;
  }
}

function fillRect(x: number, y: number, w: number, h: number, black: boolean) {
  for (let yy = y; yy < y + h; yy++) line(x, yy, x + w - 1, black);
}

function frame(x: number, y: number, w: number, h: number) {
  line(x, y, x + w - 1, true);
  line(x, y + h - 1, x + w - 1, true);
  line(x, y, x, y + h - 1, true);
  line(x + w - 1, y, x + w - 1, y + h - 1, true);
}

function glyph(x: number, y: number, code: number, black: boolean) {
  const rows = uiGlyphRows(code);
  if (rows === null) return;
  for (let row = 0; row < 8; row++) {
    const byte = rows[row];
    let c = 0;
    while (c < 8) {
      if ((byte & (0x80 >> c)) === 0) {
        c++;
        continue;
      }
      const start = c;
      while (c < 8 && (byte & (0x80 >> c)) !== 0) c++;
      line(x + start, y + row, x + c - 1, black);
    }
  }
}

function glyphs(x: number, y: number, codes: number[], black: boolean) {
  codes.forEach((code, i) => glyph(x + i * 8, y, code, black));
}

function button(x: number, y: number, w: number, h: number, codes: number[], enabled = true) {
  fillRect(x, y, w, h, false);
  frame(x, y, w, h);
  const labelW = codes.length * 8;
  const lx = x + Math.floor((w - labelW) / 2);
  const ly = y + Math.floor((h - 8) / 2);
  glyphs(lx, ly, codes, true);
  if (!enabled) {
    // gray pattern: every other pixel column, as a stand-in
    for (let yy = y; yy < y + h; yy++) {
      for (let xx = x; xx < x + w; xx++) {
        if ((yy & 1) === 0 && bits[yy * W + xx] === 1 && (xx & 1) === 1) {
          bits[yy * W + xx] = 0;
        }
      }
    }
  }
}

// --- draw ------------------------------------------------------------------
fillRect(0, 0, W, H, false);

glyphs(BOOKS_LABEL.x, BOOKS_LABEL.y, LABEL_BOOKS, true);
glyphs(FONTS_LABEL.x, FONTS_LABEL.y, LABEL_FONTS, true);

const booksEnabled = !process.argv.includes('--empty');
button(READ.x, READ.y, READ.w, READ.h, LABEL_READ, booksEnabled);
button(DELETE_BOOK.x, DELETE_BOOK.y, DELETE_BOOK.w, DELETE_BOOK.h, LABEL_DELETE, booksEnabled);
button(DELETE_FONT.x, DELETE_FONT.y, DELETE_FONT.w, DELETE_FONT.h, LABEL_DELETE, booksEnabled);

// --- print -----------------------------------------------------------------
for (let y = 8; y < 152; y++) {
  let s = '';
  for (let x = 0; x < W; x++) s += bits[y * W + x] ? '#' : ' ';
  if (s.trim() === '') continue;
  console.log(String(y).padStart(3) + '|' + s + '|');
}

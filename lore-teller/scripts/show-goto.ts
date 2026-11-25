// Render the goto keypad exactly as reader/Src/GotoForm.c does and print it,
// so the layout can be checked without a device.
//
//   npx tsx scripts/show-goto.ts

import { uiGlyphRows } from '../src/fonts/uiGlyphs';
import { HYBRID_FONT_BYTES, HYBRID_FONT_HEIGHT } from '../src/fonts/hybrid6x8';

const W = 160;
const H = 160;
const bits = new Uint8Array(W * H); // 1 = black

// --- GotoForm.c geometry ---------------------------------------------------
const TITLE_Y = 8;
const BOX = { x: 12, y: 20, w: 136, h: 18 };
const KEY_W = 44;
const KEY_H = 18;
const KEY_X = [12, 58, 104];
const ROW = [42, 62, 82];
const ZERO = { x: 12, y: 102, w: 90, h: 18 };
const BACK = { x: 104, y: 102, w: 44, h: 18 };
const OK = { x: 20, y: 124, w: 56, h: 18 };
const CANCEL = { x: 84, y: 124, w: 56, h: 18 };

const TITLE = [0xccf8, 0xd7aa, 0xd6c1, 0xd2b3, 0xc2eb]; // 跳转至页码
const LBL_OK = [0xccf8, 0xd7aa]; // 跳转
const LBL_CANCEL = [0xc8a1, 0xcffb]; // 取消

const typed = '990';
const range = '599-999';

function line(x0: number, y: number, x1: number, black: boolean) {
  if (y < 0 || y >= H) return;
  for (let x = Math.max(0, Math.min(x0, x1)); x <= Math.min(W - 1, Math.max(x0, x1)); x++) {
    bits[y * W + x] = black ? 1 : 0;
  }
}

function frame(r: { x: number; y: number; w: number; h: number }) {
  for (let x = r.x; x < r.x + r.w; x++) {
    line(x, r.y, x, true);
    line(x, r.y + r.h - 1, x, true);
  }
  for (let y = r.y; y < r.y + r.h; y++) {
    line(r.x, y, r.x, true);
    line(r.x + r.w - 1, y, r.x + r.w - 1, true);
  }
}

function fillRect(x: number, y: number, w: number, h: number, black: boolean) {
  for (let yy = y; yy < y + h; yy++) line(x, yy, x + w - 1, black);
}

function uiGlyph(x: number, y: number, code: number) {
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
      line(x + start, y + row, x + c - 1, true);
    }
  }
}

function ascii(x: number, y: number, text: string) {
  let cx = x;
  for (const ch of text) {
    const base = ch.charCodeAt(0) * HYBRID_FONT_HEIGHT;
    for (let row = 0; row < HYBRID_FONT_HEIGHT; row++) {
      const byte = HYBRID_FONT_BYTES[base + row];
      let c = 0;
      while (c < 6) {
        if ((byte & (0x80 >> c)) === 0) {
          c++;
          continue;
        }
        const start = c;
        while (c < 6 && (byte & (0x80 >> c)) !== 0) c++;
        line(cx + start, y + row, cx + c - 1, true);
      }
    }
    cx += 6;
  }
}

function key(
  r: { x: number; y: number; w: number; h: number },
  caption: string | number[],
) {
  fillRect(r.x, r.y, r.w, r.h, false);
  frame(r);
  if (Array.isArray(caption)) {
    const w = caption.length * 8;
    const x = r.x + Math.floor((r.w - w) / 2);
    const y = r.y + Math.floor((r.h - 8) / 2);
    caption.forEach((code, i) => uiGlyph(x + i * 8, y, code));
  } else {
    const w = caption.length * 6;
    const x = r.x + Math.floor((r.w - w) / 2);
    const y = r.y + Math.floor((r.h - 8) / 2);
    ascii(x, y, caption);
  }
}

// --- draw ------------------------------------------------------------------
fillRect(0, 0, W, H, false);

// title, centred
const titleW = TITLE.length * 8;
TITLE.forEach((code, i) => uiGlyph((W - titleW) / 2 + i * 8, TITLE_Y, code));

// text box
fillRect(BOX.x, BOX.y, BOX.w, BOX.h, false);
frame(BOX);
ascii(BOX.x + 4, BOX.y + 5, typed + '_');
ascii(BOX.x + BOX.w - 4 - range.length * 6, BOX.y + 5, range);

// keypad
['7', '8', '9'].forEach((d, i) => key({ x: KEY_X[i], y: ROW[0], w: KEY_W, h: KEY_H }, d));
['4', '5', '6'].forEach((d, i) => key({ x: KEY_X[i], y: ROW[1], w: KEY_W, h: KEY_H }, d));
['1', '2', '3'].forEach((d, i) => key({ x: KEY_X[i], y: ROW[2], w: KEY_W, h: KEY_H }, d));
key(ZERO, '0');
key(BACK, '<-');

key(OK, LBL_OK);
key(CANCEL, LBL_CANCEL);

// --- print -----------------------------------------------------------------
for (let y = 0; y < H; y++) {
  let s = '';
  for (let x = 0; x < W; x++) s += bits[y * W + x] ? '#' : ' ';
  if (s.trim() === '') continue;
  console.log(String(y).padStart(3) + '|' + s + '|');
}

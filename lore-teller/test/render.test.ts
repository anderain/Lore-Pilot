// DOM-level smoke test for the preview renderer and the font loader.
// Runs under jsdom; verifies that a page actually paints pixels and that the
// header/footer ASCII font is used.

import { describe, expect, it } from 'vitest';
import { parseGlyphFont } from '../src/lib/glyphFont';
import { UI_GLYPHS, uiGlyphRows } from '../src/fonts/uiGlyphs';
import {
  FOOTER_GOTO_X,
  FOOTER_NEXT_X,
  FOOTER_PREV_X,
  FOOTER_SHELF_X,
  paintToCanvas,
  renderScreen,
  SCREEN_H,
  SCREEN_W,
} from '../src/lib/render';
import fs from 'node:fs';
import path from 'node:path';

const FONT_DIR = path.resolve(__dirname, '..', 'public', 'fonts');

function loadFont(file: string) {
  const full = path.join(FONT_DIR, file);
  if (!fs.existsSync(full)) return null;
  const raw = fs.readFileSync(full);
  const buf = raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength) as ArrayBuffer;
  return parseGlyphFont(buf, file);
}

const font8 = loadFont('LPF08F.pdb');
const font12 = loadFont('LPF12F.pdb');

describe('built-in glyph table', () => {
  it('matches reader/Src/UiGlyphs.c', () => {
    const source = path.resolve(__dirname, '..', '..', 'reader', 'Src', 'UiGlyphs.c');
    const src = fs.readFileSync(source, 'utf8');
    const re = /\{\s*(0x[0-9A-Fa-f]{2}[0-9A-Fa-f]{2})\s*,\s*\{([^}]*)\}\s*\}/g;
    const fromC: Array<{ code: number; rows: number[] }> = [];
    let m: RegExpExecArray | null;
    while ((m = re.exec(src))) {
      fromC.push({
        code: parseInt(m[1], 16),
        rows: m[2]
          .split(',')
          .map((s) => s.trim())
          .filter(Boolean)
          .map((s) => parseInt(s, 16)),
      });
    }

    expect(fromC.length).toBe(UI_GLYPHS.length);
    for (let i = 0; i < fromC.length; i++) {
      expect(UI_GLYPHS[i].code).toBe(fromC[i].code);
      expect([...UI_GLYPHS[i].rows]).toEqual(fromC[i].rows);
    }
  });

  it('has every label the reader and bookshelf show', () => {
    const need = [
      0xc9cf, 0xcfc2, 0xd2bb, 0xd2b3, // 上 下 一 页
      0xbbd8, 0xcae9, 0xbcdc, // 回 书 架
      0xd7d6, 0xcce5, // 字 体
      0xd4c4, 0xb6c1, // 阅 读
      0xc9be, 0xb3fd, // 删 除
      0xccf8, 0xd7aa, // 跳 转
      0xd6c1, 0xc2eb, // 至 码
      0xc8a1, 0xcffb, // 取 消
    ];
    expect(need).toHaveLength(UI_GLYPHS.length);
    for (const code of need) {
      expect(uiGlyphRows(code), code.toString(16)).not.toBeNull();
    }
  });
});

describe('glyph font loader', () => {
  it('parses the 8x8 font', () => {
    expect(font8).not.toBeNull();
    expect(font8!.cell).toBe(8);
    expect(font8!.bytesPerGlyph).toBe(8);
    expect(font8!.pageGlyphs).toBe(2048);
    expect(font8!.firstRow).toBe(0xa1);
  });

  it('parses the 12x12 font', () => {
    expect(font12).not.toBeNull();
    expect(font12!.cell).toBe(12);
    expect(font12!.bytesPerGlyph).toBe(24);
    expect(font12!.pageGlyphs).toBe(512);
  });
});

describe('screen renderer', () => {
  it('draws inverted header/footer, body and progress bar', () => {
    const bits = renderScreen(
      {
        displayName: 'QZW',
        page: { lines: ['天地玄黄，宇宙洪荒。日月盈亏'] },
        pageNumber: 2,
        totalPages: 4,
      },
      font8,
      8,
    );
    expect(bits.length).toBe(SCREEN_W * SCREEN_H);

    // Header band is black with white glyphs: mostly black.
    let headerInk = 0;
    for (let y = 0; y < 8; y++) {
      for (let x = 0; x < SCREEN_W; x++) headerInk += bits[y * SCREEN_W + x];
    }
    expect(headerInk).toBeGreaterThan(8 * 160 * 0.7);

    // Body band is white with black glyphs: far less ink than the band size.
    let bodyInk = 0;
    for (let y = 8; y < 150; y++) {
      for (let x = 0; x < SCREEN_W; x++) bodyInk += bits[y * SCREEN_W + x];
    }
    expect(bodyInk).toBeGreaterThan(100);
    expect(bodyInk).toBeLessThan(8 * 142 * 0.5);

    // Footer band is black too.
    let footerInk = 0;
    for (let y = 150; y < 158; y++) {
      for (let x = 0; x < SCREEN_W; x++) footerInk += bits[y * SCREEN_W + x];
    }
    expect(footerInk).toBeGreaterThan(8 * 160 * 0.7);

    // Progress: row 158 fully white (0), row 159 white for half the width.
    let borderInk = 0;
    for (let x = 0; x < SCREEN_W; x++) borderInk += bits[158 * SCREEN_W + x];
    expect(borderInk).toBe(0);

    let fillInk = 0;
    for (let x = 0; x < SCREEN_W; x++) fillInk += bits[159 * SCREEN_W + x];
    // page 2/4 => 80 px filled, 80 px black.
    expect(fillInk).toBe(80);
  });

  it('draws the Chinese footer labels from the built-in glyphs', () => {
    const bits = renderScreen(
      { displayName: 'X', page: { lines: ['天'] }, pageNumber: 1, totalPages: 1 },
      font8,
      8,
    );

    // White ink (0) inside the black footer band, column by column.
    const inkAt = (x: number): boolean => {
      for (let y = 150; y < 158; y++) {
        if (bits[y * SCREEN_W + x] === 0) return true;
      }
      return false;
    };

    const span = (from: number, to: number): { first: number; last: number } => {
      let first = -1;
      let last = -1;
      for (let x = from; x <= to; x++) {
        if (!inkAt(x)) continue;
        if (first < 0) first = x;
        last = x;
      }
      return { first, last };
    };

    // Four 8px cells per 40px column. The glyphs keep a blank 8th column, so
    // ink stops one pixel short of each cell edge.
    const prev = span(0, 39);
    const goto = span(40, 79);
    const shelf = span(80, 119);
    const next = span(120, 159);

    expect(prev.first).toBe(FOOTER_PREV_X);
    expect(prev.last).toBeLessThan(FOOTER_PREV_X + 3 * 8);
    expect(goto.first).toBe(FOOTER_GOTO_X);
    expect(goto.last).toBeLessThan(FOOTER_GOTO_X + 2 * 8);
    expect(shelf.first).toBe(FOOTER_SHELF_X);
    expect(shelf.last).toBeLessThan(FOOTER_SHELF_X + 3 * 8);
    expect(next.first).toBe(FOOTER_NEXT_X);
    expect(next.last).toBeLessThan(FOOTER_NEXT_X + 3 * 8);

    // Every label is centred inside its own quarter of the screen.
    for (const [zone, x, count] of [
      [0, FOOTER_PREV_X, 3],
      [1, FOOTER_GOTO_X, 2],
      [2, FOOTER_SHELF_X, 3],
      [3, FOOTER_NEXT_X, 3],
    ] as const) {
      const centre = x + (count * 8) / 2;
      expect(centre).toBe(zone * 40 + 20);
    }

    // No ink spills between the labels.
    expect(span(32, 51)).toEqual({ first: -1, last: -1 });
    expect(span(68, 87)).toEqual({ first: -1, last: -1 });
    expect(span(112, 127)).toEqual({ first: -1, last: -1 });
  });

  it('centres the body vertically', () => {
    const bits = renderScreen(
      { displayName: 'X', page: { lines: ['天'] }, pageNumber: 1, totalPages: 1 },
      font8,
      8,
    );
    // 1 line of 8px in a 142px band: top = 8 + floor((142-8)/2) = 8 + 67 = 75.
    // The glyph 天 has an empty first row, so the first inked row is 76.
    let firstInkRow = -1;
    for (let y = 8; y < 150 && firstInkRow < 0; y++) {
      for (let x = 0; x < SCREEN_W; x++) {
        if (bits[y * SCREEN_W + x] !== 0) { firstInkRow = y; break; }
      }
    }
    expect(firstInkRow).toBe(76);
  });

  it('renders a blank page as a blank body', () => {
    const bits = renderScreen(
      { displayName: 'X', page: { lines: [] }, pageNumber: 1, totalPages: 1 },
      font8,
      8,
    );
    let bodyInk = 0;
    for (let y = 8; y < 150; y++) {
      for (let x = 0; x < SCREEN_W; x++) bodyInk += bits[y * SCREEN_W + x];
    }
    expect(bodyInk).toBe(0);
  });

  it('paints to a canvas at the requested scale', () => {
    const canvas = document.createElement('canvas');
    const bits = new Uint8Array(SCREEN_W * SCREEN_H);
    bits[0] = 1; // top-left pixel
    paintToCanvas(canvas, bits, 4);
    expect(canvas.width).toBe(SCREEN_W * 4);
    expect(canvas.height).toBe(SCREEN_H * 4);

    // jsdom has no 2D backend unless node-canvas is installed; skip pixel
    // assertions when that is the case.
    const ctx = canvas.getContext('2d');
    if (ctx === null) return;

    const px = ctx.getImageData(0, 0, 1, 1).data;
    expect(px[0]).toBe(0); // black
    const bg = ctx.getImageData(4, 4, 1, 1).data;
    expect(bg[0]).toBe(255); // white
  });
});

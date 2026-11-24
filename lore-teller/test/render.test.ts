// DOM-level smoke test for the preview renderer and the font loader.
// Runs under jsdom; verifies that a page actually paints pixels and that the
// header/footer ASCII font is used.

import { describe, expect, it } from 'vitest';
import { parseGlyphFont } from '../src/lib/glyphFont';
import { paintToCanvas, renderScreen, SCREEN_H, SCREEN_W } from '../src/lib/render';
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
  it('draws header, body and footer pixels', () => {
    const bits = renderScreen(
      {
        displayName: 'QZW',
        page: { lines: ['天地玄黄，宇宙洪荒。日月盈亏'] },
        pageNumber: 1,
        totalPages: 4,
      },
      font8,
      8,
    );
    expect(bits.length).toBe(SCREEN_W * SCREEN_H);

    // Header row 0 must have some ink (display name).
    let headerInk = 0;
    for (let x = 0; x < SCREEN_W; x++) headerInk += bits[x];
    expect(headerInk).toBeGreaterThan(0);

    // Body band must have ink.
    let bodyInk = 0;
    for (let y = 8; y < 152; y++) {
      for (let x = 0; x < SCREEN_W; x++) bodyInk += bits[y * SCREEN_W + x];
    }
    expect(bodyInk).toBeGreaterThan(100);

    // Footer row 152 must have ink from PREV/MENU/NEXT.
    let footerInk = 0;
    for (let y = 152; y < 160; y++) {
      for (let x = 0; x < SCREEN_W; x++) footerInk += bits[y * SCREEN_W + x];
    }
    expect(footerInk).toBeGreaterThan(0);
  });

  it('renders a blank page as blank body', () => {
    const bits = renderScreen(
      { displayName: 'X', page: { lines: [] }, pageNumber: 1, totalPages: 1 },
      font8,
      8,
    );
    let bodyInk = 0;
    for (let y = 8; y < 152; y++) {
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

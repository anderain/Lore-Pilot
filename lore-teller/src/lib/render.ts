// 160x160 Palm screen renderer for the preview panel.
//
// Layout (from the plan):
//   y 0..7     6x8 ASCII, left: displayName, right: page/total
//   y 8..151   body: glyph font cell size 8 or 12
//   y 152..159 6x8 ASCII: PREV  MENU  NEXT

import { HYBRID_FONT_BYTES, HYBRID_FONT_HEIGHT } from '../fonts/hybrid6x8';
import { glyphFor, type GlyphFont } from './glyphFont';
import { lookupGb } from './gb2312';
import type { Page } from './paginate';

export const SCREEN_W = 160;
export const SCREEN_H = 160;
export const HEADER_ROWS = 8;
export const FOOTER_Y = 152;

export interface RenderState {
  displayName: string;
  page: Page;
  pageNumber: number;
  totalPages: number;
}

/** Draw one 6x8 ASCII glyph at pixel position. */
function drawAscii(
  bits: Uint8Array,
  ch: string,
  x: number,
  y: number,
): void {
  const index = ch.charCodeAt(0);
  if (index < 0x20 || index > 0x7e) return;
  const base = index * HYBRID_FONT_HEIGHT;
  for (let row = 0; row < HYBRID_FONT_HEIGHT; row++) {
    const byte = HYBRID_FONT_BYTES[base + row];
    for (let col = 0; col < 6; col++) {
      if ((byte & (0x80 >> col)) === 0) continue;
      const px = x + col;
      const py = y + row;
      if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) continue;
      bits[py * SCREEN_W + px] = 1;
    }
  }
}

function drawAsciiText(bits: Uint8Array, text: string, x: number, y: number): void {
  let cx = x;
  for (const ch of text) {
    drawAscii(bits, ch, cx, y);
    cx += 6;
  }
}

/** Draw a body glyph from the glyph font. */
function drawGlyph(
  bits: Uint8Array,
  font: GlyphFont,
  code: number,
  x: number,
  y: number,
): void {
  const glyph = glyphFor(font, code);
  if (glyph === null) return;
  const { cell, rowBytes } = font;
  for (let row = 0; row < cell; row++) {
    for (let col = 0; col < cell; col++) {
      const byte = glyph[row * rowBytes + (col >> 3)];
      if ((byte & (0x80 >> (col & 7))) === 0) continue;
      const px = x + col;
      const py = y + row;
      if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) continue;
      bits[py * SCREEN_W + px] = 1;
    }
  }
}

/** Render one page into a 1bpp bit array (one byte per pixel here). */
export function renderScreen(
  state: RenderState,
  font: GlyphFont | null,
  cell: 8 | 12,
): Uint8Array {
  const bits = new Uint8Array(SCREEN_W * SCREEN_H);

  // Header: displayName left, page/total right.
  drawAsciiText(bits, state.displayName, 0, 0);
  const pageLabel = `${state.pageNumber}/${state.totalPages}`;
  drawAsciiText(bits, pageLabel, SCREEN_W - pageLabel.length * 6, 0);

  // Body.
  if (font !== null) {
    const perLine = Math.floor(SCREEN_W / cell);
    const linesPerPage = Math.floor((FOOTER_Y - HEADER_ROWS) / cell);
    const bodyY = HEADER_ROWS;
    for (let lineIndex = 0; lineIndex < state.page.lines.length && lineIndex < linesPerPage; lineIndex++) {
      const line = state.page.lines[lineIndex];
      let x = 0;
      let count = 0;
      for (const ch of line) {
        if (count >= perLine) break;
        const hit = lookupGb(ch);
        if (hit !== null) drawGlyph(bits, font, hit.code, x, bodyY + lineIndex * cell);
        x += cell;
        count++;
      }
    }
  }

  // Footer.
  drawAsciiText(bits, 'PREV', 2, FOOTER_Y);
  drawAsciiText(bits, 'MENU', 74, FOOTER_Y);
  drawAsciiText(bits, 'NEXT', 134, FOOTER_Y);

  return bits;
}

/** Paint the bit array onto a canvas, scaled by an integer factor. */
export function paintToCanvas(
  canvas: HTMLCanvasElement,
  bits: Uint8Array,
  scale: number,
): void {
  canvas.width = SCREEN_W * scale;
  canvas.height = SCREEN_H * scale;

  const ctx = canvas.getContext('2d');
  if (!ctx) return;

  const image = ctx.createImageData(canvas.width, canvas.height);
  for (let y = 0; y < SCREEN_H; y++) {
    for (let x = 0; x < SCREEN_W; x++) {
      const on = bits[y * SCREEN_W + x] !== 0;
      for (let dy = 0; dy < scale; dy++) {
        for (let dx = 0; dx < scale; dx++) {
          const px = x * scale + dx;
          const py = y * scale + dy;
          const o = (py * canvas.width + px) * 4;
          const value = on ? 0 : 255;
          image.data[o] = value;
          image.data[o + 1] = value;
          image.data[o + 2] = value;
          image.data[o + 3] = 255;
        }
      }
    }
  }
  ctx.putImageData(image, 0, 0);
}

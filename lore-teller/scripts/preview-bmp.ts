// Render a page of a text PDB to a BMP for visual inspection.
//
//   npx tsx scripts/preview-bmp.ts <part.pdb> [pageIndex] [out.bmp]
//
// Uses the same renderer as the web preview, so the image matches what the
// browser shows (and what the Palm reader should draw).

import fs from 'node:fs';
import path from 'node:path';
import { parseGlyphFont, type GlyphFont } from '../src/lib/glyphFont';
import { renderScreen, SCREEN_H, SCREEN_W } from '../src/lib/render';

const input = process.argv[2];
if (!input) {
  console.error('usage: npx tsx scripts/preview-bmp.ts <part.pdb> [pageIndex] [out.bmp]');
  process.exit(1);
}
const pageIndex = Number(process.argv[3] ?? 0);
const output = process.argv[4] ?? path.join(path.dirname(input), `preview-${pageIndex}.bmp`);
const scale = 4;

function loadFont(file: string): GlyphFont | null {
  const full = path.resolve('public', 'fonts', file);
  if (!fs.existsSync(full)) return null;
  const raw = fs.readFileSync(full);
  const buf = raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength) as ArrayBuffer;
  return parseGlyphFont(buf, file);
}

function readPart(buf: Uint8Array) {
  const be16 = (o: number) => (buf[o] << 8) | buf[o + 1];
  const be32 = (o: number) =>
    ((buf[o] << 24) | (buf[o + 1] << 16) | (buf[o + 2] << 8) | buf[o + 3]) >>> 0;
  const num = be16(76);
  const offsets: number[] = [];
  for (let i = 0; i < num; i++) offsets.push(be32(78 + i * 8));
  const header = buf.subarray(offsets[0], offsets[0] + 64);
  const h16 = (o: number) => (header[o] << 8) | header[o + 1];
  const h32 = (o: number) =>
    ((header[o] << 24) | (header[o + 1] << 16) | (header[o + 2] << 8) | header[o + 3]) >>> 0;

  const pages: string[][] = [];
  const decoder = new TextDecoder('gbk');
  for (let i = 1; i < offsets.length; i++) {
    const rec = buf.subarray(offsets[i], i + 1 < offsets.length ? offsets[i + 1] : buf.length);
    const r16 = (o: number) => (rec[o] << 8) | rec[o + 1];
    const lineCount = r16(0);
    const ends: number[] = [];
    for (let k = 0; k < lineCount; k++) ends.push(r16(4 + k * 2));
    const textStart = 4 + lineCount * 2 + 2;
    const text = rec.subarray(textStart, textStart + r16(textStart - 2));
    const lines: string[] = [];
    let prev = 0;
    for (const lineEnd of ends) {
      lines.push(decoder.decode(text.subarray(prev, lineEnd)));
      prev = lineEnd;
    }
    pages.push(lines);
  }

  return {
    name: buf.toString('ascii', 0, 32).replace(/\0.*$/, ''),
    cell: h16(8) as 8 | 12,
    displayName: header.toString('ascii', 44, 60).replace(/\0.*$/, ''),
    firstPage: h32(24),
    totalPages: h32(32),
    pages,
  };
}

function writeBmp(file: string, bits: Uint8Array, scaleFactor: number) {
  const w = SCREEN_W * scaleFactor;
  const h = SCREEN_H * scaleFactor;
  const rowBytes = Math.ceil(w / 4) * 4;
  const dataSize = rowBytes * h;
  const out = Buffer.alloc(54 + dataSize);

  out.write('BM', 0, 'ascii');
  out.writeUInt32LE(54 + dataSize, 2);
  out.writeUInt32LE(54, 10);
  out.writeUInt32LE(40, 14);
  out.writeInt32LE(w, 18);
  out.writeInt32LE(-h, 22);       // top-down
  out.writeUInt16LE(1, 26);
  out.writeUInt16LE(24, 28);
  out.writeUInt32LE(dataSize, 34);

  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const sx = Math.floor(x / scaleFactor);
      const sy = Math.floor(y / scaleFactor);
      const on = bits[sy * SCREEN_W + sx] !== 0;
      const v = on ? 0 : 255;
      const o = 54 + y * rowBytes + x * 3;
      out[o] = v;
      out[o + 1] = v;
      out[o + 2] = v;
    }
  }
  fs.writeFileSync(file, out);
}

const raw = fs.readFileSync(input);
const part = readPart(raw);
if (pageIndex < 0 || pageIndex >= part.pages.length) {
  console.error(`page index ${pageIndex} out of range 1..${part.pages.length}`);
  process.exit(1);
}

const font = loadFont(part.cell === 12 ? 'LPF12F.pdb' : 'LPF08F.pdb');
if (font === null) console.error('warning: glyph font not found, body will be blank');

const bits = renderScreen(
  {
    displayName: part.displayName,
    page: { lines: part.pages[pageIndex] },
    pageNumber: part.firstPage + pageIndex,
    totalPages: part.totalPages,
  },
  font,
  part.cell,
);

writeBmp(output, bits, scale);
console.log(`wrote ${output} (page ${part.firstPage + pageIndex}/${part.totalPages}, cell ${part.cell})`);

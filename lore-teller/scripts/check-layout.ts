// Report the exact ink layout of a rendered page, for quick verification.

import fs from 'node:fs';
import { parseGlyphFont, type GlyphFont } from '../src/lib/glyphFont';
import { renderScreen, SCREEN_W, SCREEN_H } from '../src/lib/render';

function loadFont(file: string): GlyphFont | null {
  const full = `public/fonts/${file}`;
  if (!fs.existsSync(full)) return null;
  const raw = fs.readFileSync(full);
  const buf = raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength) as ArrayBuffer;
  return parseGlyphFont(buf, file);
}

const font8 = loadFont('LPF08F.pdb');
const font12 = loadFont('LPF12F.pdb');

for (const [cell, font] of [[8, font8], [12, font12]] as const) {
  const lines = cell === 8 ? Array(17).fill('天地玄黄宇宙洪荒日月盈') : Array(11).fill('天地玄黄宇宙洪荒日月');
  const bits = renderScreen(
    { displayName: 'TEST', page: { lines }, pageNumber: 5, totalPages: 20 },
    font,
    cell,
  );
  const rowInk = (y: number) => {
    let n = 0;
    for (let x = 0; x < SCREEN_W; x++) n += bits[y * SCREEN_W + x];
    return n;
  };

  const header = [0, 1, 2, 3, 4, 5, 6, 7].map(rowInk);
  const footer = [150, 151, 152, 153, 154, 155, 156, 157].map(rowInk);
  let first = -1;
  let last = -1;
  for (let y = 8; y < 150; y++) {
    if (rowInk(y) > 0) {
      if (first < 0) first = y;
      last = y;
    }
  }
  console.log(`${cell}px: header ink ${header.join(',')}`);
  console.log(`      footer ink ${footer.join(',')}`);
  console.log(`      body ${first}..${last} (band 8..149)`);
  console.log(`      progress border row158 ink=${rowInk(158)} (0=white line), fill row159 ink=${rowInk(159)} (5/20 => 40px filled => 120)`);
  console.log(`      screen ${SCREEN_W}x${SCREEN_H}`);
}

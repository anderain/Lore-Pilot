// Print the footer band of a rendered page as ASCII art, so the built-in
// Chinese labels can be eyeballed without a device.
//
//   npx tsx scripts/show-footer.ts [cell]

import fs from 'node:fs';
import {
  FOOTER_GOTO_X,
  FOOTER_NEXT_X,
  FOOTER_PREV_X,
  FOOTER_SHELF_X,
  renderScreen,
  SCREEN_W,
} from '../src/lib/render';
import { parseGlyphFont } from '../src/lib/glyphFont';

const cell = Number(process.argv[2] ?? 8) as 8 | 12;
const file = cell === 12 ? 'public/fonts/LPF12F.pdb' : 'public/fonts/LPF08F.pdb';
const raw = fs.readFileSync(file);
const font = parseGlyphFont(
  raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength) as ArrayBuffer,
  file,
);

const bits = renderScreen(
  {
    displayName: 'BOOK',
    page: { lines: ['天地玄黄宇宙洪荒日月盈'] },
    pageNumber: 3,
    totalPages: 8,
  },
  font,
  cell,
);

const crops: Array<[string, number, number]> = [
  ['prev  上一页', FOOTER_PREV_X, FOOTER_PREV_X + 3 * 8 - 1],
  ['goto  跳转  ', FOOTER_GOTO_X, FOOTER_GOTO_X + 2 * 8 - 1],
  ['shelf 回书架', FOOTER_SHELF_X, FOOTER_SHELF_X + 3 * 8 - 1],
  ['next  下一页', FOOTER_NEXT_X, FOOTER_NEXT_X + 3 * 8 - 1],
];

for (const [label, x0, x1] of crops) {
  console.log(`${label}  (x ${x0}..${x1})`);
  for (let y = 150; y < 158; y++) {
    let line = '';
    for (let x = x0; x <= x1; x++) line += bits[y * SCREEN_W + x] ? '.' : '#';
    console.log('  ' + line);
  }
  console.log();
}

// Generate a text PDB from a UTF-8/GBK TXT without the browser.
//
//   npx tsx scripts/make-sample.ts <input.txt> [outDir] [cell] [partKb]
//
// Uses the same pipeline as the web app; handy for eyeballing the format with
// scripts/inspect-text-pdb.mjs.

import fs from 'node:fs';
import path from 'node:path';
import { convertText, displayNameFromFile } from '../src/lib/convert';
import { decodeText } from '../src/lib/decode';

const input = process.argv[2];
if (!input) {
  console.error('usage: npx tsx scripts/make-sample.ts <input.txt> [outDir] [cell=8] [partKb=64]');
  process.exit(1);
}

const outDir = process.argv[3] ?? path.dirname(input);
const cell = Number(process.argv[4] ?? 8) as 8 | 12;
const partKb = Number(process.argv[5] ?? 64);

if (cell !== 8 && cell !== 12) {
  console.error('cell must be 8 or 12');
  process.exit(1);
}

const raw = fs.readFileSync(input);
const buffer = raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength) as ArrayBuffer;

const decoded = decodeText(buffer);
const displayName = displayNameFromFile(path.basename(input));
const result = convertText(buffer, {
  displayName,
  cell,
  partLimit: partKb * 1024,
  encoding: decoded.encoding,
});

fs.mkdirSync(outDir, { recursive: true });
for (const part of result.parts) {
  const out = path.join(outDir, part.fileName);
  fs.writeFileSync(out, part.bytes);
  const last = part.firstPage + part.pageCount - 1;
  console.log(`wrote ${out} (${part.size} bytes, pages ${part.firstPage}..${last})`);
}
console.log(
  `${result.charCount} chars, ${result.lineCount} lines, ${result.totalPages} pages, ` +
    `${result.parts.length} part(s), cell ${cell}`,
);
if (result.replaced.length) console.log(`replaced: ${result.replaced.join('')}`);

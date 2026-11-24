// Generate text PDBs from a UTF-8/GBK TXT without the browser.
//
//   npx tsx scripts/make-sample.ts <input.txt> [outDir] [cell] [pagesPerPart]
//
// Uses the same pipeline as the web app; handy for eyeballing the format with
// scripts/inspect-text-pdb.mjs.

import fs from 'node:fs';
import path from 'node:path';
import { convertText, displayNameFromFile } from '../src/lib/convert';
import { decodeText } from '../src/lib/decode';
import { DEFAULT_PAGES_PER_PART } from '../src/lib/split';

const input = process.argv[2];
if (!input) {
  console.error(
    'usage: npx tsx scripts/make-sample.ts <input.txt> [outDir] [cell=8] [pagesPerPart=100]',
  );
  process.exit(1);
}

const outDir = process.argv[3] ?? path.dirname(input);
const cell = Number(process.argv[4] ?? 8) as 8 | 12;
const pagesPerPart = Number(process.argv[5] ?? DEFAULT_PAGES_PER_PART);

if (cell !== 8 && cell !== 12) {
  console.error('cell must be 8 or 12');
  process.exit(1);
}
if (!Number.isFinite(pagesPerPart) || pagesPerPart < 1) {
  console.error('pagesPerPart must be a positive integer');
  process.exit(1);
}

const raw = fs.readFileSync(input);
const buffer = raw.buffer.slice(raw.byteOffset, raw.byteOffset + raw.byteLength) as ArrayBuffer;

const decoded = decodeText(buffer);
const displayName = displayNameFromFile(path.basename(input));
const result = convertText(buffer, {
  displayName,
  cell,
  pagesPerPart,
  encoding: decoded.encoding,
});

fs.mkdirSync(outDir, { recursive: true });
for (const part of result.parts) {
  const out = path.join(outDir, part.fileName);
  fs.writeFileSync(out, part.bytes);
  const last = part.firstPage + part.pageCount - 1;
  console.log(
    `wrote ${out} (${part.size} bytes, volume ${part.partIndex + 1}/${part.partCount}, ` +
      `pages ${part.firstPage}..${last})`,
  );
}
console.log(
  `${result.charCount} chars, ${result.lineCount} lines, ${result.totalPages} pages, ` +
    `${result.parts.length} volume(s), cell ${cell}, ${pagesPerPart} pages/volume`,
);
if (result.replaced.length) console.log(`replaced: ${result.replaced.join('')}`);

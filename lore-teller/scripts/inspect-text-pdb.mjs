// Inspect a text PDB produced by lore-teller.
//
//   node scripts/inspect-text-pdb.mjs path/to/LPB_BOOK_01.pdb
//
// Prints the header and every page's line offsets and decoded text, so the
// format can be checked without opening the web app.

import fs from 'node:fs';

const file = process.argv[2];
if (!file) {
  console.error('usage: node scripts/inspect-text-pdb.mjs <file.pdb>');
  process.exit(1);
}

const buf = fs.readFileSync(file);
const u16 = (o) => (buf[o] << 8) | buf[o + 1];
const u32 = (o) =>
  ((buf[o] << 24) | (buf[o + 1] << 16) | (buf[o + 2] << 8) | buf[o + 3]) >>> 0;
const ascii = (start, end) => buf.toString('ascii', start, end).replace(/\0.*$/, '');

const numRecords = u16(76);
const offsets = [];
for (let i = 0; i < numRecords; i++) offsets.push(u32(78 + i * 8));

const header = buf.subarray(offsets[0], offsets[0] + 64);
const h16 = (o) => (header[o] << 8) | header[o + 1];
const h32 = (o) =>
  ((header[o] << 24) | (header[o + 1] << 16) | (header[o + 2] << 8) | header[o + 3]) >>> 0;

const magic = header.toString('ascii', 0, 4);
console.log(`file          ${file} (${buf.length} bytes)`);
console.log(`db name       ${ascii(0, 32)}`);
console.log(`type/creator  ${ascii(60, 64)}/${ascii(64, 68)}`);
console.log(`records       ${numRecords} (1 header + ${numRecords - 1} pages)`);
console.log(`magic         ${magic}${magic === 'LPTX' ? '' : '  <-- unexpected'}`);
console.log(`version       ${h16(4)}`);
console.log(`cell          ${h16(8)}x${h16(8)}`);
console.log(`chars/line    ${h16(10)}`);
console.log(`lines/page    ${h16(12)}`);
console.log(`header/footer ${h16(14)}/${h16(16)} rows`);
console.log(`progress      ${h16(42)} rows (0 = old layout)`);
console.log(`part          ${h16(18) + 1}/${h16(20)}`);
console.log(`pages         first ${h32(24)}, this part ${h32(28)}, total ${h32(32)}`);
console.log(`display name  ${header.toString('ascii', 44, 60).replace(/\0.*$/, '')}`);
console.log(`record 0 size ${offsets[1] - offsets[0]} bytes`);

const decoder = new TextDecoder('gbk');
let totalChars = 0;

for (let i = 1; i < offsets.length; i++) {
  const start = offsets[i];
  const end = i + 1 < offsets.length ? offsets[i + 1] : buf.length;
  const rec = buf.subarray(start, end);
  const r16 = (o) => (rec[o] << 8) | rec[o + 1];
  const lineCount = r16(0);
  const ends = [];
  for (let k = 0; k < lineCount; k++) ends.push(r16(4 + k * 2));
  const textStart = 4 + lineCount * 2 + 2;
  const textLen = r16(textStart - 2);
  const text = rec.subarray(textStart, textStart + textLen);

  console.log(`\npage ${i}  record ${start}..${end - 1}  ${rec.length} bytes  ${lineCount} lines  ${textLen} text bytes`);
  let prev = 0;
  ends.forEach((lineEnd, k) => {
    const line = decoder.decode(text.subarray(prev, lineEnd));
    totalChars += [...line].length;
    console.log(`  ${String(k + 1).padStart(2)} [${String(prev).padStart(3)}..${String(lineEnd - 1).padStart(3)}] ${line}`);
    prev = lineEnd;
  });
}

console.log(`\ncharacters on this part: ${totalChars}`);

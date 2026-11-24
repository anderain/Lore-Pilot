// Emulate the C reader's parsing of an LPBK PDB and print the resulting
// layout, so the Palm side can be checked without a device.
//
//   node script  # no argument needed

import fs from 'node:fs';

const HEADER_ROWS_DEFAULT = 8;
const FOOTER_ROWS_DEFAULT = 8;
const SCREEN = 160;

function parseHeader(h) {
  const u16 = (o) => (h[o] << 8) | h[o + 1];
  const u32 = (o) => ((h[o] << 24) | (h[o + 1] << 16) | (h[o + 2] << 8) | h[o + 3]) >>> 0;
  const name = [];
  for (let i = 0; i < 16 && h[44 + i]; i++) name.push(String.fromCharCode(h[44 + i]));
  let progressRows = u16(42);
  if (progressRows !== 0 && progressRows > 4) progressRows = 0;
  return {
    cell: u16(8),
    charsPerLine: u16(10),
    linesPerPage: u16(12),
    headerRows: u16(14) || HEADER_ROWS_DEFAULT,
    footerRows: u16(16) || FOOTER_ROWS_DEFAULT,
    partIndex: u16(18),
    partCount: u16(20),
    firstPage: u32(24),
    pageCount: u32(28),
    totalPages: u32(32),
    progressRows,
    displayName: name.join(''),
    progressReserved: u16(42),
  };
}

const file = process.argv[2];
if (!file) {
  console.error('usage: node scripts/check-reader-layout.mjs <LPB_*.pdb>');
  process.exit(1);
}

const b = fs.readFileSync(file);
const numRecords = (b[76] << 8) | b[77];
const entry = (i) => {
  const base = 78 + i * 8;
  return ((b[base] << 24) | (b[base + 1] << 16) | (b[base + 2] << 8) | b[base + 3]) >>> 0;
};

const header = parseHeader(b.subarray(entry(0), entry(1)));

// Mirror ReaderBodyTop / ReaderBodyBottom / ReaderCharsPerLine.
const bodyTop = header.headerRows;
const bodyBottom = SCREEN - (header.footerRows + header.progressRows);
const bodyHeight = bodyBottom - bodyTop;
const perLine = Math.min(Math.floor(SCREEN / header.cell), header.charsPerLine || 999);
const maxLines = Math.floor(bodyHeight / header.cell);

console.log(`${file}`);
console.log(
  `  header: cell ${header.cell}, ${perLine} chars/line, ${header.linesPerPage} lines/page, ` +
    `progressRows ${header.progressRows} (raw ${header.progressReserved})`,
);
console.log(`  part ${header.partIndex + 1}/${header.partCount}, pages ${header.firstPage}+, ` +
            `this part ${header.pageCount}, total ${header.totalPages}`);
console.log(`  body y ${bodyTop}..${bodyBottom - 1} (${bodyHeight}px), max ${maxLines} lines`);
console.log(`  footer y ${bodyBottom}..${SCREEN - 1}`);

const expectedTop = bodyTop + Math.floor((bodyHeight - maxLines * header.cell) / 2);
console.log(`  full page first line y = ${expectedTop}`);

let problems = 0;
for (let p = 0; p < Math.min(header.pageCount, numRecords - 1); p++) {
  const rec = b.subarray(entry(1 + p), entry(1 + p + 1));
  const u16 = (o) => (rec[o] << 8) | rec[o + 1];
  const lineCount = u16(0);
  const prevEnd = [];
  let last = 0;
  for (let i = 0; i < lineCount; i++) {
    const end = u16(4 + i * 2);
    if (end < last) problems++;
    prevEnd.push({ start: last, len: end - last });
    last = end;
  }
  const textLen = u16(4 + lineCount * 2);
  if (textLen !== last) {
    console.log(`  page ${p + 1}: textLen ${textLen} != line table ${last}`);
    problems++;
  }
  // Every line must be a even number of bytes: GB2312 double bytes only.
  for (const line of prevEnd) {
    if (line.len % 2 !== 0) {
      console.log(`  page ${p + 1}: odd line length ${line.len}`);
      problems++;
      break;
    }
    if (line.len > perLine * 2) {
      console.log(`  page ${p + 1}: line too long ${line.len} > ${perLine * 2}`);
      problems++;
      break;
    }
  }
  if (lineCount > maxLines) {
    console.log(`  page ${p + 1}: ${lineCount} lines > max ${maxLines}, would clip`);
    problems++;
  }
  if (p === 0) {
    const used = Math.min(lineCount, maxLines);
    const top = bodyTop + Math.floor((bodyHeight - used * header.cell) / 2);
    console.log(`  page 1: ${lineCount} lines -> drawn y ${top}..${top + used * header.cell - 1}`);
  }
}

console.log(problems === 0 ? '  OK: all pages parse and fit' : `  ${problems} problem(s)`);

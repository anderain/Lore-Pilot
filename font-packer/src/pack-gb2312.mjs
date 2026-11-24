#!/usr/bin/env node
// Pack GB2312 glyphs into a paged Palm OS record database.
//
//   node font-packer/src/pack-gb2312.mjs --level basic            # 8x8, A1-D7
//   node font-packer/src/pack-gb2312.mjs --level full             # 8x8, A1-F7
//   node font-packer/src/pack-gb2312.mjs --level basic --cell 12  # 12x12, A1-D7
//
// Database names follow LPF<cell><level>: LPF08B, LPF08F, LPF08H,
// LPF12B, LPF12F, LPF12H. Cell is zero-padded so name order matches size.
//
// The layout is page-major with a dense O(1) mapping:
//   index = (hi - gbFirstRow) * gbCols + (lo - gbFirstCol)
//   page  = index / gbPageGlyphs
//   off   = (index % gbPageGlyphs) * bytesPerGlyph
// Records 1..N hold one page each (gbPageGlyphs * bytesPerGlyph bytes).

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { loadFont } from './ttf.mjs';
import { buildPdb, parsePdb, PdbError, writePdb, PDB_HEADER_SIZE } from './pdb.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const PACKER_ROOT = path.resolve(HERE, '..');
const WORKSPACE_ROOT = path.resolve(PACKER_ROOT, '..');

const FONT_HEADER_SIZE = 48;

// Page size stays near 12-16KB so a single record is small and the reader's
// two-to-three page cache fits a 1MB device.
const CELL_CONFIG = {
  8:  { cell: 8,  bytesPerGlyph: 8,  pageGlyphs: 2048, tag: '8',  sampleSuffix: ''   },
  12: { cell: 12, bytesPerGlyph: 24, pageGlyphs: 512,  tag: '12', sampleSuffix: '12' },
};

const LEVELS = {
  basic: { firstRow: 0xa1, lastRow: 0xd7, tag: 'B', label: 'A1-D7' },
  full:  { firstRow: 0xa1, lastRow: 0xf7, tag: 'F', label: 'A1-F7' },
  hanzi: { firstRow: 0xb0, lastRow: 0xd7, tag: 'H', label: 'B0-D7' },
};

const DEFAULTS = {
  level: 'basic',
  cell: '8',
  font: null,                // derived from cell
  pdb: null,                 // derived from cell + level
  name: null,                // derived from cell + level
  samples: null,             // derived from cell
  text: path.join(WORKSPACE_ROOT, 'reader', 'Src', 'GbText.h'),
  'text-file': null,         // derived from level
  verify: true,
  selftest: true,
  quiet: false,
};

const SAMPLE_TEXT = '天地玄黄宇宙洪荒日月';

class PackError extends Error {
  constructor(code, message) {
    super(message);
    this.name = 'PackError';
    this.code = code;
  }
}

function missingFontMessage(file) {
  return [
    `font file not found: ${file}`,
    'The font is not distributed with this repository.',
    'Download "Fusion Pixel 8px/12px Mono zh-Hans" (SIL OFL 1.1) from',
    '  https://github.com/TakWolf/fusion-pixel-font/releases',
    'and place the TTF at the repository root, or pass --font <path>.',
  ].join('\n  ');
}

function parseArgs(argv) {
  const opts = { ...DEFAULTS };
  const valueKeys = new Set(['level', 'cell', 'font', 'pdb', 'name', 'samples', 'text', 'text-file']);

  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (!arg.startsWith('--')) throw new PackError('BAD_ARG', `unexpected argument: ${arg}`);
    let key = arg.slice(2);
    let value = null;
    const eq = key.indexOf('=');
    if (eq >= 0) {
      value = key.slice(eq + 1);
      key = key.slice(0, eq);
    }
    if (valueKeys.has(key)) {
      if (value === null) {
        if (i + 1 >= argv.length) throw new PackError('BAD_ARG', `--${key} needs a value`);
        value = argv[++i];
      }
      opts[key] = value;
    } else if (key === 'verify' || key === 'selftest' || key === 'quiet') {
      opts[key] = value === null ? true : !/^(0|no|false)$/i.test(value);
    } else if (key === 'no-verify') {
      opts.verify = false;
    } else if (key === 'help' || key === 'h') {
      printUsage();
      process.exit(0);
    } else {
      throw new PackError('BAD_ARG', `unknown option: --${key}`);
    }
  }

  let levelKey = String(opts.level).toLowerCase();
  if (levelKey === '1') levelKey = 'basic';
  if (levelKey === '2') levelKey = 'full';
  if (!LEVELS[levelKey]) {
    throw new PackError('BAD_LEVEL', `--level must be basic, full, or hanzi (got ${opts.level})`);
  }
  opts.level = levelKey;
  const level = LEVELS[levelKey];

  const cellKey = String(opts.cell).toLowerCase();
  if (!CELL_CONFIG[cellKey]) {
    throw new PackError('BAD_CELL', `--cell must be 8 or 12 (got ${opts.cell})`);
  }
  opts.cell = cellKey;
  const cfg = CELL_CONFIG[cellKey];

  const cellTag = cfg.cell < 10 ? `0${cfg.cell}` : `${cfg.cell}`;
  const base = `LPF${cellTag}${level.tag}`;
  if (!opts.font) {
    opts.font = path.join(WORKSPACE_ROOT,
      cfg.cell === 8
        ? 'fusion-pixel-8px-monospaced-zh_hans.ttf'
        : 'fusion-pixel-12px-monospaced-zh_hans.ttf');
  }
  if (!opts.pdb) opts.pdb = path.join(PACKER_ROOT, 'out', `${base}.pdb`);
  if (!opts.name) opts.name = base;
  if (!opts.samples) {
    opts.samples = path.join(WORKSPACE_ROOT, 'reader', 'Src',
      cfg.cell === 8 ? 'GbSamples.h' : 'GbSamples12.h');
  }
  if (!opts['text-file']) opts['text-file'] = path.join(PACKER_ROOT, 'data', 'qianziwen.txt');
  if (opts.name.length < 1 || opts.name.length > 31) {
    throw new PackError('BAD_ARG', `database name must be 1..31 chars, got ${opts.name.length}`);
  }
  return opts;
}

function printUsage() {
  console.log(`GB2312 -> Palm OS paged glyph database

  node font-packer/src/pack-gb2312.mjs [options]

Options:
  --level basic|1   A1-D7 (symbols + level-1 hanzi), default
  --level full|2    A1-F7 (symbols + level-1 + level-2 hanzi)
  --level hanzi     B0-D7 (hanzi only, no Chinese punctuation)
  --cell 8|12       glyph cell size (default 8)
  --font <file>     pixel TTF; default depends on --cell
  --pdb <file>      output database
                    (default: LPF08<B|F|H>.pdb for 8, LPF12<B|F|H>.pdb for 12)
  --name <name>     Palm database name (same default as --pdb)
  --samples <file>  sample answer header
                    (default: GbSamples.h for 8, GbSamples12.h for 12)
  --text <file>     demo text header (default: reader/Src/GbText.h)
  --text-file <f>   UTF-8 source text (default: font-packer/data/qianziwen.txt)
  --no-verify       skip re-parsing the written PDB
  --selftest        run negative tests on in-memory copies
  --quiet           only print errors
`);
}

function decodeGbTable(level) {
  const dec = new TextDecoder('gbk');
  const rows = level.lastRow - level.firstRow + 1;
  const cols = 94;
  const slots = new Map();       // code -> { code, cp, ch, index }
  const byChar = new Map();      // ch -> code (first occurrence)
  const gbk = new Map();         // ch -> any GBK code (includes non-GB2312)

  // Full GBK double-byte space, used for text encoding (not for the dense grid).
  for (let hi = 0x81; hi <= 0xfe; hi++) {
    for (let lo = 0x40; lo <= 0xfe; lo++) {
      if (lo === 0x7f) continue;
      const ch = dec.decode(Uint8Array.from([hi, lo]));
      if ([...ch].length !== 1) continue;
      const cp = ch.codePointAt(0);
      if (cp < 0x80 || cp === 0xfffd) continue;
      const code = (hi << 8) | lo;
      if (!gbk.has(ch)) gbk.set(ch, code);
    }
  }

  let index = 0;
  for (let hi = level.firstRow; hi <= level.lastRow; hi++) {
    for (let lo = 0xa1; lo <= 0xfe; lo++) {
      const ch = dec.decode(Uint8Array.from([hi, lo]));
      if ([...ch].length !== 1) continue;
      const cp = ch.codePointAt(0);
      if (cp < 0x80 || cp === 0xfffd) continue;
      const code = (hi << 8) | lo;
      slots.set(code, { code, cp, ch, index });
      if (!byChar.has(ch)) byChar.set(ch, code);
      index++;
    }
  }

  if (index !== rows * cols) {
    throw new PackError('BAD_TABLE', `expected ${rows * cols} slots, got ${index}`);
  }
  return { slots, byChar, gbk, rows, cols, count: index };
}

function buildGlyphPages(font, table, cfg) {
  const pages = Math.ceil((table.rows * table.cols) / cfg.pageGlyphs);
  const pageBuffers = [];
  for (let i = 0; i < pages; i++) pageBuffers.push(Buffer.alloc(cfg.pageGlyphs * cfg.bytesPerGlyph));

  let included = 0;
  let missing = 0;
  const missingSamples = [];

  for (const entry of table.slots.values()) {
    const gid = font.gidFor(entry.cp);
    if (gid === 0) {
      missing++;
      if (missingSamples.length < 12) missingSamples.push(entry.ch);
      continue;
    }
    const { rows } = font.rasterize(entry.cp, cfg.cell, cfg.cell);
    const page = Math.floor(entry.index / cfg.pageGlyphs);
    const off = (entry.index % cfg.pageGlyphs) * cfg.bytesPerGlyph;
    for (let i = 0; i < cfg.bytesPerGlyph; i++) pageBuffers[page][off + i] = rows[i];
    included++;
  }

  return { pages, pageBuffers, included, missing, missingSamples };
}

function buildFontHeader(level, table, cfg, pages, included, extraRecord) {
  const h = Buffer.alloc(FONT_HEADER_SIZE);
  h.write('LPG1', 0, 4, 'ascii');
  h.writeUInt16BE(2, 4);                    // version
  h.writeUInt16BE(cfg.cell, 6);
  h.writeUInt16BE(cfg.cell, 8);
  h.writeUInt16BE(cfg.bytesPerGlyph, 10);
  h.writeUInt16BE(included, 12);            // informational
  h.writeUInt16BE(1, 14);                   // layout = paged
  h.writeUInt16BE(cfg.pageGlyphs, 16);
  h.writeUInt16BE(0x0001, 18);              // bit0 MSB-first
  h.writeUInt16BE(0, 20);                   // firstCode
  h.writeUInt16BE(0, 22);                   // compression none
  h.writeUInt16BE(0, 24);                   // checksum disabled
  h.writeUInt16BE(0, 26);                   // packing row aligned
  h.writeUInt32BE(extraRecord, 28);         // extra (non-GB2312) glyph record, 0 if none
  h.writeUInt16BE(level.firstRow, 32);
  h.writeUInt16BE(level.lastRow, 34);
  h.writeUInt16BE(0xa1, 36);
  h.writeUInt16BE(0xfe, 38);
  h.writeUInt16BE(table.rows, 40);
  h.writeUInt16BE(table.cols, 42);
  h.writeUInt16BE(pages, 44);
  h.writeUInt16BE(cfg.pageGlyphs, 46);
  return h;
}

function parseFontHeader(record) {
  if (record.length < FONT_HEADER_SIZE) {
    throw new PackError('BAD_HDR_SIZE', `header record is ${record.length} bytes, need ${FONT_HEADER_SIZE}`);
  }
  if (record.toString('ascii', 0, 4) !== 'LPG1') {
    throw new PackError('BAD_MAGIC', `magic "${record.toString('ascii', 0, 4)}" != "LPG1"`);
  }
  return {
    version: record.readUInt16BE(4),
    cellW: record.readUInt16BE(6),
    cellH: record.readUInt16BE(8),
    bytesPerGlyph: record.readUInt16BE(10),
    glyphCount: record.readUInt16BE(12),
    layout: record.readUInt16BE(14),
    glyphsPerRecord: record.readUInt16BE(16),
    flags: record.readUInt16BE(18),
    compression: record.readUInt16BE(22),
    packing: record.readUInt16BE(26),
    textRecord: record.readUInt32BE(28),
    firstRow: record.readUInt16BE(32),
    lastRow: record.readUInt16BE(34),
    firstCol: record.readUInt16BE(36),
    lastCol: record.readUInt16BE(38),
    rows: record.readUInt16BE(40),
    cols: record.readUInt16BE(42),
    pages: record.readUInt16BE(44),
    pageGlyphs: record.readUInt16BE(46),
  };
}

function slotOffset(code, h) {
  const hi = (code >> 8) & 0xff;
  const lo = code & 0xff;
  if (hi < h.firstRow || hi > h.lastRow || lo < h.firstCol || lo > h.lastCol) return null;
  const index = (hi - h.firstRow) * h.cols + (lo - h.firstCol);
  const page = Math.floor(index / h.pageGlyphs);
  if (page >= h.pages) return null;
  return { page, off: (index % h.pageGlyphs) * h.bytesPerGlyph };
}

function glyphFromPdb(parsed, h, code) {
  const slot = slotOffset(code, h);
  if (!slot) return null;
  const rec = parsed.records[1 + slot.page];
  if (!rec || rec.length !== h.pageGlyphs * h.bytesPerGlyph) return null;
  return rec.subarray(slot.off, slot.off + h.bytesPerGlyph);
}

function verifyPdb(buf, opts, level, cfg, table, expected) {
  const parsed = parsePdb(buf);
  if (parsed.name !== opts.name) throw new PackError('BAD_NAME', `database name "${parsed.name}" != "${opts.name}"`);
  if (parsed.type !== 'GLYF') throw new PackError('BAD_TYPE', `type "${parsed.type}" != "GLYF"`);
  if (parsed.creator !== 'LPRD') throw new PackError('BAD_CREATOR', `creator "${parsed.creator}" != "LPRD"`);

  const h = parseFontHeader(parsed.records[0]);
  const expect = {
    version: 2,
    cellW: cfg.cell,
    cellH: cfg.cell,
    bytesPerGlyph: cfg.bytesPerGlyph,
    layout: 1,
    glyphsPerRecord: cfg.pageGlyphs,
    firstRow: level.firstRow,
    lastRow: level.lastRow,
    firstCol: 0xa1,
    lastCol: 0xfe,
    rows: table.rows,
    cols: table.cols,
    pages: expected.pages,
    pageGlyphs: cfg.pageGlyphs,
    textRecord: expected.extraRecord,
  };
  for (const [k, v] of Object.entries(expect)) {
    if (h[k] !== v) throw new PackError('BAD_HDR', `header ${k}=${h[k]}, expected ${v}`);
  }
  if (h.glyphCount !== expected.included) {
    throw new PackError('BAD_HDR', `glyphCount ${h.glyphCount} != ${expected.included}`);
  }
  const expectedRecords = 1 + h.pages + (h.textRecord ? 1 : 0);
  if (parsed.numRecords !== expectedRecords) {
    throw new PackError('BAD_COUNT', `record count ${parsed.numRecords} != ${expectedRecords}`);
  }
  for (let i = 1; i <= h.pages; i++) {
    if (parsed.records[i].length !== cfg.pageGlyphs * cfg.bytesPerGlyph) {
      throw new PackError('BAD_PAGE', `page ${i - 1} is ${parsed.records[i].length} bytes`);
    }
  }

  // Extras must match and must not shadow the dense grid.
  if (h.textRecord) {
    const extras = parsed.records[h.textRecord];
    if (!extras || extras.length < 2) throw new PackError('BAD_EXTRA', 'extras record missing');
    const count = extras.readUInt16BE(0);
    const itemSize = 2 + cfg.bytesPerGlyph;
    if (extras.length !== 2 + count * itemSize) {
      throw new PackError('BAD_EXTRA', `extras record is ${extras.length} bytes for ${count} glyphs`);
    }
    if (count !== expected.extraCodes.length) {
      throw new PackError('BAD_EXTRA', `extras count ${count} != ${expected.extraCodes.length}`);
    }
    for (let i = 0; i < count; i++) {
      const base = 2 + i * itemSize;
      const code = extras.readUInt16BE(base);
      if (code !== expected.extraCodes[i]) {
        throw new PackError('BAD_EXTRA', `extra ${i} code ${code.toString(16)} != ${expected.extraCodes[i].toString(16)}`);
      }
      if (slotOffset(code, h)) {
        throw new PackError('BAD_EXTRA', `extra code ${code.toString(16)} is also addressable in the dense grid`);
      }
      const got = extras.subarray(base + 2, base + 2 + cfg.bytesPerGlyph);
      if (!got.equals(expected.extraBytes[i])) {
        throw new PackError('BYTE_MISMATCH', `extra ${i} bytes differ`);
      }
    }
  }

  // Sample check: known characters must match the header/font source.
  for (const sample of expected.samples) {
    const got = glyphFromPdb(parsed, h, sample.code);
    if (!got) throw new PackError('BAD_SAMPLE', `sample ${sample.ch} not addressable`);
    if (!got.equals(sample.bytes)) {
      throw new PackError('BYTE_MISMATCH', `sample ${sample.ch} bytes differ: got ${hex(got)} expected ${hex(sample.bytes)}`);
    }
  }

  // Every font-missing code must be a zero slot.
  for (const code of expected.zeroCodes) {
    const got = glyphFromPdb(parsed, h, code);
    if (!got) throw new PackError('BAD_SLOT', `missing glyph slot ${code.toString(16)} not addressable`);
    for (const b of got) {
      if (b !== 0) throw new PackError('BAD_ZERO', `slot ${code.toString(16)} should be blank`);
    }
  }

  return { parsed, h };
}

function runSelfTest(buf, opts, level, cfg, table, expected) {
  const cases = [];
  const expectFail = (name, codes, mutate) => {
    const list = Array.isArray(codes) ? codes : [codes];
    const copy = Buffer.from(buf);
    const replacement = mutate(copy);
    const target = Buffer.isBuffer(replacement) ? replacement : copy;
    try {
      verifyPdb(target, opts, level, cfg, table, expected);
      cases.push({ name, ok: false, detail: 'accepted' });
    } catch (e) {
      const known = e instanceof PackError || e instanceof PdbError;
      const ok = known && list.includes(e.code);
      cases.push({ name, ok, detail: ok ? 'rejected' : `rejected as ${e.code || e.name}: ${e.message}` });
    }
  };

  const firstRec = parsePdb(Buffer.from(buf)).entries[0].offset;
  const firstPage = parsePdb(Buffer.from(buf)).entries[1].offset;

  expectFail('bad magic', 'BAD_MAGIC', (b) => b.write('XXXX', firstRec, 4, 'ascii'));
  expectFail('truncated tail', ['TRUNCATED', 'BAD_PAGE', 'BAD_EXTRA'], (b) => b.subarray(0, b.length - 16));
  expectFail('bad page offset', 'BAD_OFFSET', (b) => b.writeUInt32BE(0, PDB_HEADER_SIZE + 8));
  expectFail('flipped glyph byte', ['BYTE_MISMATCH', 'BAD_ZERO'], (b) => {
    const parsed = parsePdb(Buffer.from(b));
    const h = parseFontHeader(parsed.records[0]);
    const sample = expected.samples[0];
    const slot = slotOffset(sample.code, h);
    b[parsed.entries[1 + slot.page].offset + slot.off] ^= 0x01;
  });

  const failed = cases.filter((c) => !c.ok);
  if (failed.length) {
    for (const c of failed) console.error(`SELFTEST FAIL ${c.name}: ${c.detail}`);
    throw new PackError('SELFTEST', `${failed.length} negative test(s) failed`);
  }
  return cases;
}

function hex(buf) {
  return Array.from(buf, (b) => b.toString(16).padStart(2, '0').toUpperCase()).join(' ');
}

function renderSamples(samples, cfg) {
  const guard = `LOREPILOT_GB_SAMPLES${cfg.sampleSuffix}_H`;
  const sfx = cfg.sampleSuffix;              // '' for 8x8, '12' for 12x12
  const rowBytes = cfg.bytesPerGlyph;
  const lines = [];
  lines.push('/* Generated by font-packer/src/pack-gb2312.mjs. Do not edit. */');
  lines.push(`#ifndef ${guard}`);
  lines.push(`#define ${guard}`);
  lines.push('');
  lines.push(`#define kGbSampleCell${sfx}            ${cfg.cell}`);
  lines.push(`#define kGbSampleBytesPerGlyph${sfx}   ${rowBytes}`);
  lines.push(`#define kGbSampleCount${sfx} ${samples.length}`);
  lines.push('');
  lines.push(`static const unsigned short kGbSampleCodes${sfx}[kGbSampleCount${sfx}] = {`);
  lines.push(`    ${samples.map((s) => `0x${s.code.toString(16).toUpperCase().padStart(4, '0')}`).join(', ')}`);
  lines.push('};');
  lines.push('');
  lines.push(`static const unsigned char kGbSampleBytes${sfx}[kGbSampleCount${sfx}][${rowBytes}] = {`);
  for (const s of samples) {
    const chunk = Array.from(s.bytes, (b) => `0x${b.toString(16).padStart(2, '0').toUpperCase()}`);
    const parts = [];
    for (let i = 0; i < chunk.length; i += 12) parts.push(chunk.slice(i, i + 12).join(', '));
    lines.push(`    { ${parts.join(',\n      ')} }, /* ${s.ch} */`);
  }
  lines.push('};');
  lines.push('');
  lines.push(`#endif /* ${guard} */`);
  return lines.join('\n') + '\n';
}

function renderText(bytes, charCount) {
  const lines = [];
  lines.push('/* Generated by font-packer/src/pack-gb2312.mjs. Do not edit. */');
  lines.push('#ifndef LOREPILOT_GB_TEXT_H');
  lines.push('#define LOREPILOT_GB_TEXT_H');
  lines.push('');
  lines.push(`#define kGbTextCount ${charCount}`);
  lines.push(`#define kGbTextBytesCount ${bytes.length}`);
  lines.push(`#define kGbTextCharsPerPage 40`);
  lines.push(`#define kGbTextPageCount ${Math.ceil(charCount / 40)}`);
  lines.push('');
  lines.push('/* GB2312 double-byte text; ASCII characters are not present. */');
  lines.push('static const unsigned char kGbTextBytes[kGbTextBytesCount] = {');
  for (let i = 0; i < bytes.length; i += 16) {
    const chunk = bytes.subarray(i, i + 16);
    lines.push(`    ${Array.from(chunk, (b) => `0x${b.toString(16).padStart(2, '0').toUpperCase()}`).join(', ')},`);
  }
  lines.push('};');
  lines.push('');
  lines.push('#endif /* LOREPILOT_GB_TEXT_H */');
  return lines.join('\n') + '\n';
}

// Read the source text and encode it as GBK double bytes.
// Whitespace and line breaks are ignored so the file can be laid out for
// editing. Characters outside the GB2312 dense grid (trail byte 0x40-0xA0)
// are still GBK characters used by the text; they go into an "extras"
// record so the reader can draw them too.
function encodeDemoText(file, table, font) {
  const raw = fs.readFileSync(file, 'utf8').replace(/^\uFEFF/, '');
  const chars = [...raw].filter((ch) => !/[\s]/.test(ch));
  const bytes = [];
  const codes = [];
  const missingFont = [];

  for (const ch of chars) {
    const code = table.gbk.get(ch);
    if (!code) {
      throw new PackError('BAD_TEXT', `character ${ch} is not encodable as GBK`);
    }
    if (font.gidFor(ch.codePointAt(0)) === 0) {
      missingFont.push(ch);
      continue;
    }
    bytes.push((code >> 8) & 0xff, code & 0xff);
    codes.push(code);
  }

  if (missingFont.length) {
    throw new PackError('BAD_TEXT', `characters missing from font: ${[...new Set(missingFont)].join('')}`);
  }
  if (chars.length === 0) throw new PackError('BAD_TEXT', `text file is empty: ${file}`);
  if (chars.length % 2 !== 0) {
    throw new PackError('BAD_TEXT', `text has ${chars.length} characters; expected an even count`);
  }

  return { chars, bytes: Buffer.from(bytes), codes };
}

// Characters whose GBK code falls outside the A1-FE x A1-FE dense grid.
function isDenseCode(code) {
  const hi = (code >> 8) & 0xff;
  const lo = code & 0xff;
  return hi >= 0xa1 && hi <= 0xfe && lo >= 0xa1 && lo <= 0xfe;
}

function main() {
  const opts = parseArgs(process.argv.slice(2));
  const level = LEVELS[opts.level];
  const cfg = CELL_CONFIG[opts.cell];

  if (!fs.existsSync(opts.font)) throw new PackError('NO_FONT', missingFontMessage(opts.font));
  const font = loadFont(opts.font);
  if (!Number.isInteger(font.unitsPerEm / cfg.cell)) {
    throw new PackError('BAD_FONT',
      `unitsPerEm ${font.unitsPerEm} is not divisible by cell size ${cfg.cell}; ` +
      'is this the matching pixel font?');
  }
  if (font.unitsPerEm / cfg.cell !== 100) {
    throw new PackError('BAD_FONT',
      `font grid is ${font.unitsPerEm / cfg.cell} units per pixel, expected 100`);
  }

  const table = decodeGbTable(level);
  const built = buildGlyphPages(font, table, cfg);
  const extraRecordIndex = built.pages + 1;   // 0 = header, 1..pages = pages
  const header = buildFontHeader(level, table, cfg, built.pages, built.included,
                                 extraRecordIndex);

  const sampleChars = [...SAMPLE_TEXT];
  const samples = sampleChars.map((ch) => {
    const code = table.byChar.get(ch);
    if (!code) throw new PackError('BAD_SAMPLE', `sample character ${ch} is not in GB2312`);
    const entry = table.slots.get(code);
    const bytes = Buffer.from(font.rasterize(entry.cp, cfg.cell, cfg.cell).rows);
    return { ch, code, cp: entry.cp, bytes };
  });

  const demo = encodeDemoText(opts['text-file'], table, font);

  // Glyphs used by the text that this database cannot address go into a
  // separate record: header u16 count, then count x (u16 code, bytesPerGlyph).
  // This covers both GBK-only characters and GB2312 characters that fall
  // outside the selected level (e.g. level-2 hanzi in the basic build).
  const addressable = (code) => {
    const hi = (code >> 8) & 0xff;
    const lo = code & 0xff;
    return isDenseCode(code) && hi >= level.firstRow && hi <= level.lastRow;
  };
  const extraChars = [...new Set([...demo.chars].filter((ch) => !addressable(table.gbk.get(ch))))];
  if (extraChars.length > 128) {
    throw new PackError('TOO_MANY_EXTRAS',
      `${extraChars.length} out-of-grid glyphs; the reader caches at most 128`);
  }
  const extraItemSize = 2 + cfg.bytesPerGlyph;
  let extraRecord = null;
  if (extraChars.length > 0) {
    extraRecord = Buffer.alloc(2 + extraChars.length * extraItemSize);
    extraRecord.writeUInt16BE(extraChars.length, 0);
    extraChars.forEach((ch, i) => {
      const code = table.gbk.get(ch);
      const cp = ch.codePointAt(0);
      const base = 2 + i * extraItemSize;
      extraRecord.writeUInt16BE(code, base);
      const rows = font.rasterize(cp, cfg.cell, cfg.cell).rows;
      for (let k = 0; k < cfg.bytesPerGlyph; k++) extraRecord[base + 2 + k] = rows[k];
    });
  }

  const zeroCodes = [];
  for (const entry of table.slots.values()) {
    if (font.gidFor(entry.cp) === 0 && zeroCodes.length < 32) zeroCodes.push(entry.code);
  }

  const expected = {
    pages: built.pages,
    included: built.included,
    samples,
    zeroCodes,
    extraRecord: extraRecord ? built.pages + 1 : 0,
    extraCodes: extraRecord ? extraChars.map((ch) => table.gbk.get(ch)) : [],
    extraBytes: extraRecord
      ? extraChars.map((ch) => Buffer.from(font.rasterize(ch.codePointAt(0), cfg.cell, cfg.cell).rows))
      : [],
  };

  const records = [header, ...built.pageBuffers];
  if (extraRecord) records.push(extraRecord);
  const builtPdb = buildPdb({
    name: opts.name,
    type: 'GLYF',
    creator: 'LPRD',
    records,
  });
  fs.mkdirSync(path.dirname(path.resolve(opts.pdb)), { recursive: true });
  fs.writeFileSync(opts.pdb, builtPdb.buffer);

  let verifyInfo = null;
  if (opts.verify) verifyInfo = verifyPdb(fs.readFileSync(opts.pdb), opts, level, cfg, table, expected);
  const selfTests = opts.selftest ? runSelfTest(fs.readFileSync(opts.pdb), opts, level, cfg, table, expected) : [];

  fs.mkdirSync(path.dirname(path.resolve(opts.samples)), { recursive: true });
  fs.writeFileSync(opts.samples, renderSamples(samples, cfg));
  fs.mkdirSync(path.dirname(path.resolve(opts.text)), { recursive: true });
  fs.writeFileSync(opts.text, renderText(demo.bytes, demo.chars.length));

  if (!opts.quiet) {
    console.log(`level        ${opts.level} (${level.label}), cell ${cfg.cell}x${cfg.cell}`);
    console.log(`font         ${opts.font}`);
    console.log(`database     ${opts.name}  type=GLYF creator=LPRD`);
    console.log(`range        rows ${level.firstRow.toString(16).toUpperCase()}-${level.lastRow.toString(16).toUpperCase()}, ${table.rows}x${table.cols} slots`);
    console.log(`glyphs       ${built.included} with glyphs, ${built.missing} blank slots`);
    console.log(`pages        ${built.pages} x ${cfg.pageGlyphs} glyphs (${cfg.pageGlyphs * cfg.bytesPerGlyph} bytes/page)`);
    console.log(`pdb          ${opts.pdb}  ${builtPdb.buffer.length} bytes`);
    console.log(`records      ${records.length} (1 header + ${built.pages} pages${extraRecord ? ' + 1 extras' : ''})`);
    console.log(`samples      ${opts.samples}`);
    console.log(`text         ${opts.text} (${demo.chars.length} chars, ${Math.ceil(demo.chars.length / 40)} pages @ 40)`);
    if (extraChars.length) {
      console.log(`extras       ${extraChars.length} glyph(s): ${extraChars.join('')}`);
    }
    if (opts.verify) console.log('verify       OK');
    if (selfTests.length) console.log(`selftest     ${selfTests.length}/${selfTests.length} negative cases rejected`);
    if (built.missingSamples.length) {
      console.log(`blank sample ${built.missingSamples.join(' ')}`);
    }
    console.log('');
    console.log('record offsets:');
    for (let i = 0; i < builtPdb.offsets.length; i++) {
      let label = `page ${i - 1}`;
      if (i === 0) label = 'header';
      else if (extraRecord && i === records.length - 1) label = `extras (${records[i].length} bytes)`;
      console.log(`  [${String(i).padStart(2, '0')}] offset=${String(builtPdb.offsets[i]).padStart(6)} len=${String(records[i].length).padStart(5)}  ${label}`);
    }
  }
}

try {
  main();
} catch (e) {
  if (e instanceof PackError || e instanceof PdbError) {
    console.error(`ERROR [${e.code}] ${e.message}`);
  } else {
    console.error('ERROR', e && e.stack ? e.stack : e);
  }
  process.exit(1);
}

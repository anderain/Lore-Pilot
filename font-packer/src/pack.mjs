#!/usr/bin/env node
// lore-pilot font packer: ASCII 8x8 art + pixel-font text -> Palm OS record DB
//
// Usage (from anywhere):
//   node font-packer/src/pack.mjs
//   node font-packer/src/pack.mjs --verify --selftest
//   node font-packer/src/pack.mjs --font fusion-pixel-8px-monospaced-zh_hans.ttf
//
// No third-party dependencies. Node 18+.

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { loadFont } from './ttf.mjs';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const PACKER_ROOT = path.resolve(HERE, '..');
const WORKSPACE_ROOT = path.resolve(PACKER_ROOT, '..');

const DEFAULTS = {
  in: path.join(HERE, 'atlas-8x8.txt'),
  pdb: path.join(PACKER_ROOT, 'out', 'LPT08A.pdb'),
  header: path.join(WORKSPACE_ROOT, 'reader', 'Src', 'GlyphAtlas.h'),
  preview: path.join(PACKER_ROOT, 'out', 'preview.txt'),
  svg: path.join(PACKER_ROOT, 'out', 'preview.svg'),
  font: path.join(WORKSPACE_ROOT, 'fusion-pixel-8px-monospaced-zh_hans.ttf'),
  name: 'LPT08A',
  type: 'GLYF',
  creator: 'LPRD',
  verify: true,
  selftest: true,
  quiet: false,
};

const CELL_W = 8;
const CELL_H = 8;
const BYTES_PER_GLYPH = 8;
const PDB_HEADER_SIZE = 78;
const RECORD_ENTRY_SIZE = 8;
const FONT_HEADER_SIZE = 32;
const PALM_EPOCH_MS = Date.UTC(1904, 0, 1, 0, 0, 0);

class PackError extends Error {
  constructor(code, message) {
    super(message);
    this.name = 'PackError';
    this.code = code;
  }
}

function missingFontMessage() {
  return [
    'font file not found: fusion-pixel-8px-monospaced-zh_hans.ttf',
    'The font is not distributed with this repository.',
    'Download "Fusion Pixel 8px Mono zh-Hans" (SIL OFL 1.1) from',
    '  https://github.com/TakWolf/fusion-pixel-font/releases',
    'and place the TTF at the repository root, or pass --font <path>.',
  ].join('\n  ');
}

// ---------------------------------------------------------------- args

function parseArgs(argv) {
  const opts = { ...DEFAULTS };
  const takesValue = new Set(['in', 'pdb', 'header', 'preview', 'svg', 'font', 'name', 'type', 'creator']);

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

    if (takesValue.has(key)) {
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

  if (opts.name.length < 1 || opts.name.length > 31) {
    throw new PackError('BAD_ARG', `database name must be 1..31 chars, got ${opts.name.length}`);
  }
  if (!/^[\x20-\x7E]+$/.test(opts.name)) {
    throw new PackError('BAD_ARG', 'database name must be printable ASCII');
  }
  if (!/^[ -~]{4}$/.test(opts.type)) throw new PackError('BAD_ARG', 'type must be exactly 4 ASCII chars');
  if (!/^[ -~]{4}$/.test(opts.creator)) throw new PackError('BAD_ARG', 'creator must be exactly 4 ASCII chars');

  return opts;
}

function printUsage() {
  console.log(`lore-pilot font packer

  node font-packer/src/pack.mjs [options]

Options:
  --in <file>       ASCII atlas (default: font-packer/src/atlas-8x8.txt)
  --pdb <file>      output database (default: font-packer/out/LPT08A.pdb)
  --header <file>   generated C header (default: reader/Src/GlyphAtlas.h)
  --preview <file>  ASCII preview (default: font-packer/out/preview.txt)
  --svg <file>      SVG preview (default: font-packer/out/preview.svg)
  --font <file>     8px pixel TTF for @text blocks
  --name <name>     Palm database name (1..31 chars)
  --type <XXXX>     Palm database type (default: GLYF)
  --creator <XXXX>  Palm database creator (default: LPRD)
  --no-verify       skip re-parsing the written PDB
  --selftest        run negative tests on in-memory copies
  --quiet           only print errors
`);
}

// ---------------------------------------------------------------- atlas

function parseAtlas(text, sourceName) {
  const src = text.replace(/^\uFEFF/, '').replace(/\r\n?/g, '\n');
  const lines = src.split('\n');
  const glyphs = [];
  const texts = [];
  let current = null;

  const fail = (lineNo, code, message) => {
    throw new PackError(code, `${sourceName}:${lineNo}: ${message}`);
  };

  for (let i = 0; i < lines.length; i++) {
    const lineNo = i + 1;
    const line = lines[i].trim();

    if (line === '' || line.startsWith('//')) continue;

    if (line.startsWith('@text')) {
      if (current !== null) {
        if (current.rows.length !== CELL_H) {
          fail(lineNo, 'SHORT_GLYPH', `glyph "${current.name}" has ${current.rows.length} rows, expected ${CELL_H}`);
        }
        current = null;
      }
      const phrase = line.slice(5).trim();
      if (!phrase) fail(lineNo, 'BAD_TEXT', 'empty @text block');
      texts.push({ phrase, line: lineNo });
      continue;
    }

    if (line.startsWith('@')) {
      if (current !== null) {
        if (current.rows.length !== CELL_H) {
          fail(lineNo, 'SHORT_GLYPH', `glyph "${current.name}" has ${current.rows.length} rows, expected ${CELL_H}`);
        }
      }

      const parts = line.slice(1).trim().split(/\s+/);
      if (!parts[0]) fail(lineNo, 'BAD_HEADER', 'empty glyph name');

      const name = parts[0];
      let code = glyphs.length;
      if (parts.length > 1) {
        if (!/^(0x[0-9a-fA-F]+|[0-9]+)$/.test(parts[1])) {
          fail(lineNo, 'BAD_CODE', `invalid code "${parts[1]}"`);
        }
        code = parts[1].startsWith('0x') || parts[1].startsWith('0X')
          ? parseInt(parts[1].slice(2), 16)
          : parseInt(parts[1], 10);
      }
      if (code < 0 || code > 65535) fail(lineNo, 'BAD_CODE', `code out of range: ${code}`);

      current = { name, code, rows: [], line: lineNo };
      glyphs.push(current);
      continue;
    }

    if (current === null) fail(lineNo, 'ROW_OUTSIDE', 'bitmap row outside a glyph block (missing @name?)');
    if (line.length !== CELL_W) {
      fail(lineNo, 'BAD_WIDTH', `row must be ${CELL_W} chars, got ${line.length}: "${line}"`);
    }
    if (!/^[#.]{8}$/.test(line)) {
      fail(lineNo, 'BAD_CHAR', `row may only contain '#' and '.': "${line}"`);
    }
    current.rows.push(line);
    if (current.rows.length > CELL_H) {
      fail(lineNo, 'TALL_GLYPH', `glyph "${current.name}" has more than ${CELL_H} rows`);
    }
  }

  if (glyphs.length === 0) throw new PackError('EMPTY', 'no glyphs found');
  for (const g of glyphs) {
    if (g.rows.length !== CELL_H) {
      throw new PackError('SHORT_GLYPH', `${sourceName}:${g.line}: glyph "${g.name}" has ${g.rows.length} rows, expected ${CELL_H}`);
    }
  }

  const seen = new Set();
  for (const g of glyphs) {
    if (seen.has(g.code)) throw new PackError('DUP_CODE', `duplicate code ${g.code} ("${g.name}")`);
    seen.add(g.code);
  }
  for (let i = 0; i < glyphs.length; i++) {
    if (!seen.has(i)) {
      throw new PackError('NONCONTIG', `missing code ${i}; Phase A requires contiguous codes 0..${glyphs.length - 1}`);
    }
  }
  if (texts.length > 1) {
    throw new PackError('MULTI_TEXT', 'only one @text block is supported in this phase');
  }

  return { glyphs: glyphs.sort((a, b) => a.code - b.code), texts };
}

function encodeGlyph(glyph) {
  const bytes = Buffer.alloc(BYTES_PER_GLYPH);
  for (let r = 0; r < CELL_H; r++) {
    let b = 0;
    for (let c = 0; c < CELL_W; c++) {
      if (glyph.rows[r][c] === '#') b |= 0x80 >> c;
    }
    bytes[r] = b;
  }
  return bytes;
}

function rowStrings(bytes) {
  const rows = [];
  for (const b of bytes) {
    let s = '';
    for (let c = 0; c < CELL_W; c++) s += (b & (0x80 >> c)) ? '#' : '.';
    rows.push(s);
  }
  return rows;
}

function checksumOf(glyphBuffers) {
  let sum = 0;
  for (const g of glyphBuffers) {
    for (const b of g) sum = (sum + b) & 0xffff;
  }
  return sum;
}

// Rasterize @text blocks and append their unique characters after the art.
function buildGlyphSet(atlas, fontFile) {
  const glyphs = atlas.glyphs.map((g) => ({ ...g, source: 'art' }));
  const byCp = new Map();
  const phrases = [];

  if (atlas.texts.length > 0) {
    if (!fontFile) throw new PackError('NO_FONT', missingFontMessage());
    if (!fs.existsSync(fontFile)) throw new PackError('NO_FONT', missingFontMessage());
    const font = loadFont(fontFile);

    for (const t of atlas.texts) {
      const indices = [];
      for (const ch of t.phrase) {
        const cp = ch.codePointAt(0);
        if (cp > 0xffff) {
          throw new PackError('NON_BMP', `U+${cp.toString(16).toUpperCase()} is outside the BMP; not supported yet`);
        }
        let entry = byCp.get(cp);
        if (!entry) {
          const { gid, rows } = font.rasterize(cp, CELL_W, CELL_H);
          const bytes = Buffer.from(rows);
          entry = {
            name: `U+${cp.toString(16).toUpperCase().padStart(4, '0')}`,
            code: glyphs.length,
            rows: rowStrings(bytes),
            source: 'text',
            cp,
            gid,
          };
          glyphs.push(entry);
          byCp.set(cp, entry);
        }
        indices.push(entry.code);
      }
      phrases.push({ phrase: t.phrase, indices });
    }
  }

  if (glyphs.length > 4080) throw new PackError('TOO_MANY', 'too many glyphs for one header (max 4080)');
  return { glyphs, phrases };
}

// ---------------------------------------------------------------- PDB build

function palmSecondsNow() {
  return Math.max(0, Math.floor((Date.now() - PALM_EPOCH_MS) / 1000));
}

function buildFontHeader(glyphCount, checksum, textRecord) {
  const h = Buffer.alloc(FONT_HEADER_SIZE);
  h.write('LPG1', 0, 4, 'ascii');
  h.writeUInt16BE(1, 4);                 // version
  h.writeUInt16BE(CELL_W, 6);
  h.writeUInt16BE(CELL_H, 8);
  h.writeUInt16BE(BYTES_PER_GLYPH, 10);
  h.writeUInt16BE(glyphCount, 12);
  h.writeUInt16BE(0, 14);                // layout: one record per glyph
  h.writeUInt16BE(0, 16);                // glyphsPerRecord
  h.writeUInt16BE(0x0003, 18);           // bit0 MSB-first, bit1 checksum valid
  h.writeUInt16BE(0, 20);                // firstCode
  h.writeUInt16BE(0, 22);                // compression none
  h.writeUInt16BE(checksum, 24);         // data checksum
  h.writeUInt16BE(0, 26);                // packing: row aligned
  h.writeUInt32BE(textRecord, 28);       // record index holding the phrase (0 = none)
  return h;
}

function buildTextRecord(phrases) {
  if (phrases.length === 0) return null;
  const p = phrases[0];
  const count = p.indices.length;
  const rec = Buffer.alloc(4 + count * 4);
  rec.writeUInt16BE(1, 0);               // version
  rec.writeUInt16BE(count, 2);
  for (let i = 0; i < count; i++) {
    rec.writeUInt16BE(p.indices[i], 4 + i * 2);
  }
  for (let i = 0; i < count; i++) {
    rec.writeUInt16BE([...p.phrase][i].codePointAt(0), 4 + count * 2 + i * 2);
  }
  return rec;
}

function buildPdb(opts, glyphs, phrases) {
  const glyphBuffers = glyphs.map(encodeGlyph);
  const checksum = checksumOf(glyphBuffers);
  const textRecord = buildTextRecord(phrases);
  const headerRecord = buildFontHeader(glyphs.length, checksum, textRecord ? glyphs.length + 1 : 0);

  const records = [headerRecord, ...glyphBuffers];
  if (textRecord) records.push(textRecord);

  const numRecords = records.length;
  const listEnd = PDB_HEADER_SIZE + numRecords * RECORD_ENTRY_SIZE;

  const offsets = [];
  let cursor = listEnd;
  for (const rec of records) {
    offsets.push(cursor);
    cursor += rec.length;
  }
  const totalSize = cursor;

  const out = Buffer.alloc(totalSize);
  const now = palmSecondsNow();

  out.write(opts.name, 0, 'ascii');
  out.writeUInt16BE(0x0000, 32);          // attributes
  out.writeUInt16BE(1, 34);               // PDB version
  out.writeUInt32BE(now, 36);             // creation
  out.writeUInt32BE(now, 40);             // modification
  out.writeUInt32BE(0, 44);               // last backup
  out.writeUInt32BE(0, 48);               // modification number
  out.writeUInt32BE(0, 52);               // appInfoID
  out.writeUInt32BE(0, 56);               // sortInfoID
  out.write(opts.type, 60, 4, 'ascii');
  out.write(opts.creator, 64, 4, 'ascii');
  out.writeUInt32BE(numRecords + 1, 68);  // uniqueID seed
  out.writeUInt32BE(0, 72);               // nextRecordListID
  out.writeUInt16BE(numRecords, 76);

  for (let i = 0; i < numRecords; i++) {
    const base = PDB_HEADER_SIZE + i * RECORD_ENTRY_SIZE;
    out.writeUInt32BE(offsets[i], base);
    out[base + 4] = 0;                    // record attribute: normal
    out.writeUIntBE(i + 1, base + 5, 3);  // unique ID
  }

  for (let i = 0; i < numRecords; i++) {
    records[i].copy(out, offsets[i]);
  }

  return { pdb: out, glyphBuffers, checksum, records, offsets, totalSize, textRecordIndex: textRecord ? glyphs.length + 1 : 0 };
}

// ---------------------------------------------------------------- PDB parse/verify

function parsePdb(buf) {
  if (buf.length < PDB_HEADER_SIZE) throw new PackError('TRUNCATED', 'file is smaller than PDB header');
  const name = buf.toString('ascii', 0, 32).replace(/\0.*$/, '');
  const numRecords = buf.readUInt16BE(76);
  const listEnd = PDB_HEADER_SIZE + numRecords * RECORD_ENTRY_SIZE;
  if (numRecords === 0) throw new PackError('NO_RECORDS', 'PDB has no records');
  if (listEnd > buf.length) throw new PackError('BAD_LIST', 'record list runs past end of file');

  const entries = [];
  for (let i = 0; i < numRecords; i++) {
    const base = PDB_HEADER_SIZE + i * RECORD_ENTRY_SIZE;
    entries.push({
      offset: buf.readUInt32BE(base),
      attr: buf[base + 4],
      uid: buf.readUIntBE(base + 5, 3),
    });
  }

  for (let i = 0; i < entries.length; i++) {
    const e = entries[i];
    if (e.offset < listEnd) throw new PackError('BAD_OFFSET', `record ${i} offset ${e.offset} overlaps the record list (min ${listEnd})`);
    if (e.offset >= buf.length) {
      if (i === entries.length - 1) {
        throw new PackError('TRUNCATED', `last record starts at ${e.offset}, file ends at ${buf.length}`);
      }
      throw new PackError('BAD_OFFSET', `record ${i} offset ${e.offset} is past EOF (${buf.length})`);
    }
    if (i > 0 && e.offset <= entries[i - 1].offset) {
      throw new PackError('BAD_ORDER', `record ${i} offset ${e.offset} is not after record ${i - 1} offset ${entries[i - 1].offset}`);
    }
  }

  const records = entries.map((e, i) => {
    const next = i + 1 < entries.length ? entries[i + 1].offset : buf.length;
    return buf.subarray(e.offset, next);
  });

  return {
    name,
    attributes: buf.readUInt16BE(32),
    version: buf.readUInt16BE(34),
    type: buf.toString('ascii', 60, 64),
    creator: buf.toString('ascii', 64, 68),
    numRecords,
    entries,
    records,
    listEnd,
  };
}

function parseHeader(h) {
  if (h.length < FONT_HEADER_SIZE) throw new PackError('BAD_HDR_SIZE', `header record is ${h.length} bytes, need ${FONT_HEADER_SIZE}`);
  if (h.toString('ascii', 0, 4) !== 'LPG1') {
    throw new PackError('BAD_MAGIC', `magic "${h.toString('ascii', 0, 4)}" != "LPG1"`);
  }
  return {
    version: h.readUInt16BE(4),
    cellW: h.readUInt16BE(6),
    cellH: h.readUInt16BE(8),
    bytesPerGlyph: h.readUInt16BE(10),
    glyphCount: h.readUInt16BE(12),
    layout: h.readUInt16BE(14),
    glyphsPerRecord: h.readUInt16BE(16),
    flags: h.readUInt16BE(18),
    firstCode: h.readUInt16BE(20),
    compression: h.readUInt16BE(22),
    dataChecksum: h.readUInt16BE(24),
    packing: h.readUInt16BE(26),
    textRecord: h.readUInt32BE(28),
  };
}

function verifyPdb(buf, opts, glyphs, phrases) {
  const parsed = parsePdb(buf);

  if (parsed.name !== opts.name) throw new PackError('BAD_NAME', `database name "${parsed.name}" != "${opts.name}"`);
  if (parsed.type !== opts.type) throw new PackError('BAD_TYPE', `type "${parsed.type}" != "${opts.type}"`);
  if (parsed.creator !== opts.creator) throw new PackError('BAD_CREATOR', `creator "${parsed.creator}" != "${opts.creator}"`);

  const heading = parseHeader(parsed.records[0]);
  const hasText = phrases.length > 0;
  const expectedRecords = glyphs.length + 1 + (hasText ? 1 : 0);
  if (parsed.numRecords !== expectedRecords) {
    throw new PackError('BAD_COUNT', `record count ${parsed.numRecords} != ${expectedRecords}`);
  }

  for (const e of parsed.entries) {
    if (e.attr !== 0) throw new PackError('BAD_ATTR', `record attribute ${e.attr} != 0`);
  }

  const expect = {
    version: 1,
    cellW: CELL_W,
    cellH: CELL_H,
    bytesPerGlyph: BYTES_PER_GLYPH,
    glyphCount: glyphs.length,
    layout: 0,
    glyphsPerRecord: 0,
    flags: 0x0003,
    firstCode: 0,
    compression: 0,
    packing: 0,
    textRecord: hasText ? glyphs.length + 1 : 0,
  };
  for (const [k, v] of Object.entries(expect)) {
    if (heading[k] !== v) throw new PackError('BAD_HDR', `header ${k}=${heading[k]}, expected ${v}`);
  }

  const glyphBuffers = [];
  for (let i = 0; i < glyphs.length; i++) {
    const rec = parsed.records[i + 1];
    if (rec.length !== BYTES_PER_GLYPH) {
      throw new PackError('BAD_REC_SIZE', `glyph ${i} record is ${rec.length} bytes, expected ${BYTES_PER_GLYPH}`);
    }
    const expected = encodeGlyph(glyphs[i]);
    if (!rec.equals(expected)) {
      throw new PackError('BYTE_MISMATCH', `glyph ${i} (${glyphs[i].name}) bytes differ: got ${hex(rec)} expected ${hex(expected)}`);
    }
    glyphBuffers.push(Buffer.from(rec));
  }

  const sum = checksumOf(glyphBuffers);
  if (heading.dataChecksum !== sum) {
    throw new PackError('BAD_SUM', `checksum 0x${heading.dataChecksum.toString(16)} != computed 0x${sum.toString(16)}`);
  }

  if (hasText) {
    const rec = parsed.records[glyphs.length + 1];
    if (!rec || rec.length < 4) throw new PackError('BAD_TEXT', 'text record missing or too short');
    const version = rec.readUInt16BE(0);
    const count = rec.readUInt16BE(2);
    if (version !== 1) throw new PackError('BAD_TEXT', `text record version ${version} != 1`);
    if (count !== phrases[0].indices.length) {
      throw new PackError('BAD_TEXT', `text count ${count} != ${phrases[0].indices.length}`);
    }
    if (rec.length !== 4 + count * 4) {
      throw new PackError('BAD_TEXT', `text record size ${rec.length} != ${4 + count * 4}`);
    }
    for (let i = 0; i < count; i++) {
      const idx = rec.readUInt16BE(4 + i * 2);
      if (idx !== phrases[0].indices[i]) {
        throw new PackError('BAD_TEXT', `text index ${i}=${idx} != ${phrases[0].indices[i]}`);
      }
      const cp = rec.readUInt16BE(4 + count * 2 + i * 2);
      if (cp !== [...phrases[0].phrase][i].codePointAt(0)) {
        throw new PackError('BAD_TEXT', `text codepoint ${i}=${cp} is wrong`);
      }
    }
  }

  return { parsed, heading, sum };
}

// ---------------------------------------------------------------- self test

function runSelfTest(pdb, opts, glyphs, phrases) {
  const cases = [];

  const expectFail = (name, codes, mutate) => {
    const list = Array.isArray(codes) ? codes : [codes];
    const copy = Buffer.from(pdb);
    const replacement = mutate(copy);
    const target = Buffer.isBuffer(replacement) ? replacement : copy;
    try {
      verifyPdb(target, opts, glyphs, phrases);
      cases.push({ name, code: list.join('|'), ok: false, detail: 'verifier accepted a corrupted file' });
    } catch (e) {
      const ok = e instanceof PackError && list.includes(e.code);
      cases.push({ name, code: list.join('|'), ok, detail: ok ? 'rejected' : `rejected as ${e.code || e.name}: ${e.message}` });
    }
  };

  const firstRec = parsePdb(Buffer.from(pdb)).entries[0].offset;

  expectFail('bad magic', 'BAD_MAGIC', (b) => b.write('XXXX', firstRec, 4, 'ascii'));
  /* Chopping the tail can hit either the structural check or the text-record
     length check, depending on which record is last. Both mean "data loss". */
  expectFail('truncated file', ['TRUNCATED', 'BAD_TEXT'], (b) => b.subarray(0, b.length - 8));
  expectFail('record offset zero', 'BAD_OFFSET', (b) => b.writeUInt32BE(0, PDB_HEADER_SIZE + RECORD_ENTRY_SIZE));
  expectFail('flipped glyph byte', 'BYTE_MISMATCH', (b) => {
    const parsed = parsePdb(Buffer.from(b));
    const off = parsed.entries[1].offset;
    b[off] ^= 0x01;
  });

  const failed = cases.filter((c) => !c.ok);
  if (failed.length) {
    for (const c of failed) console.error(`SELFTEST FAIL ${c.name}: expected ${c.code}; ${c.detail}`);
    throw new PackError('SELFTEST', `${failed.length} negative test(s) failed`);
  }
  return cases;
}

// ---------------------------------------------------------------- outputs

function hex(buf) {
  return Array.from(buf, (b) => b.toString(16).padStart(2, '0').toUpperCase()).join(' ');
}

function renderPreview(glyphs, glyphBuffers, checksum, phrases) {
  const out = [];
  out.push(`lore-pilot 8x8 atlas: ${glyphs.length} glyphs, checksum 0x${checksum.toString(16).toUpperCase().padStart(4, '0')}`);
  out.push('');
  for (let i = 0; i < glyphs.length; i++) {
    out.push(`#${String(i).padStart(2, '0')} ${glyphs[i].name.padEnd(8)} ${hex(glyphBuffers[i])}`);
    for (const row of glyphs[i].rows) out.push(`   ${row.replace(/#/g, '##').replace(/\./g, '..')}`);
    out.push('');
  }
  if (phrases.length > 0) {
    const p = phrases[0];
    out.push(`phrase: ${p.phrase}`);
    out.push(`indices: ${p.indices.join(',')}`);
    out.push('');
    out.push('1x:');
    const lines = [];
    for (let start = 0; start < p.indices.length; start += 20) {
      const seg = p.indices.slice(start, start + 20);
      for (let r = 0; r < CELL_H; r++) {
        lines.push(seg.map((idx) => glyphs[idx].rows[r]).join(''));
      }
    }
    out.push(...lines);
    out.push('');
    out.push('2x:');
    for (let start = 0; start < p.indices.length; start += 8) {
      const seg = p.indices.slice(start, start + 8);
      for (let r = 0; r < CELL_H; r++) {
        let s = '';
        for (const idx of seg) {
          for (const px of glyphs[idx].rows[r]) s += px + px;
        }
        out.push(s);
      }
      out.push('');
    }
  }
  return out.join('\n');
}

function escapeXml(s) {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

function renderSvg(glyphs, glyphBuffers, checksum) {
  const scale = 14;
  const cell = CELL_W * scale;
  const cols = 4;
  const pad = 18;
  const labelH = 34;
  const cellW = cell + pad * 2;
  const cellH = cell + labelH + pad;
  const rows = Math.ceil(glyphs.length / cols);
  const width = cols * cellW + pad;
  const height = rows * cellH + pad + 26;

  const parts = [];
  parts.push(`<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">`);
  parts.push('<rect width="100%" height="100%" fill="#f4f1ea"/>');
  parts.push(`<text x="${pad}" y="20" font-family="monospace" font-size="13" fill="#222">lore-pilot 8x8 atlas - ${glyphs.length} glyphs - checksum 0x${checksum.toString(16).toUpperCase().padStart(4, '0')}</text>`);

  for (let i = 0; i < glyphs.length; i++) {
    const cx = pad + (i % cols) * cellW;
    const cy = 34 + Math.floor(i / cols) * cellH;
    parts.push(`<rect x="${cx - 6}" y="${cy - 6}" width="${cell + 12}" height="${cell + 12}" fill="#fff" stroke="#c9c2b4"/>`);
    for (let r = 0; r < CELL_H; r++) {
      for (let c = 0; c < CELL_W; c++) {
        if (glyphs[i].rows[r][c] !== '#') continue;
        parts.push(`<rect x="${cx + c * scale}" y="${cy + r * scale}" width="${scale}" height="${scale}" fill="#1b1b1b"/>`);
      }
    }
    parts.push(`<text x="${cx - 6}" y="${cy + cell + 18}" font-family="monospace" font-size="12" fill="#222">#${i} ${escapeXml(glyphs[i].name)}</text>`);
    parts.push(`<text x="${cx - 6}" y="${cy + cell + 30}" font-family="monospace" font-size="9" fill="#666">${hex(glyphBuffers[i])}</text>`);
  }
  parts.push('</svg>');
  return parts.join('\n') + '\n';
}

function renderHeader(glyphs, glyphBuffers, checksum, phrases) {
  const lines = [];
  lines.push('/* Generated by font-packer/src/pack.mjs. Do not edit. */');
  lines.push('#ifndef LOREPILOT_GLYPH_ATLAS_H');
  lines.push('#define LOREPILOT_GLYPH_ATLAS_H');
  lines.push('');
  lines.push(`#define kGlyphAtlasCellW         ${CELL_W}`);
  lines.push(`#define kGlyphAtlasCellH         ${CELL_H}`);
  lines.push(`#define kGlyphAtlasBytesPerGlyph ${BYTES_PER_GLYPH}`);
  lines.push(`#define kGlyphAtlasCount         ${glyphs.length}`);
  lines.push(`#define kGlyphAtlasChecksum      0x${checksum.toString(16).toUpperCase().padStart(4, '0')}`);
  lines.push(`#define kGlyphAtlasPhraseCount   ${phrases.length ? phrases[0].indices.length : 0}`);
  lines.push(`#define kGlyphAtlasTextRecord    ${phrases.length ? glyphs.length + 1 : 0}`);
  lines.push('');
  lines.push('static const unsigned char kGlyphAtlasBytes[kGlyphAtlasCount * kGlyphAtlasBytesPerGlyph] = {');
  for (let i = 0; i < glyphBuffers.length; i++) {
    lines.push(`    ${Array.from(glyphBuffers[i], (b) => `0x${b.toString(16).padStart(2, '0').toUpperCase()}`).join(', ')}, /* ${i} ${glyphs[i].name} */`);
  }
  lines.push('};');
  lines.push('');
  if (phrases.length > 0) {
    const p = phrases[0];
    lines.push('/* Fallback phrase when the PDB has no text record. */');
    lines.push(`static const unsigned short kGlyphAtlasPhrase[kGlyphAtlasPhraseCount] = {`);
    lines.push(`    ${p.indices.join(', ')}`);
    lines.push('};');
    lines.push('');
  }
  lines.push('#endif /* LOREPILOT_GLYPH_ATLAS_H */');
  return lines.join('\n') + '\n';
}

// ---------------------------------------------------------------- main

function ensureDir(file) {
  fs.mkdirSync(path.dirname(path.resolve(file)), { recursive: true });
}

function main() {
  const opts = parseArgs(process.argv.slice(2));
  const text = fs.readFileSync(opts.in, 'utf8');
  const atlas = parseAtlas(text, opts.in);
  const { glyphs, phrases } = buildGlyphSet(atlas, opts.font);
  const built = buildPdb(opts, glyphs, phrases);

  ensureDir(opts.pdb);
  fs.writeFileSync(opts.pdb, built.pdb);

  if (opts.verify) verifyPdb(fs.readFileSync(opts.pdb), opts, glyphs, phrases);
  const selfTestCases = opts.selftest ? runSelfTest(fs.readFileSync(opts.pdb), opts, glyphs, phrases) : [];

  ensureDir(opts.header);
  fs.writeFileSync(opts.header, renderHeader(glyphs, built.glyphBuffers, built.checksum, phrases));

  ensureDir(opts.preview);
  fs.writeFileSync(opts.preview, renderPreview(glyphs, built.glyphBuffers, built.checksum, phrases));

  ensureDir(opts.svg);
  fs.writeFileSync(opts.svg, renderSvg(glyphs, built.glyphBuffers, built.checksum));

  if (!opts.quiet) {
    console.log(`atlas        ${opts.in}`);
    console.log(`font         ${atlas.texts.length ? opts.font : '(not used)'}`);
    console.log(`glyphs       ${glyphs.length}`);
    console.log(`cell         ${CELL_W}x${CELL_H}, ${BYTES_PER_GLYPH} bytes/glyph`);
    console.log(`checksum     0x${built.checksum.toString(16).toUpperCase().padStart(4, '0')}`);
    console.log(`database     ${opts.name}  type=${opts.type} creator=${opts.creator}`);
    console.log(`records      ${built.records.length} (1 header + ${glyphs.length} glyphs${built.textRecordIndex ? ' + 1 phrase' : ''})`);
    console.log(`pdb          ${opts.pdb}  ${built.totalSize} bytes`);
    console.log(`header       ${opts.header}`);
    console.log(`preview      ${opts.preview}`);
    console.log(`svg          ${opts.svg}`);
    if (opts.verify) console.log('verify       OK');
    if (selfTestCases.length) console.log(`selftest     ${selfTestCases.length}/${selfTestCases.length} negative cases rejected`);
    if (phrases.length) {
      console.log(`phrase       ${phrases[0].phrase}`);
      console.log(`indices      ${phrases[0].indices.join(',')}`);
    }
    console.log('');
    console.log('record offsets:');
    for (let i = 0; i < built.offsets.length; i++) {
      let label;
      if (i === 0) label = 'header';
      else if (built.textRecordIndex && i === built.textRecordIndex) label = `phrase (${built.records[i].length} bytes)`;
      else label = `glyph ${i - 1} ${glyphs[i - 1].name}`;
      console.log(`  [${String(i).padStart(2, '0')}] offset=${String(built.offsets[i]).padStart(4)} len=${String(built.records[i].length).padStart(2)}  ${label}`);
    }
  }
}

try {
  main();
} catch (e) {
  if (e instanceof PackError) {
    console.error(`ERROR [${e.code}] ${e.message}`);
  } else {
    console.error('ERROR', e && e.stack ? e.stack : e);
  }
  process.exit(1);
}

// Minimal TrueType parser + 8x8 rasterizer for pixel fonts.
//
// Scope: simple (non-composite) glyphs, cmap format 12 (preferred) or 4,
// 1bpp output on an 8x8 pixel grid. Enough for Fusion Pixel 8px and similar
// pixel fonts; it is not a general font engine.

import fs from 'node:fs';

const TAG = (b, o) => b.toString('ascii', o, o + 4);
const U8 = (b, o) => b[o];
const I16 = (b, o) => b.readInt16BE(o);
const U16 = (b, o) => b.readUInt16BE(o);
const U32 = (b, o) => b.readUInt32BE(o);

function readTables(b) {
  if (b.length < 12) throw new Error('TTF: file too short');
  const numTables = U16(b, 4);
  const tables = {};
  for (let i = 0; i < numTables; i++) {
    const o = 12 + i * 16;
    const tag = TAG(b, o);
    tables[tag] = { offset: U32(b, o + 8), length: U32(b, o + 12) };
  }
  return tables;
}

function cmapLookup(b, cmapOffset) {
  const n = U16(b, cmapOffset + 2);
  let format12 = -1;
  let format4 = -1;
  for (let i = 0; i < n; i++) {
    const rec = cmapOffset + 4 + i * 8;
    const platform = U16(b, rec);
    const encoding = U16(b, rec + 2);
    const sub = cmapOffset + U32(b, rec + 4);
    const format = U16(b, sub);
    const unicode = platform === 0 || (platform === 3 && (encoding === 1 || encoding === 10));
    if (!unicode) continue;
    if (format === 12) format12 = sub;
    else if (format === 4) format4 = sub;
  }

  if (format12 >= 0) {
    const groups = U32(b, format12 + 12);
    return (cp) => {
      let lo = 0;
      let hi = groups - 1;
      while (lo <= hi) {
        const mid = (lo + hi) >> 1;
        const o = format12 + 16 + mid * 12;
        const start = U32(b, o);
        const end = U32(b, o + 4);
        const startGid = U32(b, o + 8);
        if (cp < start) hi = mid - 1;
        else if (cp > end) lo = mid + 1;
        else return startGid + (cp - start);
      }
      return 0;
    };
  }

  if (format4 >= 0) {
    const segX2 = U16(b, format4 + 6);
    const segs = segX2 >> 1;
    const endOfs = format4 + 14;
    const startOfs = endOfs + segX2 + 2;
    const deltaOfs = startOfs + segX2;
    const rangeOfs = deltaOfs + segX2;
    return (cp) => {
      if (cp > 0xffff) return 0;
      for (let i = 0; i < segs; i++) {
        const end = U16(b, endOfs + i * 2);
        if (cp > end) continue;
        const start = U16(b, startOfs + i * 2);
        if (cp < start) return 0;
        const delta = I16(b, deltaOfs + i * 2);
        const ro = U16(b, rangeOfs + i * 2);
        if (ro === 0) return (cp + delta) & 0xffff;
        const glyphOfs = rangeOfs + i * 2 + ro + (cp - start) * 2;
        const gid = U16(b, glyphOfs);
        return gid === 0 ? 0 : (gid + delta) & 0xffff;
      }
      return 0;
    };
  }

  throw new Error('TTF: no Unicode cmap (format 4 or 12)');
}

function parseGlyph(b, glyfOffset, offsets, gid) {
  const start = glyfOffset + offsets[gid];
  const end = glyfOffset + offsets[gid + 1];
  if (end <= start) return [];
  const nc = I16(b, start);
  if (nc < 0) {
    throw new Error(`TTF: composite glyph ${gid} is not supported`);
  }
  if (nc === 0) return [];

  const endPts = [];
  for (let i = 0; i < nc; i++) endPts.push(U16(b, start + 10 + i * 2));
  const instrLen = U16(b, start + 10 + nc * 2);
  let p = start + 12 + nc * 2 + instrLen;
  const pointCount = endPts[nc - 1] + 1;

  const flags = [];
  while (flags.length < pointCount) {
    const f = U8(b, p++);
    flags.push(f);
    if (f & 8) {
      const repeat = U8(b, p++);
      for (let i = 0; i < repeat; i++) flags.push(f);
    }
  }

  const xs = [];
  let x = 0;
  for (const f of flags) {
    if (f & 2) {
      const d = U8(b, p++);
      x += (f & 16) ? d : -d;
    } else if (!(f & 16)) {
      x += I16(b, p);
      p += 2;
    }
    xs.push(x);
  }

  const ys = [];
  let y = 0;
  for (const f of flags) {
    if (f & 4) {
      const d = U8(b, p++);
      y += (f & 32) ? d : -d;
    } else if (!(f & 32)) {
      y += I16(b, p);
      p += 2;
    }
    ys.push(y);
  }

  const contours = [];
  let first = 0;
  for (let c = 0; c < nc; c++) {
    const pts = [];
    for (let i = first; i <= endPts[c]; i++) pts.push([xs[i], ys[i]]);
    first = endPts[c] + 1;
    contours.push(pts);
  }
  return contours;
}

function dropDuplicatePoints(contour) {
  const out = [];
  for (const p of contour) {
    const q = out[out.length - 1];
    if (!q || q[0] !== p[0] || q[1] !== p[1]) out.push(p);
  }
  if (out.length > 1) {
    const a = out[0];
    const z = out[out.length - 1];
    if (a[0] === z[0] && a[1] === z[1]) out.pop();
  }
  return out;
}

function insideEvenOdd(contours, x, y) {
  let inside = false;
  for (const c of contours) {
    for (let i = 0; i < c.length; i++) {
      const a = c[i];
      const b = c[(i + 1) % c.length];
      if ((a[1] > y) !== (b[1] > y)) {
        const t = (y - a[1]) / (b[1] - a[1]);
        if (a[0] + t * (b[0] - a[0]) > x) inside = !inside;
      }
    }
  }
  return inside;
}

export function loadFont(file) {
  const b = fs.readFileSync(file);
  const tables = readTables(b);
  for (const need of ['head', 'maxp', 'hhea', 'hmtx', 'loca', 'glyf', 'cmap']) {
    if (!tables[need]) throw new Error(`TTF: missing table ${need}`);
  }

  const unitsPerEm = U16(b, tables.head.offset + 18);
  const longLoca = U16(b, tables.head.offset + 50) !== 0;
  const ascent = I16(b, tables.hhea.offset + 4);
  const descent = I16(b, tables.hhea.offset + 6);
  const numGlyphs = U16(b, tables.maxp.offset + 4);
  const numHMetrics = U16(b, tables.hhea.offset + 34);
  const glyfOffset = tables.glyf.offset;

  const loca = (gid) => {
    if (longLoca) return U32(b, tables.loca.offset + gid * 4);
    return U16(b, tables.loca.offset + gid * 2) * 2;
  };

  const offsets = new Array(numGlyphs + 1);
  for (let i = 0; i <= numGlyphs; i++) offsets[i] = loca(i);

  const gidFor = cmapLookup(b, tables.cmap.offset);

  function advance(gid) {
    const i = gid < numHMetrics ? gid : numHMetrics - 1;
    return U16(b, tables.hmtx.offset + i * 4);
  }

  function rasterize(cp, cellW = 8, cellH = 8) {
    const gid = gidFor(cp);
    if (gid === 0) {
      throw new Error(`TTF: character U+${cp.toString(16).toUpperCase()} is not in the font`);
    }
    const contours = parseGlyph(b, glyfOffset, offsets, gid)
      .map(dropDuplicatePoints)
      .filter((c) => c.length >= 3);

    if (cellH !== cellW) throw new Error('TTF: non-square cells are not supported yet');
    const cell = unitsPerEm / cellW;
    if (!Number.isInteger(cell)) {
      throw new Error(`TTF: unitsPerEm ${unitsPerEm} is not divisible by cell size ${cellW}`);
    }

    const rowBytes = Math.ceil(cellW / 8);
    const rows = Buffer.alloc(rowBytes * cellH);
    for (let r = 0; r < cellH; r++) {
      for (let c = 0; c < cellW; c++) {
        const x = (c + 0.5) * cell;
        const y = ascent - (r + 0.5) * cell;
        if (insideEvenOdd(contours, x, y)) {
          rows[r * rowBytes + (c >> 3)] |= 0x80 >> (c & 7);
        }
      }
    }
    return { gid, rows, rowBytes, advance: advance(gid) };
  }

  return {
    file,
    unitsPerEm,
    ascent,
    descent,
    cell: unitsPerEm / 8,
    glyphCount: numGlyphs,
    gidFor,
    rasterize,
  };
}

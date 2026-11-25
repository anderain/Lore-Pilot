// Verify the byte-addressed pixel logic in reader/Src/ScreenBuf.c.
//
// ScreenBuf.c writes packed 1bpp pixels with explicit shift masks, because
// Palm OS 2.0 exposes no way to address a single bitmap bit (BmpGetBits is
// 3.5+). That arithmetic is easy to get subtly wrong at byte boundaries, so
// this script ports SBMaskByte / SBRun / ScreenBufBits to TypeScript and
// compares them against a naive one-pixel-at-a-time reference.
//
//   npx tsx scripts/check-screenbuf.ts

const W = 160;
const ROW_BYTES = W / 8;
const H = 160;

type Ink = 'black' | 'white' | 'gray';

const INKS: Ink[] = ['black', 'white', 'gray'];
let comparisons = 0;

// ---------------------------------------------------------------- reference
/** Naive reference: one bit at a time, no byte tricks. */
function refCanvas(): Uint8Array {
  return new Uint8Array(W * H);
}

function refRun(canvas: Uint8Array, y: number, x0: number, x1: number, ink: Ink) {
  if (y < 0 || y >= H) return;
  const a = Math.max(0, Math.min(x0, x1));
  const b = Math.min(W - 1, Math.max(x0, x1));
  if (a > b) return;
  for (let x = a; x <= b; x++) {
    if (ink === 'gray') canvas[y * W + x] = (x + y) & 1 ? 1 : 0;
    else canvas[y * W + x] = ink === 'black' ? 1 : 0;
  }
}

// ------------------------------------------------------- port of ScreenBuf.c
/** Byte-addressed port that mirrors SBMaskByte / SBRun / ScreenBufBits. */
class Buf {
  bits: Uint8Array;
  height: number;

  constructor(height: number) {
    this.height = height;
    this.bits = new Uint8Array(height * ROW_BYTES);
  }

  private maskByte(offset: number, mask: number, ink: Ink) {
    if (ink === 'black') this.bits[offset] |= mask;
    else this.bits[offset] &= ~mask & 0xff;
  }

  run(y: number, x0: number, x1: number, ink: Ink) {
    if (y < 0 || y >= this.height) return;
    let a = x0;
    let b = x1;
    if (a > b) [a, b] = [b, a];
    if (a < 0) a = 0;
    if (b > W - 1) b = W - 1;
    if (a > b) return;

    const line = y * ROW_BYTES;

    if (ink === 'gray') {
      for (let x = a; x <= b; x++) {
        const bit = 0x80 >> (x & 7);
        if ((x + y) & 1) this.bits[line + (x >> 3)] |= bit;
        else this.bits[line + (x >> 3)] &= ~bit & 0xff;
      }
      return;
    }

    const lo = a & 7;
    const hi = b & 7;
    const b0 = a >> 3;
    const b1 = b >> 3;

    if (b0 === b1) {
      this.maskByte(line + b0, (0xff >> lo) & ((0xff << (7 - hi)) & 0xff), ink);
      return;
    }

    this.maskByte(line + b0, 0xff >> lo, ink);
    for (let k = b0 + 1; k < b1; k++) this.maskByte(line + k, 0xff, ink);
    this.maskByte(line + b1, (0xff << (7 - hi)) & 0xff, ink);
  }

  /** ScreenBufBits, including the byte-aligned fast path. */
  blit(x: number, y: number, rows: Uint8Array, w: number, h: number,
       rowBytes: number, ink: Ink) {
    if (w === 0 || h === 0 || rowBytes === 0) return;

    if (ink === 'black' && (x & 7) === 0 && x >= 0 && x + w <= W &&
        (w & 7) === 0 && rowBytes === w >> 3) {
      const dstBytes = w >> 3;
      const baseCol = x >> 3;
      for (let r = 0; r < h; r++) {
        const sy = y + r;
        if (sy < 0 || sy >= this.height) continue;
        const dst = sy * ROW_BYTES + baseCol;
        for (let c = 0; c < dstBytes; c++) this.bits[dst + c] |= rows[r * rowBytes + c];
      }
      return;
    }

    for (let r = 0; r < h; r++) {
      let c = 0;
      while (c < w) {
        if ((rows[r * rowBytes + (c >> 3)] & (0x80 >> (c & 7))) === 0) {
          c++;
          continue;
        }
        const start = c;
        while (c < w && (rows[r * rowBytes + (c >> 3)] & (0x80 >> (c & 7))) !== 0) c++;
        this.run(y + r, x + start, x + c - 1, ink);
      }
    }
  }

  /** Unpack to one byte per pixel for comparison. */
  pixels(): Uint8Array {
    const out = new Uint8Array(W * this.height);
    for (let y = 0; y < this.height; y++) {
      for (let x = 0; x < W; x++) {
        out[y * W + x] =
          (this.bits[y * ROW_BYTES + (x >> 3)] & (0x80 >> (x & 7))) !== 0 ? 1 : 0;
      }
    }
    return out;
  }
}

// ------------------------------------------------------------------- helpers
function rng(seed: number) {
  let s = seed >>> 0;
  return () => {
    s = (s * 1664525 + 1013904223) >>> 0;
    return s / 0x100000000;
  };
}

function compare(label: string, ref: Uint8Array, got: Uint8Array) {
  for (let i = 0; i < ref.length; i++) {
    if (ref[i] !== got[i]) {
      const x = i % W;
      const y = Math.floor(i / W);
      throw new Error(`${label}: mismatch at (${x},${y}) ref=${ref[i]} got=${got[i]}`);
    }
  }
  comparisons++;
}

// ------------------------------------------------------------------ the check
function checkRuns() {
  for (let x0 = 0; x0 < W; x0 += 7) {
    for (const len of [0, 1, 2, 7, 8, 9, 15, 16, 33, 80, 159]) {
      const x1 = Math.min(W - 1, x0 + len);
      const ref = refCanvas();
      const buf = new Buf(H);
      refRun(ref, 40, x0, x1, 'black');
      buf.run(40, x0, x1, 'black');
      compare(`run ${x0}..${x1}`, ref, buf.pixels());
    }
  }
}

function checkClears() {
  for (let x0 = 0; x0 < W; x0 += 5) {
    for (const len of [0, 1, 8, 17, 40]) {
      const x1 = Math.min(W - 1, x0 + len);
      const ref = refCanvas();
      ref.fill(1);
      const buf = new Buf(H);
      for (let y = 0; y < H; y++) buf.run(y, 0, W - 1, 'black');
      refRun(ref, 12, x0, x1, 'white');
      buf.run(12, x0, x1, 'white');
      compare(`white ${x0}..${x1}`, ref, buf.pixels());
    }
  }
}

function checkClipping() {
  const cases: Array<[number, number]> = [
    [-1, 10], [-3, -1], [-50, 200], [157, 170], [160, 180],
    [-160, 0], [0, 159], [-8, 159],
  ];
  for (const [x0, x1] of cases) {
    const ref = refCanvas();
    const buf = new Buf(H);
    refRun(ref, 70, x0, x1, 'black');
    buf.run(70, x0, x1, 'black');
    compare(`clip ${x0}..${x1}`, ref, buf.pixels());
  }
}

function checkGray() {
  const ref = refCanvas();
  const buf = new Buf(H);
  refRun(ref, 100, 0, 159, 'gray');
  buf.run(100, 0, 159, 'gray');
  compare('gray row', ref, buf.pixels());

  const px = buf.pixels().slice(100 * W, 101 * W);
  const ones = px.reduce((n, v) => n + v, 0);
  if (ones !== 80) throw new Error(`gray coverage ${ones}, expected 80`);
}

function checkGlyphs() {
  const rand = rng(12345);
  for (let trial = 0; trial < 500; trial++) {
    const cell = trial % 3 === 0 ? 12 : 8;
    const rowBytes = cell / 8;
    const glyph = new Uint8Array(cell * rowBytes);
    for (let i = 0; i < glyph.length; i++) glyph[i] = Math.floor(rand() * 256) & 0xff;

    const x = Math.floor(rand() * 200) - 20;
    const y = Math.floor(rand() * 180) - 10;
    const ink: Ink = trial % 4 === 0 ? 'gray' : 'black';

    const ref = refCanvas();
    for (let r = 0; r < cell; r++) {
      for (let c = 0; c < cell; c++) {
        if ((glyph[r * rowBytes + (c >> 3)] & (0x80 >> (c & 7))) === 0) continue;
        refRun(ref, y + r, x + c, x + c, ink);
      }
    }

    const buf = new Buf(H);
    buf.blit(x, y, glyph, cell, cell, rowBytes, ink);
    compare(`glyph ${cell}px at (${x},${y}) ${ink}`, ref, buf.pixels());
  }
}

function checkFuzz() {
  const rand = rng(999);
  for (let trial = 0; trial < 60; trial++) {
    const ref = refCanvas();
    const buf = new Buf(H);
    for (let op = 0; op < 90; op++) {
      const y = Math.floor(rand() * 180) - 10;
      const x0 = Math.floor(rand() * 200) - 20;
      const len = Math.floor(rand() * 60);
      const ink = INKS[Math.floor(rand() * 3)];
      refRun(ref, y, x0, x0 + len, ink);
      buf.run(y, x0, x0 + len, ink);
    }
    compare(`fuzz ${trial}`, ref, buf.pixels());
  }
}

function checkNeighbours() {
  // Clearing a run must not wipe the pixel next to it in the same byte; that
  // is exactly what the shift masks exist to prevent.
  const buf = new Buf(H);
  buf.run(50, 0, W - 1, 'black');
  buf.run(50, 3, 12, 'white');
  const px = buf.pixels();
  for (let x = 0; x < W; x++) {
    const expected = !(x >= 3 && x <= 12);
    if ((px[50 * W + x] === 1) !== expected) {
      throw new Error(`neighbour at x=${x} is ${px[50 * W + x]}`);
    }
  }
  comparisons++;
}

const suites: Array<[string, () => void]> = [
  ['runs', checkRuns],
  ['clears', checkClears],
  ['clipping', checkClipping],
  ['gray', checkGray],
  ['glyphs', checkGlyphs],
  ['fuzz', checkFuzz],
  ['neighbours', checkNeighbours],
];

let failed = 0;
for (const [name, fn] of suites) {
  try {
    fn();
    console.log(`ok   ${name}`);
  } catch (error) {
    failed++;
    console.log(`FAIL ${name}: ${error instanceof Error ? error.message : error}`);
  }
}

console.log(`${comparisons} frame comparisons, ${failed} suite(s) failed`);
if (failed > 0) process.exitCode = 1;

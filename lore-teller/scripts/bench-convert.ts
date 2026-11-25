// Compare the synchronous pipeline with the async (worker) one on a large
// book, and report how long the main thread was actually blocked.
//
//   npx tsx scripts/bench-convert.ts [megabytes]

import { convertText } from '../src/lib/convert';
import type { ConvertProgress } from '../src/lib/progress';

const megabytes = Number(process.argv[2] ?? 8);

function makeText(mb: number): string {
  const line = '天地玄黄，宇宙洪荒。日月盈亏，辰宿列张。寒来暑往，秋收冬藏。\n';
  const repeats = Math.ceil((mb * 1024 * 1024) / Buffer.byteLength(line, 'utf8'));
  return line.repeat(repeats);
}

const text = makeText(megabytes);
const buffer = new TextEncoder().encode(text).buffer as ArrayBuffer;
console.log(`input ${(buffer.byteLength / 1024 / 1024).toFixed(1)} MB`);

const options = {
  displayName: 'BENCH',
  cell: 8 as const,
  pagesPerPart: Number(process.argv[3] ?? 1000),
  encoding: 'utf-8' as const,
};

const reports: Array<{ at: number; phase: string; percent: number }> = [];
const t0 = performance.now();
const result = convertText(buffer, {
  ...options,
  onProgress: (p) => {
    reports.push({ at: performance.now(), phase: p.phase, percent: p.percent });
  },
});
const t1 = performance.now();

console.log(`sync  ${(t1 - t0).toFixed(0)} ms`);
console.log(
  `      ${result.charCount} chars, ${result.totalPages} pages, ` +
    `${result.parts.length} volumes, ${(result.totalBytes / 1024 / 1024).toFixed(1)} MB output`,
);
console.log(
  `      ${reports.length} progress reports, last at ${reports.at(-1)?.percent}%`,
);

// Rough split of where the time goes, to sanity check the phase weights.
const phases = new Map<string, number>();
let prev = t0;
for (const r of reports) {
  phases.set(r.phase, (phases.get(r.phase) ?? 0) + (r.at - prev));
  prev = r.at;
}
for (const [phase, ms] of phases) {
  console.log(`      ${phase.padEnd(7)} ${ms.toFixed(0)} ms`);
}

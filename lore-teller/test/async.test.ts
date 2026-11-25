// The async pipeline must produce byte-identical output to the sync one, and
// report progress that only ever moves forward.

import { describe, expect, it } from 'vitest';
import { convertText } from '../src/lib/convert';
import { convertTextAsync } from '../src/lib/convertAsync';
import type { ConvertProgress } from '../src/lib/progress';
import { phasePercent } from '../src/lib/progress';

const text =
  '天地玄黄，宇宙洪荒。日月盈亏，辰宿列张。\n'.repeat(400) + '最后一行。';

function encode(text: string): ArrayBuffer {
  return new TextEncoder().encode(text).buffer as ArrayBuffer;
}

const base = {
  displayName: 'ASYNC',
  cell: 8 as const,
  pagesPerPart: 3,
  encoding: 'utf-8' as const,
};

describe('async conversion', () => {
  it('matches the synchronous result byte for byte', async () => {
    const sync = convertText(encode(text), base);
    // jsdom has no Worker, so this exercises the inline fallback path.
    const async = await convertTextAsync(encode(text), base);

    expect(async.parts.length).toBe(sync.parts.length);
    expect(async.totalPages).toBe(sync.totalPages);
    for (let i = 0; i < sync.parts.length; i++) {
      expect(async.parts[i].fileName).toBe(sync.parts[i].fileName);
      expect([...async.parts[i].bytes]).toEqual([...sync.parts[i].bytes]);
    }
  });

  it('reports progress in a non-decreasing order ending at 100', () => {
    const seen: ConvertProgress[] = [];
    convertText(encode(text), { ...base, onProgress: (p) => seen.push(p) });

    expect(seen.length).toBeGreaterThan(2);
    let last = -1;
    for (const p of seen) {
      expect(p.percent).toBeGreaterThanOrEqual(last);
      expect(p.percent).toBeLessThanOrEqual(100);
      last = p.percent;
    }
    expect(seen[seen.length - 1].percent).toBe(100);
  });

  it('clamps phase percentages', () => {
    expect(phasePercent('decode', 0, 1)).toBe(0);
    expect(phasePercent('decode', 1, 1)).toBe(5);
    expect(phasePercent('layout', 0, 10)).toBe(5);
    expect(phasePercent('layout', 10, 10)).toBe(25);
    expect(phasePercent('pack', 10, 10)).toBe(100);
    expect(phasePercent('pack', 0, 0)).toBe(25);
  });

  it('accepts 1000 pages per volume', () => {
    const result = convertText(encode(text), { ...base, pagesPerPart: 1000 });
    expect(result.parts.length).toBe(1);
    expect(result.parts[0].pageCount).toBe(result.totalPages);
  });
});

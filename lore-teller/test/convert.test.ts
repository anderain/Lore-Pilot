// Unit tests for the text -> PDB pipeline. Run with `npm test`.

import { describe, expect, it } from 'vitest';
import { CELL_CONFIG, convertText } from '../src/lib/convert';
import { lookupGb } from '../src/lib/gb2312';
import { wrapLine } from '../src/lib/layout';
import { toFullwidth, toFullwidthText } from '../src/lib/toFullwidth';

function encode(text: string, encoding: 'utf-8' | 'gbk'): ArrayBuffer {
  if (encoding === 'utf-8') return new TextEncoder().encode(text).buffer as ArrayBuffer;

  // Minimal GBK encoder via a reverse table for tests.
  const map = new Map<string, number>();
  const dec = new TextDecoder('gbk');
  for (let hi = 0x81; hi <= 0xfe; hi++) {
    for (let lo = 0x40; lo <= 0xfe; lo++) {
      if (lo === 0x7f) continue;
      const ch = dec.decode(Uint8Array.from([hi, lo]));
      if ([...ch].length === 1 && !map.has(ch)) map.set(ch, (hi << 8) | lo);
    }
  }
  const bytes: number[] = [];
  for (const ch of text) {
    const code = map.get(ch);
    if (code === undefined) throw new Error(`no GBK for ${ch}`);
    bytes.push((code >> 8) & 0xff, code & 0xff);
  }
  return new Uint8Array(bytes).buffer as ArrayBuffer;
}

interface ParsedPage {
  lineCount: number;
  lines: string[];
}

function readPdb(buf: Uint8Array) {
  const be16 = (o: number) => (buf[o] << 8) | buf[o + 1];
  const be32 = (o: number) =>
    ((buf[o] << 24) | (buf[o + 1] << 16) | (buf[o + 2] << 8) | buf[o + 3]) >>> 0;

  const num = be16(76);
  const offsets: number[] = [];
  for (let i = 0; i < num; i++) offsets.push(be32(78 + i * 8));

  const header = buf.subarray(offsets[0], offsets[0] + 64);
  const h16 = (o: number) => (header[o] << 8) | header[o + 1];
  const h32 = (o: number) =>
    ((header[o] << 24) | (header[o + 1] << 16) | (header[o + 2] << 8) | header[o + 3]) >>> 0;

  const decoder = new TextDecoder('gbk');
  const pages: ParsedPage[] = [];
  for (let i = 1; i < offsets.length; i++) {
    const start = offsets[i];
    const end = i + 1 < offsets.length ? offsets[i + 1] : buf.length;
    const rec = buf.subarray(start, end);
    const r16 = (o: number) => (rec[o] << 8) | rec[o + 1];
    const lineCount = r16(0);
    const ends: number[] = [];
    for (let k = 0; k < lineCount; k++) ends.push(r16(4 + k * 2));
    const textStart = 4 + lineCount * 2 + 2;
    const textLen = r16(textStart - 2);
    const textBytes = rec.subarray(textStart, textStart + textLen);
    const lines: string[] = [];
    let prev = 0;
    for (const lineEnd of ends) {
      lines.push(decoder.decode(textBytes.subarray(prev, lineEnd)));
      prev = lineEnd;
    }
    pages.push({ lineCount, lines });
  }

  return {
    name: new TextDecoder('ascii').decode(buf.subarray(0, 32)).replace(/\0.*$/, ''),
    type: new TextDecoder('ascii').decode(buf.subarray(60, 64)),
    creator: new TextDecoder('ascii').decode(buf.subarray(64, 68)),
    version: h16(4),
    cell: h16(8),
    charsPerLine: h16(10),
    linesPerPage: h16(12),
    partIndex: h16(18),
    partCount: h16(20),
    progressRows: h16(42),
    firstPage: h32(24),
    pageCount: h32(28),
    totalPages: h32(32),
    displayName: new TextDecoder('ascii').decode(header.subarray(44, 60)).replace(/\0.*$/, ''),
    pages,
  };
}

const qianziwen =
  '天地玄黄，宇宙洪荒。日月盈亏，辰宿列张。寒来暑往，秋收冬藏。闰余成岁，律吕调阳。';

describe('fullwidth conversion', () => {
  it('maps ASCII and space into GB2312 fullwidth forms', () => {
    expect(toFullwidth('A')).toBe('Ａ');
    expect(toFullwidth('1')).toBe('１');
    expect(toFullwidth(' ')).toBe('\u3000');
    expect(lookupGb(toFullwidth('A'))?.code).toBe(0xa3c1);
  });

  it('covers every printable ASCII character', () => {
    for (let cp = 0x21; cp <= 0x7e; cp++) {
      const ch = String.fromCharCode(cp);
      expect(lookupGb(toFullwidth(ch)), ch).not.toBeNull();
    }
    expect(toFullwidthText('a b')).toBe('ａ\u3000ｂ');
  });
});

describe('line wrapping', () => {
  it('wraps at the cell count', () => {
    const unsupported = new Map<string, number>();
    const lines = wrapLine('天'.repeat(45), 20, unsupported);
    expect(lines.map((l) => [...l].length)).toEqual([20, 20, 5]);
  });

  it('never starts a line with closing punctuation', () => {
    const unsupported = new Map<string, number>();
    const text = '天'.repeat(20) + '，下一句';
    const lines = wrapLine(text, 20, unsupported);
    expect([...lines[0]].length).toBe(19);
    expect(lines[0].endsWith('，')).toBe(false);
    expect(lines[1].startsWith('天')).toBe(true);
    expect(lines[1]).toContain('，');
  });

  it('keeps blank lines', () => {
    const unsupported = new Map<string, number>();
    expect(wrapLine('', 20, unsupported)).toEqual(['']);
  });
});

describe('conversion', () => {
  it('produces the documented header and one page per 17 lines (8px)', () => {
    const result = convertText(encode(qianziwen, 'utf-8'), {
      displayName: 'QZW',
      cell: 8,
      pagesPerPart: 100,
      encoding: 'utf-8',
    });
    expect(result.parts).toHaveLength(1);

    const pd = readPdb(result.parts[0].bytes);
    expect(pd.name).toBe('LPB_QZW_01');
    expect(pd.type).toBe('LPBK');
    expect(pd.creator).toBe('LPRD');
    expect(pd.version).toBe(1);
    expect(pd.cell).toBe(8);
    expect(pd.charsPerLine).toBe(20);
    expect(pd.linesPerPage).toBe(17);
    expect(pd.progressRows).toBe(2);
    expect(pd.partIndex).toBe(0);
    expect(pd.partCount).toBe(1);
    expect(pd.firstPage).toBe(1);
    expect(pd.totalPages).toBe(result.totalPages);
    expect(pd.displayName).toBe('QZW');
    expect(pd.pages.length).toBe(result.totalPages);
    expect(pd.pages[0].lines.join('')).toContain('天地玄黄');
    for (const page of pd.pages) {
      expect(page.lineCount).toBeLessThanOrEqual(17);
      for (const line of page.lines) expect([...line].length).toBeLessThanOrEqual(20);
    }
  });

  it('preserves blank lines as blank lines', () => {
    const result = convertText(encode('第一段\n\n第二段', 'utf-8'), {
      displayName: 'T',
      cell: 8,
      pagesPerPart: 100,
      encoding: 'utf-8',
    });
    const pd = readPdb(result.parts[0].bytes);
    expect(pd.pages[0].lines).toEqual(['第一段', '', '第二段']);
    expect(pd.pages[0].lineCount).toBe(3);
  });

  it('converts ASCII to fullwidth in the body', () => {
    const result = convertText(encode('Hello, 2026!', 'utf-8'), {
      displayName: 'ASCII',
      cell: 8,
      pagesPerPart: 100,
      encoding: 'utf-8',
    });
    const pd = readPdb(result.parts[0].bytes);
    expect(pd.pages[0].lines[0]).toBe('Ｈｅｌｌｏ，\u3000２０２６！');
  });

  it('reports unsupported characters and replaces them with a fullwidth question mark', () => {
    const result = convertText(encode('天🧪', 'utf-8'), {
      displayName: 'MISS',
      cell: 8,
      pagesPerPart: 100,
      encoding: 'utf-8',
    });
    expect(result.replaced).toContain('🧪');
    expect(result.warnings.join('')).toContain('GB2312');
    const pd = readPdb(result.parts[0].bytes);
    expect(pd.pages[0].lines[0]).toBe('天？');
  });

  it('splits into volumes at page boundaries and keeps page numbers global', () => {
    const many = Array.from({ length: 400 }, (_, i) => `第${i + 1}段内容。`).join('\n');
    const result = convertText(encode(many, 'utf-8'), {
      displayName: 'SPLIT',
      cell: 8,
      pagesPerPart: 3,
      encoding: 'utf-8',
    });
    expect(result.parts.length).toBeGreaterThan(1);

    let expectedFirst = 1;
    for (const [i, part] of result.parts.entries()) {
      const pd = readPdb(part.bytes);
      expect(pd.name).toBe(`LPB_SPLIT_${String(i + 1).padStart(2, '0')}`);
      expect(pd.partIndex).toBe(i);
      expect(pd.partCount).toBe(result.parts.length);
      expect(pd.firstPage).toBe(expectedFirst);
      expect(pd.pageCount).toBe(part.pageCount);
      expect(pd.totalPages).toBe(result.totalPages);
      // A volume holds exactly pagesPerPart pages, except the last one.
      const expectedPages = Math.min(3, result.totalPages - (expectedFirst - 1));
      expect(part.pageCount).toBe(expectedPages);
      expectedFirst += part.pageCount;
    }
    expect(expectedFirst - 1).toBe(result.totalPages);
  });

  it('refuses more than 99 volumes', () => {
    const many = Array.from({ length: 4000 }, (_, i) => `第${i + 1}段。`).join('\n');
    expect(() =>
      convertText(encode(many, 'utf-8'), {
        displayName: 'TOOMANY',
        cell: 8,
        pagesPerPart: 1,
        encoding: 'utf-8',
      }),
    ).toThrow(/99/);
  });

  it('keeps every volume to pagesPerPart pages', () => {
    const many = Array.from({ length: 200 }, () => '字'.repeat(20)).join('\n');
    const result = convertText(encode(many, 'utf-8'), {
      displayName: 'BIG',
      cell: 8,
      pagesPerPart: 2,
      encoding: 'utf-8',
    });
    expect(result.parts.length).toBeGreaterThan(1);
    for (const part of result.parts) {
      expect(part.pageCount).toBeLessThanOrEqual(2);
    }
  });

  it('uses 13 chars per line and 11 lines per page at 12px', () => {
    expect(CELL_CONFIG[12]).toEqual({ charsPerLine: 13, linesPerPage: 11 });
    const result = convertText(encode(qianziwen, 'gbk'), {
      displayName: 'Q12',
      cell: 12,
      pagesPerPart: 100,
      encoding: 'gbk',
    });
    const pd = readPdb(result.parts[0].bytes);
    expect(pd.cell).toBe(12);
    expect(pd.charsPerLine).toBe(13);
    expect(pd.linesPerPage).toBe(11);
    for (const page of pd.pages) {
      expect(page.lineCount).toBeLessThanOrEqual(11);
      for (const line of page.lines) expect([...line].length).toBeLessThanOrEqual(13);
    }
  });

  it('decodes a GBK source file when asked', () => {
    const result = convertText(encode(qianziwen, 'gbk'), {
      displayName: 'GBK',
      cell: 8,
      pagesPerPart: 100,
      encoding: 'gbk',
    });
    const pd = readPdb(result.parts[0].bytes);
    expect(pd.pages[0].lines.join('')).toContain('天地玄黄');
  });
});

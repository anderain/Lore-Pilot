// End-to-end text -> PDB pipeline.

import { gbTableSize, isDenseGb } from './gb2312';
import { wrapAll } from './layout';
import { paginate } from './paginate';
import { toFullwidthText } from './toFullwidth';
import { buildTextParts, type BuildResult } from './split';
import type { TextEncoding } from './decode';

export interface ConvertOptions {
  displayName: string;
  cell: 8 | 12;
  partLimit: number;
  encoding: TextEncoding;
}

export interface ConvertResult extends BuildResult {
  charsPerLine: number;
  linesPerPage: number;
  lineCount: number;
  charCount: number;
  displayName: string;
  cell: 8 | 12;
  fontSupported: boolean;
  warnings: string[];
}

export const CELL_CONFIG = {
  8: { charsPerLine: 20, linesPerPage: 18 },
  12: { charsPerLine: 13, linesPerPage: 12 },
} as const;

export function displayNameFromFile(fileName: string): string {
  const base = fileName.replace(/\.[^.]+$/, '');
  const cleaned = base.replace(/[^A-Za-z0-9]/g, '').toUpperCase();
  return (cleaned || 'BOOK').slice(0, 16);
}

export function convertText(buffer: ArrayBuffer, options: ConvertOptions): ConvertResult {
  const { charsPerLine, linesPerPage } = CELL_CONFIG[options.cell];

  // Trust the user's encoding choice: strict decode so mojibake cannot slip
  // through silently.
  let decodedText: string;
  try {
    decodedText = new TextDecoder(options.encoding, { fatal: true }).decode(buffer);
  } catch {
    throw new Error(
      options.encoding === 'utf-8'
        ? '按 UTF-8 解码失败：文件可能不是 UTF-8，请切换到 GBK 再试。'
        : '按 GBK 解码失败：文件可能不是 GBK，请切换到 UTF-8 再试。',
    );
  }
  const normalised = stripBom(decodedText).replace(/\r\n?/g, '\n');

  // Body characters all become fullwidth so every cell is one glyph.
  const fullwidthLines = normalised
    .split('\n')
    .map((line) => toFullwidthText(line));
  const logicalLines = fullwidthLines.map((text) => ({ text, hard: true }));

  const wrapped = wrapAll(logicalLines, charsPerLine);
  const pages = paginate(wrapped.lines, linesPerPage);

  const result = buildTextParts(pages, {
    displayName: options.displayName,
    cell: options.cell,
    charsPerLine,
    linesPerPage,
    partLimit: options.partLimit,
  });

  const charCount = [...normalised.replace(/\n/g, '')].length;
  const warnings: string[] = [];
  if (result.replaced.length > 0) {
    warnings.push(
      `${result.replaced.length} 个字符不在 GB2312 内，已替换为全角问号：${result.replaced
        .slice(0, 20)
        .join('')}${result.replaced.length > 20 ? '…' : ''}`,
    );
  }
  if (result.parts.length > 1) {
    warnings.push(`已按 ${options.partLimit / 1024}K 拆分为 ${result.parts.length} 个分卷。`);
  }

  return {
    ...result,
    charsPerLine,
    linesPerPage,
    lineCount: wrapped.lines.length,
    charCount,
    displayName: options.displayName,
    cell: options.cell,
    fontSupported: gbTableSize() > 0,
    warnings,
  };
}

function stripBom(text: string): string {
  return text.charCodeAt(0) === 0xfeff ? text.slice(1) : text;
}

/** Count characters not representable in the dense GB2312 grid. */
export function countUnsupported(text: string): Map<string, number> {
  const map = new Map<string, number>();
  for (const ch of toFullwidthText(text)) {
    if (!isDenseGb(ch)) map.set(ch, (map.get(ch) ?? 0) + 1);
  }
  return map;
}

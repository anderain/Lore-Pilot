// Line breaking with simple Chinese typography rules.
//
// Body text is pre-converted to fullwidth cells, so each unit is exactly one
// cell wide. Only hard breaks and the character count matter.

import { isDenseGb, lookupGb } from './gb2312';

/** Characters that must not start a line. */
const NO_LINE_START = new Set(
  [...'，。、；：？！）》」』】〕〉…—·"\'%‰℃"'],
);
/** Characters that must not end a line. */
const NO_LINE_END = new Set([...'（《「『【〔〈"\'']);

export interface RawLine {
  text: string;
  /** Hard break from the source (explicit newline or paragraph end). */
  hard: boolean;
}

/**
 * Split normalised text into logical lines on LF. Empty lines are preserved
 * (the user asked for blank lines to stay as blank lines).
 */
export function splitLogicalLines(text: string): RawLine[] {
  return text.split('\n').map((line) => ({ text: line, hard: true }));
}

export interface WrapResult {
  lines: string[];
  /** Characters that could not be encoded (replaced by U+FF1F). */
  unsupported: Map<string, number>;
}

/**
 * Wrap one logical line into chunks of at most `perLine` cells.
 * Spacing is preserved; leading fullwidth spaces count as cells.
 */
export function wrapLine(
  line: string,
  perLine: number,
  unsupported: Map<string, number>,
): string[] {
  if (line.length === 0) return [''];

  const cells = [...line];
  const out: string[] = [];
  let start = 0;

  while (start < cells.length) {
    let end = Math.min(start + perLine, cells.length);

    if (end < cells.length) {
      // Pull one character back if the break would violate the rules.
      let guard = 0;
      while (end > start + 1 && guard < perLine) {
        const next = cells[end];
        const prev = cells[end - 1];
        if (NO_LINE_START.has(next) || NO_LINE_END.has(prev)) {
          end--;
          guard++;
          continue;
        }
        break;
      }
      // Avoid an empty or near-empty line caused by aggressive pulling.
      if (end <= start) end = Math.min(start + perLine, cells.length);
    }

    const chunk = cells.slice(start, end).join('');
    out.push(chunk);
    start = end;
  }

  for (const ch of line) {
    if (!isDenseGb(ch)) {
      unsupported.set(ch, (unsupported.get(ch) ?? 0) + 1);
    }
  }

  return out;
}

/** Wrap every logical line; blank source lines stay blank. */
export function wrapAll(
  lines: RawLine[],
  perLine: number,
): WrapResult {
  const unsupported = new Map<string, number>();
  const out: string[] = [];
  for (const line of lines) {
    out.push(...wrapLine(line.text, perLine, unsupported));
  }
  return { lines: out, unsupported };
}

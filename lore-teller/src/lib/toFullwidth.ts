// ASCII -> fullwidth conversion, so every body character occupies one cell.
//
// The 8px and 12px fonts both cover U+FF01-U+FF5E and U+3000, and GB2312
// encodes them (94/94 verified), so body text stays pure double-byte.

const IDEOGRAPHIC_SPACE = '\u3000';

/** Convert one character to its fullwidth body form. */
export function toFullwidth(ch: string): string {
  const cp = ch.codePointAt(0) ?? 0;
  if (cp === 0x20 || cp === 0x09) return IDEOGRAPHIC_SPACE;
  if (cp >= 0x21 && cp <= 0x7e) return String.fromCodePoint(cp + 0xfee0);
  return ch;
}

/** Convert a whole ASCII run; non-ASCII characters pass through. */
export function toFullwidthText(text: string): string {
  let out = '';
  for (const ch of text) out += toFullwidth(ch);
  return out;
}

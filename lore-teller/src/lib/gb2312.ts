// GB2312 (and the GBK superset) char -> double-byte code.
//
// Built once at runtime by walking the two-byte space and decoding with the
// browser's GBK decoder. The dense GB2312 grid is A1-F7 x A1-FE; GBK extras
// (trail byte 0x40-0xA0) are kept so rare characters can be reported rather
// than silently mangled.

export interface GbCode {
  code: number;
  dense: boolean;
}

let table: Map<string, GbCode> | null = null;

function buildTable(): Map<string, GbCode> {
  const decoder = new TextDecoder('gbk');
  const map = new Map<string, GbCode>();

  for (let hi = 0x81; hi <= 0xfe; hi++) {
    for (let lo = 0x40; lo <= 0xfe; lo++) {
      if (lo === 0x7f) continue;
      let ch: string;
      try {
        ch = decoder.decode(Uint8Array.from([hi, lo]));
      } catch {
        continue;
      }
      if ([...ch].length !== 1) continue;
      const cp = ch.codePointAt(0) ?? 0;
      if (cp < 0x80 || cp === 0xfffd) continue;
      if (map.has(ch)) continue;
      const dense = hi >= 0xa1 && hi <= 0xf7 && lo >= 0xa1 && lo <= 0xfe;
      map.set(ch, { code: (hi << 8) | lo, dense });
    }
  }
  return map;
}

function getTable(): Map<string, GbCode> {
  if (table === null) table = buildTable();
  return table;
}

/** Look up a single character. Returns null when it is not representable. */
export function lookupGb(ch: string): GbCode | null {
  return getTable().get(ch) ?? null;
}

/** True when the character can be stored in the dense GB2312 grid. */
export function isDenseGb(ch: string): boolean {
  const hit = lookupGb(ch);
  return hit !== null && hit.dense;
}

export function gbTableSize(): number {
  return getTable().size;
}

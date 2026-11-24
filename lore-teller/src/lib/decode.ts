// Text decode helpers: UTF-8 first (strict), then GBK fallback.

export type TextEncoding = 'utf-8' | 'gbk';

export interface DecodedText {
  text: string;
  encoding: TextEncoding;
  bytes: number;
}

function tryDecode(buffer: ArrayBuffer, encoding: string, fatal: boolean): string | null {
  try {
    return new TextDecoder(encoding, { fatal }).decode(buffer);
  } catch {
    return null;
  }
}

/** Decode with strict UTF-8 first; fall back to GBK. */
export function decodeText(buffer: ArrayBuffer): DecodedText {
  const utf8 = tryDecode(buffer, 'utf-8', true);
  if (utf8 !== null) {
    return { text: stripBom(utf8), encoding: 'utf-8', bytes: buffer.byteLength };
  }
  const gbk = tryDecode(buffer, 'gbk', false) ?? '';
  return { text: stripBom(gbk), encoding: 'gbk', bytes: buffer.byteLength };
}

/** Decode with an explicit encoding, falling back to a lossy decode. */
export function decodeAs(buffer: ArrayBuffer, encoding: TextEncoding): string {
  const text = tryDecode(buffer, encoding, encoding === 'utf-8') ?? tryDecode(buffer, encoding, false) ?? '';
  return stripBom(text);
}

function stripBom(text: string): string {
  return text.charCodeAt(0) === 0xfeff ? text.slice(1) : text;
}

/** Normalise CRLF and lone CR to LF. */
export function normalizeNewlines(text: string): string {
  return text.replace(/\r\n?/g, '\n');
}

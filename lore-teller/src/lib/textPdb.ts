// Text PDB writer. Format is documented in docs/text-pdb.md.
//
// Record 0: 64-byte header
// Record 1..N: one page each:
//   u16 lineCount
//   u16 reserved
//   u16 lineEnd[lineCount]   cumulative byte offsets into text
//   u16 textLen
//   u8  text[textLen]        GB2312 double bytes

import { isDenseGb, lookupGb } from './gb2312';
import type { Page } from './paginate';

export const TEXT_DB_TYPE = 'LPBK';
export const TEXT_DB_CREATOR = 'LPRD';
export const TEXT_DB_MAGIC = 0x4c505458; // 'LPTX'
export const TEXT_DB_VERSION = 1;
export const TEXT_DB_ENCODING_GB2312 = 1;

const PDB_HEADER_SIZE = 78;
const RECORD_ENTRY_SIZE = 8;
const FONT_HEADER_SIZE = 64;
const FULLWIDTH_QUESTION = 0xff1f; // ？

export interface TextPdbHeader {
  cell: 8 | 12;
  charsPerLine: number;
  linesPerPage: number;
  partIndex: number;
  partCount: number;
  firstPage: number;
  pageCount: number;
  totalPages: number;
  displayName: string;
}

export interface BuiltPage {
  bytes: Uint8Array;
  /** Unsupported characters replaced by '？' on this page. */
  replaced: string[];
}

function be16(value: number): number[] {
  return [(value >> 8) & 0xff, value & 0xff];
}

function be32(value: number): number[] {
  return [
    (value >>> 24) & 0xff,
    (value >>> 16) & 0xff,
    (value >>> 8) & 0xff,
    value & 0xff,
  ];
}

/** Encode one display line as GB2312; unknown characters become '？'. */
export function encodeLine(line: string): { bytes: number[]; replaced: string[] } {
  const bytes: number[] = [];
  const replaced: string[] = [];

  for (const ch of line) {
    const hit = lookupGb(ch);
    if (hit === null || !isDenseGb(ch)) {
      replaced.push(ch);
      bytes.push(...be16(lookupGb('？')!.code));
      continue;
    }
    bytes.push(...be16(hit.code));
  }
  return { bytes, replaced };
}

export function buildPageRecord(page: Page): BuiltPage {
  const encodedLines: number[][] = [];
  const replaced: string[] = [];

  for (const line of page.lines) {
    const { bytes, replaced: miss } = encodeLine(line);
    encodedLines.push(bytes);
    replaced.push(...miss);
  }

  const lineCount = encodedLines.length;
  const headerBytes = 4 + lineCount * 2 + 2;
  const text: number[] = [];
  const lineEnd: number[] = [];

  for (const bytes of encodedLines) {
    text.push(...bytes);
    lineEnd.push(text.length);
  }

  const out = new Uint8Array(headerBytes + text.length);
  let p = 0;
  for (const b of be16(lineCount)) out[p++] = b;
  for (const b of be16(0)) out[p++] = b;
  for (const end of lineEnd) for (const b of be16(end)) out[p++] = b;
  for (const b of be16(text.length)) out[p++] = b;
  for (const b of text) out[p++] = b;

  return { bytes: out, replaced };
}

/** Build the 64-byte header record. */
export function buildHeaderRecord(h: TextPdbHeader): Uint8Array {
  const out = new Uint8Array(FONT_HEADER_SIZE);
  const write16 = (offset: number, v: number) => {
    out[offset] = (v >> 8) & 0xff;
    out[offset + 1] = v & 0xff;
  };
  const write32 = (offset: number, v: number) => {
    out[offset] = (v >>> 24) & 0xff;
    out[offset + 1] = (v >>> 16) & 0xff;
    out[offset + 2] = (v >>> 8) & 0xff;
    out[offset + 3] = v & 0xff;
  };

  write32(0, TEXT_DB_MAGIC);
  write16(4, TEXT_DB_VERSION);
  write16(6, 0);                       // flags
  write16(8, h.cell);
  write16(10, h.charsPerLine);
  write16(12, h.linesPerPage);
  write16(14, 8);                      // headerRows
  write16(16, 8);                      // footerRows
  write16(18, h.partIndex);
  write16(20, h.partCount);
  write16(22, TEXT_DB_ENCODING_GB2312);
  write32(24, h.firstPage);
  write32(28, h.pageCount);
  write32(32, h.totalPages);
  write32(36, 0);                      // textBytes, filled by caller? informational
  write16(40, 1);                      // pageRecordStart
  write16(42, 0);                      // reserved

  const name = h.displayName.slice(0, 16);
  for (let i = 0; i < name.length; i++) out[44 + i] = name.charCodeAt(i) & 0x7f;
  out[44 + name.length] = 0;

  return out;
}

/** Assemble a complete PDB from a header and page records. */
export function buildTextPdb(
  name: string,
  header: Uint8Array,
  pages: Uint8Array[],
): Uint8Array {
  const records = [header, ...pages];
  const numRecords = records.length;
  const listEnd = PDB_HEADER_SIZE + numRecords * RECORD_ENTRY_SIZE;

  let total = listEnd;
  for (const r of records) total += r.length;

  const out = new Uint8Array(total);
  const write16 = (offset: number, v: number) => {
    out[offset] = (v >> 8) & 0xff;
    out[offset + 1] = v & 0xff;
  };
  const write32 = (offset: number, v: number) => {
    out[offset] = (v >>> 24) & 0xff;
    out[offset + 1] = (v >>> 16) & 0xff;
    out[offset + 2] = (v >>> 8) & 0xff;
    out[offset + 3] = v & 0xff;
  };

  const nameBytes = name.slice(0, 31);
  for (let i = 0; i < nameBytes.length; i++) out[i] = nameBytes.charCodeAt(i);

  const now = palmSecondsNow();
  write16(32, 0);          // attributes
  write16(34, 1);          // version
  write32(36, now);
  write32(40, now);
  write32(44, 0);
  write32(48, 0);
  write32(52, 0);          // appInfoID
  write32(56, 0);          // sortInfoID
  out[60] = TEXT_DB_TYPE.charCodeAt(0);
  out[61] = TEXT_DB_TYPE.charCodeAt(1);
  out[62] = TEXT_DB_TYPE.charCodeAt(2);
  out[63] = TEXT_DB_TYPE.charCodeAt(3);
  out[64] = TEXT_DB_CREATOR.charCodeAt(0);
  out[65] = TEXT_DB_CREATOR.charCodeAt(1);
  out[66] = TEXT_DB_CREATOR.charCodeAt(2);
  out[67] = TEXT_DB_CREATOR.charCodeAt(3);
  write32(68, numRecords + 1);
  write32(72, 0);
  write16(76, numRecords);

  let offset = listEnd;
  for (let i = 0; i < numRecords; i++) {
    const base = PDB_HEADER_SIZE + i * RECORD_ENTRY_SIZE;
    write32(base, offset);
    out[base + 4] = 0;
    out[base + 5] = (i + 1) >> 16;
    out[base + 6] = ((i + 1) >> 8) & 0xff;
    out[base + 7] = (i + 1) & 0xff;
    out.set(records[i], offset);
    offset += records[i].length;
  }

  return out;
}

function palmSecondsNow(): number {
  const PALM_EPOCH_MS = Date.UTC(1904, 0, 1, 0, 0, 0);
  return Math.max(0, Math.floor((Date.now() - PALM_EPOCH_MS) / 1000));
}

/** Size of one page record, used for part splitting. */
export function pageRecordSize(page: Page): number {
  return buildPageRecord(page).bytes.length;
}

export function questionCode(): number {
  return lookupGb('？')!.code;
}

export function fullwidthQuestion(): string {
  return String.fromCodePoint(FULLWIDTH_QUESTION);
}

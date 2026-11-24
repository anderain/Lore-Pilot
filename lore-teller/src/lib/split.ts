// Split pages into PDB files that respect a per-file size limit.
//
// Pages are never split across files. Each part gets its own 64-byte header
// with partIndex / partCount / firstPage / totalPages so the reader can show
// global page numbers.

import {
  buildHeaderRecord,
  buildPageRecord,
  buildTextPdb,
  type TextPdbHeader,
} from './textPdb';
import type { Page } from './paginate';

/** Progress bar rows written into every header (border + fill). */
export const PROGRESS_ROWS = 2;

export interface BuildOptions {
  displayName: string;
  cell: 8 | 12;
  charsPerLine: number;
  linesPerPage: number;
  /** Per-file limit in bytes, e.g. 64 * 1024. */
  partLimit: number;
}

export interface PartResult {
  fileName: string;
  dbName: string;
  bytes: Uint8Array;
  firstPage: number;
  pageCount: number;
  size: number;
  /** Unsupported characters replaced on this part. */
  replaced: string[];
}

export interface BuildResult {
  parts: PartResult[];
  totalPages: number;
  totalBytes: number;
  replaced: string[];
}

const PDB_OVERHEAD = 78;
const ENTRY_SIZE = 8;

/** File name = database name: LPB_<NAME>_<NN>.pdb */
export function partName(displayName: string, index: number): string {
  return `LPB_${displayName}_${String(index + 1).padStart(2, '0')}`;
}

export function buildTextParts(pages: Page[], options: BuildOptions): BuildResult {
  const records = pages.map((p) => buildPageRecord(p));
  const totalPages = pages.length;

  // Greedy packing: a part holds consecutive pages while it fits.
  // Every part pays for the PDB header, the record-0 entry and the 64-byte
  // header record, so start the running total at that fixed cost.
  const PART_FIXED = PDB_OVERHEAD + ENTRY_SIZE + 64;
  const groups: number[][] = [];
  let current: number[] = [];
  let currentSize = PART_FIXED;

  records.forEach((rec, i) => {
    const entry = ENTRY_SIZE + rec.bytes.length;
    if (current.length > 0 && currentSize + entry > options.partLimit) {
      groups.push(current);
      current = [];
      currentSize = PART_FIXED;
    }
    current.push(i);
    currentSize += entry;
  });
  if (current.length > 0) groups.push(current);

  if (groups.length > 99) {
    throw new Error(
      `需要 ${groups.length} 个分卷，超过 99 个；请选择更大的分卷大小或更小的字体。`,
    );
  }

  const parts: PartResult[] = [];
  let firstPage = 1;
  const replacedAll: string[] = [];

  groups.forEach((indices, partIndex) => {
    const dbName = partName(options.displayName, partIndex);
    const header: TextPdbHeader = {
      cell: options.cell,
      charsPerLine: options.charsPerLine,
      linesPerPage: options.linesPerPage,
      progressRows: PROGRESS_ROWS,
      partIndex,
      partCount: groups.length,
      firstPage,
      pageCount: indices.length,
      totalPages,
      displayName: options.displayName,
    };
    const pageRecords = indices.map((i) => records[i].bytes);
    const replaced = indices.flatMap((i) => records[i].replaced);
    replacedAll.push(...replaced);

    const bytes = buildTextPdb(dbName, buildHeaderRecord(header), pageRecords);
    parts.push({
      fileName: `${dbName}.pdb`,
      dbName,
      bytes,
      firstPage,
      pageCount: indices.length,
      size: bytes.length,
      replaced,
    });
    firstPage += indices.length;
  });

  return {
    parts,
    totalPages,
    totalBytes: parts.reduce((n, p) => n + p.size, 0),
    replaced: [...new Set(replacedAll)],
  };
}

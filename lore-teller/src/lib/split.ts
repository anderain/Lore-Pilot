// Split pages into PDB volumes.
//
// A volume is a standalone book file: it can be installed, opened and deleted
// on its own, which is the whole point on a 1 MB Palm. Volumes are filled with
// whole pages, `pagesPerPart` at a time, so the reader always knows which
// global pages a file holds without looking at the others.
//
// Each volume's 64-byte header still carries partIndex / partCount / firstPage
// / totalPages, so page numbers and the progress bar stay global.

import {
  buildHeaderRecord,
  buildPageRecord,
  buildTextPdb,
  type TextPdbHeader,
} from './textPdb';
import type { Page } from './paginate';
import { phasePercent, type ProgressCallback } from './progress';

/** Progress bar rows written into every header (border + fill). */
export const PROGRESS_ROWS = 2;

/** Default pages per volume: 100 pages is about 70 KB at 8px. */
export const DEFAULT_PAGES_PER_PART = 100;

/** Selectable volume sizes in the web app, in pages. */
export const PAGES_PER_PART_OPTIONS = [100, 200, 500, 1000] as const;

export interface BuildOptions {
  displayName: string;
  cell: 8 | 12;
  charsPerLine: number;
  linesPerPage: number;
  /** Pages per volume. */
  pagesPerPart: number;
  /**
   * Optional byte cap per volume. Off by default: the 64 KB limit applies to a
   * single page record, not to the whole database, so a 100-page volume is
   * fine. Pages are never split.
   */
  maxPartBytes?: number;
  /** Called while page records and PDBs are built. */
  onProgress?: ProgressCallback;
}

export interface PartResult {
  fileName: string;
  dbName: string;
  bytes: Uint8Array;
  /** 0-based volume index. */
  partIndex: number;
  partCount: number;
  firstPage: number;
  pageCount: number;
  size: number;
  /** Unsupported characters replaced on this volume. */
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

/** File name = database name: LPB_<NAME>_<NN>. */
export function partName(displayName: string, index: number): string {
  return `LPB_${displayName}_${String(index + 1).padStart(2, '0')}`;
}

export function buildTextParts(pages: Page[], options: BuildOptions): BuildResult {
  const report = options.onProgress;
  const totalPages = pages.length;
  const pagesPerPart = Math.max(1, Math.floor(options.pagesPerPart));
  const maxBytes = options.maxPartBytes ?? Number.POSITIVE_INFINITY;

  // Encode every page once; this is the expensive half of the pack phase.
  const records: Array<{ bytes: Uint8Array; replaced: string[] }> = [];
  const pageReportEvery = Math.max(1, Math.floor(totalPages / 50));
  for (let i = 0; i < totalPages; i++) {
    records.push(buildPageRecord(pages[i]));
    if (report !== undefined && (i % pageReportEvery === 0 || i + 1 === totalPages)) {
      // The pack phase is split 50/50 between encoding and assembling.
      report({
        phase: 'pack',
        done: i + 1,
        total: totalPages * 2,
        percent: phasePercent('pack', i + 1, totalPages * 2),
      });
    }
  }

  // Every volume pays for the PDB header, the record-0 entry and the 64-byte
  // header record, so start the running size at that fixed cost.
  const PART_FIXED = PDB_OVERHEAD + ENTRY_SIZE + 64;
  const groups: number[][] = [];
  let current: number[] = [];
  let currentSize = PART_FIXED;

  records.forEach((rec, i) => {
    const entry = ENTRY_SIZE + rec.bytes.length;
    const full = current.length >= pagesPerPart;
    const tooBig = current.length > 0 && currentSize + entry > maxBytes;

    if (full || tooBig) {
      groups.push(current);
      current = [];
      currentSize = PART_FIXED;
    }
    current.push(i);
    currentSize += entry;
  });
  if (current.length > 0) groups.push(current);

  if (groups.length > 99) {
    const needed = Math.ceil(totalPages / 99);
    const suggestion =
      PAGES_PER_PART_OPTIONS.find((n) => n >= needed) ?? needed;
    throw new Error(
      `这本 ${totalPages} 页的书按每卷 ${pagesPerPart} 页需要 ${groups.length} 卷，` +
        `超过 99 卷上限。请把每卷页数调到 ${suggestion} 或更大。`,
    );
  }

  const parts: PartResult[] = [];
  let firstPage = 1;
  const replacedAll: string[] = [];
  const groupReportEvery = 1;

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
      partIndex,
      partCount: groups.length,
      firstPage,
      pageCount: indices.length,
      size: bytes.length,
      replaced,
    });
    firstPage += indices.length;

    if (report !== undefined && (partIndex % groupReportEvery === 0 ||
        partIndex + 1 === groups.length)) {
      report({
        phase: 'pack',
        done: totalPages + partIndex + 1,
        total: totalPages * 2,
        percent: phasePercent('pack', totalPages + partIndex + 1, totalPages * 2),
      });
    }
  });

  return {
    parts,
    totalPages,
    totalBytes: parts.reduce((n, p) => n + p.size, 0),
    replaced: [...new Set(replacedAll)],
  };
}

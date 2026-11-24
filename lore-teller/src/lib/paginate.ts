// Pagination: group wrapped lines into pages of a fixed line count.

export interface Page {
  /** Lines in display order; a blank string is an empty display line. */
  lines: string[];
}

/** Split display lines into pages. The last page may be partial. */
export function paginate(lines: string[], linesPerPage: number): Page[] {
  const pages: Page[] = [];
  for (let i = 0; i < lines.length; i += linesPerPage) {
    pages.push({ lines: lines.slice(i, i + linesPerPage) });
  }
  // An empty document still needs one page so the DB has a body record.
  if (pages.length === 0) pages.push({ lines: [] });
  return pages;
}

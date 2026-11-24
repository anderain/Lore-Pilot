// Palm OS record database (.pdb) writer/parser shared by the packers.
// Big-endian throughout. Records are addressed by absolute file offsets.

import fs from 'node:fs';

export const PDB_HEADER_SIZE = 78;
export const RECORD_ENTRY_SIZE = 8;
const PALM_EPOCH_MS = Date.UTC(1904, 0, 1, 0, 0, 0);

export class PdbError extends Error {
  constructor(code, message) {
    super(message);
    this.name = 'PdbError';
    this.code = code;
  }
}

export function palmSecondsNow() {
  return Math.max(0, Math.floor((Date.now() - PALM_EPOCH_MS) / 1000));
}

// opts: { name, type, creator, records: Buffer[], version? }
// returns { buffer, offsets }
export function buildPdb(opts) {
  const { name, type, creator, records } = opts;
  if (!Array.isArray(records) || records.length === 0) {
    throw new PdbError('NO_RECORDS', 'at least one record is required');
  }
  const numRecords = records.length;
  const listEnd = PDB_HEADER_SIZE + numRecords * RECORD_ENTRY_SIZE;

  const offsets = [];
  let cursor = listEnd;
  for (const rec of records) {
    offsets.push(cursor);
    cursor += rec.length;
  }
  const out = Buffer.alloc(cursor);
  const now = palmSecondsNow();

  out.write(name, 0, 'ascii');
  out.writeUInt16BE(0x0000, 32);          // attributes
  out.writeUInt16BE(opts.version ?? 1, 34);
  out.writeUInt32BE(now, 36);             // creation
  out.writeUInt32BE(now, 40);             // modification
  out.writeUInt32BE(0, 44);               // last backup
  out.writeUInt32BE(0, 48);               // modification number
  out.writeUInt32BE(0, 52);               // appInfoID
  out.writeUInt32BE(0, 56);               // sortInfoID
  out.write(type, 60, 4, 'ascii');
  out.write(creator, 64, 4, 'ascii');
  out.writeUInt32BE(numRecords + 1, 68);  // uniqueID seed
  out.writeUInt32BE(0, 72);               // nextRecordListID
  out.writeUInt16BE(numRecords, 76);

  for (let i = 0; i < numRecords; i++) {
    const base = PDB_HEADER_SIZE + i * RECORD_ENTRY_SIZE;
    out.writeUInt32BE(offsets[i], base);
    out[base + 4] = 0;                    // record attribute: normal
    out.writeUIntBE(i + 1, base + 5, 3);  // unique ID
  }

  for (let i = 0; i < numRecords; i++) {
    records[i].copy(out, offsets[i]);
  }

  return { buffer: out, offsets };
}

export function writePdb(file, opts) {
  const built = buildPdb(opts);
  fs.writeFileSync(file, built.buffer);
  return built;
}

export function parsePdb(buf) {
  if (buf.length < PDB_HEADER_SIZE) throw new PdbError('TRUNCATED', 'file is smaller than PDB header');
  const name = buf.toString('ascii', 0, 32).replace(/\0.*$/, '');
  const numRecords = buf.readUInt16BE(76);
  if (numRecords === 0) throw new PdbError('NO_RECORDS', 'PDB has no records');

  const listEnd = PDB_HEADER_SIZE + numRecords * RECORD_ENTRY_SIZE;
  if (listEnd > buf.length) throw new PdbError('BAD_LIST', 'record list runs past end of file');

  const entries = [];
  for (let i = 0; i < numRecords; i++) {
    const base = PDB_HEADER_SIZE + i * RECORD_ENTRY_SIZE;
    entries.push({
      offset: buf.readUInt32BE(base),
      attr: buf[base + 4],
      uid: buf.readUIntBE(base + 5, 3),
    });
  }

  for (let i = 0; i < entries.length; i++) {
    const e = entries[i];
    if (e.offset < listEnd) {
      throw new PdbError('BAD_OFFSET', `record ${i} offset ${e.offset} overlaps the record list (min ${listEnd})`);
    }
    if (e.offset >= buf.length) {
      if (i === entries.length - 1) {
        throw new PdbError('TRUNCATED', `last record starts at ${e.offset}, file ends at ${buf.length}`);
      }
      throw new PdbError('BAD_OFFSET', `record ${i} offset ${e.offset} is past EOF (${buf.length})`);
    }
    if (i > 0 && e.offset <= entries[i - 1].offset) {
      throw new PdbError('BAD_ORDER', `record ${i} offset ${e.offset} is not after record ${i - 1} offset ${entries[i - 1].offset}`);
    }
  }

  const records = entries.map((e, i) => {
    const next = i + 1 < entries.length ? entries[i + 1].offset : buf.length;
    return buf.subarray(e.offset, next);
  });

  return {
    name,
    attributes: buf.readUInt16BE(32),
    version: buf.readUInt16BE(34),
    type: buf.toString('ascii', 60, 64),
    creator: buf.toString('ascii', 64, 68),
    numRecords,
    entries,
    records,
    listEnd,
  };
}

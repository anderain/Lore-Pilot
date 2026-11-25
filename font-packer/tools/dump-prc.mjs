import fs from 'node:fs';

// Palm OS .prc audit used during development.
// Resource DB entries are 10 bytes: type[4], id[2], offset[4].
// For a code resource the data starts with: word id, ...; trap auditing only
// looks for the 68K `trap #$Axxx` opcode (4E4F xxxx).

const p = process.argv[2];
if (!p) {
  console.error('usage: node font-packer/tools/dump-prc.mjs <file.prc>');
  process.exit(1);
}

const b = fs.readFileSync(p);
const u16 = (o) => b.readUInt16BE(o);
const u32 = (o) => b.readUInt32BE(o);
const fourcc = (o) => b.toString('ascii', o, o + 4);

const name = b.toString('ascii', 0, 32).replace(/\0.*$/, '');
const type = fourcc(60);
const creator = fourcc(64);
const num = u16(76);

console.log(`file     ${p}`);
console.log(`size     ${b.length}`);
console.log(`name     ${JSON.stringify(name)}`);
console.log(`type     ${JSON.stringify(type)} creator ${JSON.stringify(creator)}`);
console.log(`version  ${u16(34)} records ${num}`);

if (type === 'data') {
  console.log('(data database; use the font-packer verifier)');
  process.exit(0);
}

const resources = [];
for (let i = 0; i < num; i++) {
  const e = 78 + i * 10;
  resources.push({ type: fourcc(e), id: u16(e + 4), offset: u32(e + 6), size: 0 });
}
for (let i = 0; i < resources.length; i++) {
  const next = i + 1 < resources.length ? resources[i + 1].offset : b.length;
  resources[i].size = next - resources[i].offset;
}

console.table(resources.map((r, i) => ({ i, type: r.type, id: r.id, offset: r.offset, size: r.size })));

// IDs are assigned by PilRC in declaration order; keep this list in sync with
// reader/Rsc/LorePilotReader_Rsc.rcp.
const required = [
  ['tAIN', 1000], ['tver', 1000], ['code', 0], ['data', 0],
  ['tAIB', 1000], ['tAIB', 1001], ['MBAR', 1008],
  ['Talt', 1001], ['Talt', 1002], ['Talt', 1003], ['Talt', 1004],
  ['Talt', 1005], ['Talt', 1006], ['Talt', 1007],
  ['tFRM', 1015], ['tFRM', 1018], ['tFRM', 1019], ['tFRM', 1020],
  ['tFRM', 1023],
];
let missing = 0;
for (const [t, id] of required) {
  const hit = resources.some((r) => r.type === t && r.id === id);
  console.log(`${hit ? 'OK  ' : 'MISS'} ${t} ${id}`);
  if (!hit) missing++;
}
console.log(missing ? `${missing} expected resource(s) missing` : 'all expected resources present');

// Trap audit. Palm OS 2.0 trap table ends at 0xA2B5 (2.x traps start at
// 0xA2B6); 3.0 starts at 0xA307. Only traps >= 0xA307 are a hard failure for
// 2.0. 0xA2B6..0xA306 are 2.0-only additions and are fine.
const BAD = 0xa307;
const hits = new Map();
for (const r of resources) {
  if (r.type !== 'code') continue;
  const end = r.offset + r.size;
  for (let i = r.offset; i + 4 <= end; i++) {
    if (b[i] === 0x4e && b[i + 1] === 0x4f) {
      const trap = u16(i + 2);
      // Real Palm OS traps all live in 0xA000..0xAFFF. Anything else (e.g.
      // 0xEF00 formed by a jsr displacement followed by a lea opcode) is a
      // byte-alignment false positive.
      if (trap < 0xa000 || trap > 0xafff) continue;
      const key = '0x' + trap.toString(16).toUpperCase().padStart(4, '0');
      if (!hits.has(key)) hits.set(key, []);
      hits.get(key).push(i);
    }
  }
}
const sorted = [...hits.entries()].sort((x, y) => parseInt(x[0], 16) - parseInt(y[0], 16));
const bad = sorted.filter(([k]) => parseInt(k, 16) >= BAD);
console.log(`\ntraps in code resources: ${sorted.length} distinct, ${sorted.reduce((n, [, v]) => n + v.length, 0)} total`);
for (const [trap, where] of sorted) {
  const n = parseInt(trap, 16);
  console.log(`  ${trap}  x${where.length}  ${n >= BAD ? 'UNSAFE for 2.0' : 'ok for 2.0'}  first@0x${where[0].toString(16)}`);
}
if (bad.length) {
  console.log(`\n${bad.length} trap(s) are not available on Palm OS 2.0`);
  process.exitCode = 2;
} else {
  console.log('\nall traps are available on Palm OS 2.0');
}

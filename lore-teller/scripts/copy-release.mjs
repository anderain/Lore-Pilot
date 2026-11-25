#!/usr/bin/env node
// Copy the release artefacts the site serves as downloads into public/.
//
//   1. font-packer/out/LPF08F.pdb, LPF12F.pdb -> public/fonts/
//   2. reader/Release/LorePilotReader.prc     -> public/
//
// Vite copies public/ verbatim into dist/, so `npm run build` ships the PRC and
// both preview fonts. Missing sources are only a warning: the site still builds,
// the preview just has no body glyphs.

import { copyFileSync, existsSync, mkdirSync, statSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..', '..');
const publicDir = join(root, 'lore-teller', 'public');

const files = [
  ['font-packer/out/LPF08F.pdb', join(publicDir, 'fonts', 'LPF08F.pdb')],
  ['font-packer/out/LPF12F.pdb', join(publicDir, 'fonts', 'LPF12F.pdb')],
  ['reader/Release/LorePilotReader.prc', join(publicDir, 'LorePilotReader.prc')],
];

let copied = 0;
let skipped = 0;

for (const [from, to] of files) {
  const src = join(root, from);
  if (!existsSync(src)) {
    console.warn(`skip ${from} (not built yet)`);
    skipped++;
    continue;
  }
  mkdirSync(dirname(to), { recursive: true });
  copyFileSync(src, to);
  const kb = Math.round(statSync(to).size / 1024);
  console.log(`copied ${from} -> ${to.slice(root.length + 1)} (${kb}K)`);
  copied++;
}

console.log(`${copied} copied, ${skipped} skipped`);

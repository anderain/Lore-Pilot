<script setup lang="ts">
import { computed, ref } from 'vue';
import FileDrop from './components/FileDrop.vue';
import OptionsForm, { type Options } from './components/OptionsForm.vue';
import PartList from './components/PartList.vue';
import PreviewPanel from './components/PreviewPanel.vue';
import { convertText, displayNameFromFile, type ConvertResult } from './lib/convert';
import { decodeAs, decodeText, type TextEncoding } from './lib/decode';
import type { PartResult } from './lib/split';

const fileInfo = ref<{ name: string; size: number; encoding: TextEncoding } | null>(null);
const buffer = ref<ArrayBuffer | null>(null);
const options = ref<Options>({
  displayName: 'BOOK',
  cell: 8,
  partLimit: 64 * 1024,
  encoding: 'utf-8',
});
const result = ref<ConvertResult | null>(null);
const previewPart = ref<PartResult | null>(null);
const busy = ref(false);
const error = ref('');

const canGenerate = computed(
  () => buffer.value !== null && /^[A-Za-z0-9]{1,16}$/.test(options.value.displayName) && !busy.value,
);

function onFile(payload: { file: File; buffer: ArrayBuffer }) {
  buffer.value = payload.buffer;
  const decoded = decodeText(payload.buffer);
  options.value = {
    ...options.value,
    displayName: displayNameFromFile(payload.file.name),
    encoding: decoded.encoding,
  };
  fileInfo.value = {
    name: payload.file.name,
    size: payload.buffer.byteLength,
    encoding: decoded.encoding,
  };
  result.value = null;
  previewPart.value = null;
  error.value = '';
}

function onEncoding(encoding: TextEncoding) {
  if (!buffer.value || !fileInfo.value) return;
  fileInfo.value = { ...fileInfo.value, encoding };
  options.value = { ...options.value, encoding };
  // Show what the chosen encoding yields, even before generating.
  void decodeAs(buffer.value, encoding);
}

async function generate() {
  if (!buffer.value) return;
  busy.value = true;
  error.value = '';
  try {
    const converted = convertText(buffer.value, {
      displayName: options.value.displayName,
      cell: options.value.cell,
      partLimit: options.value.partLimit,
      encoding: options.value.encoding,
    });
    result.value = converted;
    previewPart.value = converted.parts[0] ?? null;
  } catch (e) {
    error.value = e instanceof Error ? e.message : String(e);
    result.value = null;
    previewPart.value = null;
  } finally {
    busy.value = false;
  }
}
</script>

<template>
  <main class="mx-auto max-w-5xl space-y-6 px-4 py-8">
    <header>
      <h1 class="text-2xl font-semibold tracking-tight">lore-teller</h1>
      <p class="mt-1 text-sm text-stone-600">
        把 TXT 排版成 Palm OS 160×160 书页，打包为可分卷的 PDB。
      </p>
    </header>

    <FileDrop @file="onFile" />

    <OptionsForm
      v-model="options"
      :file-info="fileInfo"
      @encoding="onEncoding"
    />

    <div class="flex flex-wrap items-center gap-3">
      <button
        class="rounded-lg bg-stone-900 px-5 py-2.5 text-sm font-medium text-white hover:bg-stone-700 disabled:opacity-40"
        :disabled="!canGenerate"
        @click="generate"
      >
        {{ busy ? '生成中…' : '生成 PDB' }}
      </button>
      <p v-if="error" class="text-sm text-red-600">{{ error }}</p>
      <p v-else-if="result" class="text-sm text-stone-600">
        {{ result.charCount }} 字 · {{ result.lineCount }} 行 · {{ result.totalPages }} 页 ·
        {{ result.parts.length }} 个分卷 ·
        总 {{ (result.totalBytes / 1024).toFixed(1) }} KB
      </p>
    </div>

    <ul v-if="result && result.warnings.length" class="space-y-1 rounded-xl bg-amber-50 p-4 text-sm text-amber-800">
      <li v-for="warning in result.warnings" :key="warning">• {{ warning }}</li>
    </ul>

    <PartList
      v-if="result"
      :parts="result.parts"
      :total-pages="result.totalPages"
      @preview="(part) => (previewPart = part)"
    />

    <PreviewPanel
      v-if="result && previewPart"
      :result="result"
      :part="previewPart"
      :display-name="result.displayName"
    />

    <footer class="pt-4 text-xs text-stone-400">
      版面：160×160，顶部 8 行显示名称与页码，正文 {{ result?.charsPerLine ?? 20 }} 字/行，
      底部 PREV / MENU / NEXT。
    </footer>
  </main>
</template>

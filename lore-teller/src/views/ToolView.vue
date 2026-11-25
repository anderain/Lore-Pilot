<script setup lang="ts">
import { computed, onBeforeUnmount, ref, watch } from 'vue';
import FileDrop from '../components/FileDrop.vue';
import OptionsForm, { type Options } from '../components/OptionsForm.vue';
import PartList from '../components/PartList.vue';
import PreviewPanel from '../components/PreviewPanel.vue';
import Spinner from '../components/Spinner.vue';
import { displayNameFromFile, type ConvertResult } from '../lib/convert';
import { convertTextAsync } from '../lib/convertAsync';
import { decodeAs, decodeText, type TextEncoding } from '../lib/decode';
import { progressLabel, type ConvertProgress } from '../lib/progress';
import type { PartResult } from '../lib/split';

const STEP_NAMES = ['选择文件', '分卷设置', '预览与导出'] as const;

const step = ref(0);
const fileInfo = ref<{ name: string; size: number; encoding: TextEncoding } | null>(null);
const buffer = ref<ArrayBuffer | null>(null);
const options = ref<Options>({
  displayName: 'BOOK',
  cell: 8,
  pagesPerPart: 100,
  encoding: 'utf-8',
});
const result = ref<ConvertResult | null>(null);
const previewPart = ref<PartResult | null>(null);
const busy = ref(false);
const progress = ref<ConvertProgress | null>(null);
const error = ref('');
let abort: AbortController | null = null;

const canLeaveStep0 = computed(() => buffer.value !== null);
const nameOk = computed(() => /^[A-Za-z0-9]{1,16}$/.test(options.value.displayName));
const canLeaveStep1 = computed(() => nameOk.value);

const stepTitle = computed(() => `第 ${step.value + 1} 步 · ${STEP_NAMES[step.value]}`);
const progressText = computed(() => {
  const p = progress.value;
  if (p === null) return '准备中…';
  return `${progressLabel(p.phase)} ${p.percent}%`;
});

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

  abort?.abort();
  abort = new AbortController();

  busy.value = true;
  error.value = '';
  progress.value = null;
  result.value = null;
  previewPart.value = null;

  try {
    const converted = await convertTextAsync(buffer.value, {
      displayName: options.value.displayName,
      cell: options.value.cell,
      pagesPerPart: options.value.pagesPerPart,
      encoding: options.value.encoding,
      onProgress: (p) => {
        progress.value = p;
      },
      signal: abort.signal,
    });
    result.value = converted;
    previewPart.value = converted.parts[0] ?? null;
  } catch (e) {
    if (e instanceof DOMException && e.name === 'AbortError') return;
    error.value = e instanceof Error ? e.message : String(e);
  } finally {
    busy.value = false;
    progress.value = null;
  }
}

function goNext() {
  if (step.value === 0) {
    if (!canLeaveStep0.value) return;
    step.value = 1;
    return;
  }
  if (step.value === 1) {
    if (!canLeaveStep1.value) return;
    step.value = 2;
    // Entering step 3 starts the conversion right away.
    void generate();
  }
}

function goPrev() {
  if (step.value === 0) return;
  abort?.abort();
  step.value -= 1;
}

function regenerate() {
  void generate();
}

// Leaving the tool page must not leave a worker running.
watch(step, (value) => {
  if (value !== 2) abort?.abort();
});

onBeforeUnmount(() => abort?.abort());
</script>

<template>
  <main class="mx-auto max-w-5xl space-y-6 px-4 py-8">
    <header>
      <h1 class="text-2xl font-semibold tracking-tight">Lore-Teller 分卷工具</h1>
      <p class="mt-1 text-sm text-ink-soft">
        把 TXT 排版成 Palm OS 160×160 书页，切成可单独安装的卷 PDB。
      </p>
    </header>

    <!-- Step bar: previous | step name | next -->
    <div class="flex items-center gap-2 rounded-xl border border-line bg-card px-3 py-2 shadow-paper">
      <button
        class="rounded-lg border border-line px-3 py-1.5 text-sm text-ink-soft transition hover:border-ink-faint disabled:opacity-40"
        :disabled="step === 0 || busy"
        @click="goPrev"
      >
        ← 上一步
      </button>

      <div class="flex flex-1 items-center justify-center gap-2 text-sm font-medium text-ink">
        <Spinner v-if="busy" :size="14" tone="text-seal" />
        <span>{{ busy ? progressText : stepTitle }}</span>
      </div>

      <button
        v-if="step < 2"
        class="rounded-lg bg-seal px-3 py-1.5 text-sm font-medium text-white transition hover:bg-seal-hover disabled:opacity-40"
        :disabled="(step === 0 && !canLeaveStep0) || (step === 1 && !canLeaveStep1)"
        @click="goNext"
      >
        下一步 →
      </button>
      <button
        v-else
        class="rounded-lg bg-seal px-3 py-1.5 text-sm font-medium text-white transition hover:bg-seal-hover disabled:opacity-40"
        :disabled="busy"
        @click="regenerate"
      >
        重新生成
      </button>
    </div>

    <!-- Step 1 -->
    <section v-if="step === 0" class="space-y-4">
      <FileDrop :file-info="fileInfo" @file="onFile" />
      <p class="text-xs text-ink-faint">
        支持 UTF-8 / GBK。文件只在浏览器本地处理，不会上传。
      </p>
    </section>

    <!-- Step 2 -->
    <section v-else-if="step === 1" class="rounded-xl border border-line bg-card p-5 shadow-paper">
      <OptionsForm
        v-model="options"
        :encoding="options.encoding"
        @encoding="onEncoding"
      />
      <p v-if="!nameOk" class="mt-3 text-sm text-red-700">
        显示名称只能是 1–16 位字母或数字。
      </p>
    </section>

    <!-- Step 3 -->
    <section v-else class="space-y-5">
      <div v-if="busy" class="flex items-center gap-3 rounded-xl border border-line bg-card p-6 shadow-paper">
        <Spinner :size="20" tone="text-seal" />
        <div>
          <p class="text-sm font-medium text-ink">{{ progressText }}</p>
          <p class="mt-0.5 text-xs text-ink-faint">
            正在后台生成，请稍等……
          </p>
        </div>
      </div>

      <p v-if="error" class="rounded-xl bg-seal-soft p-4 text-sm text-seal">{{ error }}</p>

      <template v-if="result && !busy">
        <p class="text-sm text-ink-soft">
          {{ result.charCount }} 字 · {{ result.lineCount }} 行 · {{ result.totalPages }} 页 ·
          {{ result.parts.length }} 卷 · 总 {{ (result.totalBytes / 1024).toFixed(1) }} KB
        </p>

        <ul v-if="result.warnings.length" class="space-y-1 rounded-xl bg-seal-soft p-4 text-sm text-seal">
          <li v-for="warning in result.warnings" :key="warning">• {{ warning }}</li>
        </ul>

        <div class="grid grid-cols-1 items-stretch gap-6 md:grid-cols-2">
          <PartList
            :parts="result.parts"
            :total-pages="result.totalPages"
            @preview="(part) => (previewPart = part)"
          />

          <PreviewPanel
            v-if="previewPart"
            :result="result"
            :part="previewPart"
            :display-name="result.displayName"
          />
        </div>

        <footer class="pt-2 text-xs text-ink-faint">
          版面：160×160，顶部 8 行黑底显示名称与页码，正文 {{ result.charsPerLine }} 字/行、
          {{ result.linesPerPage }} 行/页并垂直居中，顶部底部反色，
          最底 2 行是进度条，底部 上一页 / 跳转 / 回书架 / 下一页。
        </footer>
      </template>
    </section>
  </main>
</template>

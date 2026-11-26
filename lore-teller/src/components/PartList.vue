<script setup lang="ts">
import { Download, Eye } from 'lucide-vue-next';
import type { PartResult } from '../lib/split';

defineProps<{
  parts: PartResult[];
  totalPages: number;
}>();

const emit = defineEmits<{
  (e: 'preview', part: PartResult): void;
}>();

function download(part: PartResult) {
  const blob = new Blob([new Uint8Array(part.bytes)], { type: 'application/octet-stream' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = part.fileName;
  a.click();
  URL.revokeObjectURL(url);
}

function downloadAll(parts: PartResult[]) {
  parts.forEach((part, i) => {
    setTimeout(() => download(part), i * 200);
  });
}
</script>

<template>
  <div class="flex h-full flex-col rounded-xl border border-line bg-card p-5 shadow-paper">
    <div class="mb-3 flex flex-wrap items-center justify-between gap-2">
      <h2 class="text-sm font-semibold uppercase tracking-wide text-ink-soft">
        卷（{{ parts.length }} 个，共 {{ totalPages }} 页）
      </h2>
      <button
        v-if="parts.length > 1"
        class="inline-flex items-center gap-1.5 rounded-lg border border-line px-3 py-1.5 text-sm hover:border-ink-faint"
        @click="downloadAll(parts)"
      >
        <Download :size="14" />
        下载全部
      </button>
    </div>

    <ul class="divide-y divide-line">
      <li v-for="part in parts" :key="part.fileName" class="py-2.5">
        <div class="font-mono text-sm text-ink">{{ part.fileName }}</div>
        <div class="mt-1 flex flex-wrap items-center gap-x-3 gap-y-1 text-xs text-ink-soft">
          <span class="rounded bg-paper-deep px-1.5 py-0.5 font-medium text-ink-soft">
            {{ part.partIndex + 1 }}/{{ part.partCount }} 卷
          </span>
          <span>第 {{ part.firstPage }}–{{ part.firstPage + part.pageCount - 1 }} 页</span>
          <span>{{ (part.size / 1024).toFixed(1) }} KB</span>
          <span v-if="part.replaced.length" class="text-seal">
            {{ part.replaced.length }} 个替代字符
          </span>
        </div>
        <div class="mt-1.5 flex gap-2">
          <button
            class="inline-flex items-center gap-1.5 rounded-lg border border-line px-3 py-1 text-sm hover:border-ink-faint"
            @click="emit('preview', part)"
          >
            <Eye :size="14" />
            预览
          </button>
          <button
            class="inline-flex items-center gap-1.5 rounded-lg bg-seal px-3 py-1 text-sm text-white transition hover:bg-seal-hover"
            @click="download(part)"
          >
            <Download :size="14" />
            下载
          </button>
        </div>
      </li>
    </ul>
  </div>
</template>

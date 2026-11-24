<script setup lang="ts">
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
  <div class="flex h-full flex-col rounded-xl bg-white p-5 shadow-sm">
    <div class="mb-3 flex flex-wrap items-center justify-between gap-2">
      <h2 class="text-sm font-semibold uppercase tracking-wide text-stone-500">
        卷（{{ parts.length }} 个，共 {{ totalPages }} 页）
      </h2>
      <button
        v-if="parts.length > 1"
        class="rounded-lg border border-stone-300 px-3 py-1.5 text-sm hover:border-stone-500"
        @click="downloadAll(parts)"
      >
        下载全部
      </button>
    </div>

    <ul class="divide-y divide-stone-100">
      <li v-for="part in parts" :key="part.fileName" class="py-2.5">
        <div class="font-mono text-sm text-stone-900">{{ part.fileName }}</div>
        <div class="mt-1 flex flex-wrap items-center gap-x-3 gap-y-1 text-xs text-stone-500">
          <span class="rounded bg-stone-100 px-1.5 py-0.5 font-medium text-stone-700">
            {{ part.partIndex + 1 }}/{{ part.partCount }} 卷
          </span>
          <span>第 {{ part.firstPage }}–{{ part.firstPage + part.pageCount - 1 }} 页</span>
          <span>{{ (part.size / 1024).toFixed(1) }} KB</span>
          <span v-if="part.replaced.length" class="text-amber-600">
            {{ part.replaced.length }} 个替代字符
          </span>
        </div>
        <div class="mt-1.5 flex gap-2">
          <button
            class="rounded-lg border border-stone-300 px-3 py-1 text-sm hover:border-stone-500"
            @click="emit('preview', part)"
          >
            预览
          </button>
          <button
            class="rounded-lg bg-stone-900 px-3 py-1 text-sm text-white hover:bg-stone-700"
            @click="download(part)"
          >
            下载
          </button>
        </div>
      </li>
    </ul>
  </div>
</template>

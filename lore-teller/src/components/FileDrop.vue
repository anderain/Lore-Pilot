<script setup lang="ts">
import { ref } from 'vue';

const props = defineProps<{
  fileInfo: { name: string; size: number } | null;
}>();

const emit = defineEmits<{
  (e: 'file', payload: { file: File; buffer: ArrayBuffer }): void;
}>();

const dragging = ref(false);
const error = ref('');

async function handleFiles(files: FileList | null) {
  error.value = '';
  const file = files?.[0];
  if (!file) return;
  if (!/\.txt$/i.test(file.name) && file.type !== 'text/plain') {
    error.value = '请选择 .txt 文本文件。';
    return;
  }
  const buffer = await file.arrayBuffer();
  emit('file', { file, buffer });
}

function onDrop(event: DragEvent) {
  dragging.value = false;
  void handleFiles(event.dataTransfer?.files ?? null);
}

function onPick(event: Event) {
  const input = event.target as HTMLInputElement;
  void handleFiles(input.files);
  input.value = '';
}

function formatSize(bytes: number): string {
  if (bytes >= 1024 * 1024) return `${(bytes / 1024 / 1024).toFixed(1)} MB`;
  return `${(bytes / 1024).toFixed(1)} KB`;
}
</script>

<template>
  <div>
    <div
      v-if="!props.fileInfo"
      class="rounded-xl border-2 border-dashed p-10 text-center transition"
      :class="dragging ? 'border-seal bg-seal-soft' : 'border-line bg-card'"
      @dragover.prevent="dragging = true"
      @dragleave.prevent="dragging = false"
      @drop.prevent="onDrop"
    >
      <p class="text-sm text-ink-soft">把 TXT 拖到这里，或者</p>
      <label class="mt-3 inline-block cursor-pointer rounded-lg bg-seal px-4 py-2 text-sm font-medium text-white transition hover:bg-seal-hover">
        选择 TXT 文件
        <input type="file" accept=".txt,text/plain" class="hidden" @change="onPick" />
      </label>
      <p v-if="error" class="mt-3 text-sm text-red-700">{{ error }}</p>
    </div>

    <div v-else class="rounded-xl border border-line bg-card p-5 shadow-paper">
      <div class="flex flex-wrap items-baseline justify-between gap-3">
        <div>
          <p class="font-medium text-ink">{{ props.fileInfo.name }}</p>
          <p class="mt-0.5 text-sm text-ink-soft">{{ formatSize(props.fileInfo.size) }}</p>
        </div>
        <label class="cursor-pointer rounded-lg border border-line px-3 py-1.5 text-sm text-ink-soft transition hover:border-ink-faint">
          重新选择
          <input type="file" accept=".txt,text/plain" class="hidden" @change="onPick" />
        </label>
      </div>
      <p v-if="error" class="mt-3 text-sm text-red-700">{{ error }}</p>
    </div>
  </div>
</template>

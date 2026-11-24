<script setup lang="ts">
import { computed, ref } from 'vue';

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
</script>

<template>
  <div
    class="rounded-xl border-2 border-dashed p-8 text-center transition"
    :class="dragging ? 'border-stone-900 bg-stone-200' : 'border-stone-300 bg-white'"
    @dragover.prevent="dragging = true"
    @dragleave.prevent="dragging = false"
    @drop.prevent="onDrop"
  >
    <p class="text-sm text-stone-600">把 TXT 拖到这里，或者</p>
    <label class="mt-3 inline-block cursor-pointer rounded-lg bg-stone-900 px-4 py-2 text-sm font-medium text-white hover:bg-stone-700">
      选择 TXT 文件
      <input type="file" accept=".txt,text/plain" class="hidden" @change="onPick" />
    </label>
    <p v-if="error" class="mt-3 text-sm text-red-600">{{ error }}</p>
  </div>
</template>

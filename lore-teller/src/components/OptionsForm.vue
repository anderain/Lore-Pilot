<script setup lang="ts">
import { computed } from 'vue';
import type { TextEncoding } from '../lib/decode';
import { PAGES_PER_PART_OPTIONS } from '../lib/split';

export interface Options {
  displayName: string;
  cell: 8 | 12;
  pagesPerPart: number;
  encoding: TextEncoding;
}

const PAGE_COUNTS = PAGES_PER_PART_OPTIONS;

const props = defineProps<{
  modelValue: Options;
  fileInfo: { name: string; size: number; encoding: TextEncoding } | null;
}>();

const emit = defineEmits<{
  (e: 'update:modelValue', value: Options): void;
  (e: 'encoding', value: TextEncoding): void;
}>();

const nameOk = computed(() => /^[A-Za-z0-9]{1,16}$/.test(props.modelValue.displayName));

function patch(patch: Partial<Options>) {
  emit('update:modelValue', { ...props.modelValue, ...patch });
}

function onNameInput(event: Event) {
  const raw = (event.target as HTMLInputElement).value;
  const cleaned = raw.replace(/[^A-Za-z0-9]/g, '').toUpperCase().slice(0, 16);
  patch({ displayName: cleaned });
}

const cells = [
  { label: '8px', value: 8 as const },
  { label: '12px', value: 12 as const },
];
</script>

<template>
  <div class="rounded-xl bg-white p-5 shadow-sm">
    <div v-if="fileInfo" class="mb-4 flex flex-wrap items-baseline gap-x-4 gap-y-1 text-sm text-stone-600">
      <span class="font-medium text-stone-900">{{ fileInfo.name }}</span>
      <span>{{ (fileInfo.size / 1024).toFixed(1) }} KB</span>
      <label class="flex items-center gap-1">
        编码
        <select
          class="rounded border border-stone-300 px-1 py-0.5"
          :value="fileInfo.encoding"
          @change="emit('encoding', ($event.target as HTMLSelectElement).value as TextEncoding)"
        >
          <option value="utf-8">UTF-8</option>
          <option value="gbk">GBK</option>
        </select>
      </label>
    </div>

    <div class="grid gap-4 sm:grid-cols-3">
      <label class="block">
        <span class="mb-1 block text-xs font-medium uppercase tracking-wide text-stone-500">Display name</span>
        <input
          :value="modelValue.displayName"
          maxlength="16"
          placeholder="MYBOOK"
          class="w-full rounded-lg border px-3 py-2 font-mono text-sm uppercase tracking-wider"
          :class="nameOk ? 'border-stone-300' : 'border-red-400'"
          @input="onNameInput"
        />
        <span class="mt-1 block text-xs" :class="nameOk ? 'text-stone-400' : 'text-red-600'">
          1–16 位字母或数字
        </span>
      </label>

      <div>
        <span class="mb-1 block text-xs font-medium uppercase tracking-wide text-stone-500">每卷页数</span>
        <select
          class="w-full rounded-lg border border-stone-300 px-3 py-2 text-sm"
          :value="modelValue.pagesPerPart"
          @change="patch({ pagesPerPart: Number(($event.target as HTMLSelectElement).value) })"
        >
          <option v-for="count in PAGE_COUNTS" :key="count" :value="count">
            {{ count }} 页 / 卷
          </option>
        </select>
        <span class="mt-1 block text-xs text-stone-400">每卷可单独装进 Palm</span>
      </div>

      <div>
        <span class="mb-1 block text-xs font-medium uppercase tracking-wide text-stone-500">字号</span>
        <div class="flex gap-2">
          <button
            v-for="cell in cells"
            :key="cell.value"
            class="flex-1 rounded-lg border px-2 py-2 text-sm"
            :class="modelValue.cell === cell.value
              ? 'border-stone-900 bg-stone-900 text-white'
              : 'border-stone-300 bg-white text-stone-700 hover:border-stone-500'"
            @click="patch({ cell: cell.value })"
          >
            {{ cell.label }}
          </button>
        </div>
      </div>
    </div>
  </div>
</template>

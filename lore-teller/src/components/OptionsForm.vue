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
  encoding: TextEncoding;
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

const pagesHint = computed(() => {
  // 8px: 20 chars x 17 lines; 12px: 13 x 11. A page is 680 / 286 bytes of text.
  const perVolume = props.modelValue.pagesPerPart;
  return `${perVolume} 页约 ${(perVolume * 0.7).toFixed(0)} KB（8px）/ ${(perVolume * 0.3).toFixed(0)} KB（12px）`;
});
</script>

<template>
  <div class="space-y-5">
    <div>
      <span class="mb-1 block text-xs font-medium uppercase tracking-wide text-ink-soft">每卷页数</span>
      <select
        class="w-full rounded-lg border border-line bg-card px-3 py-2 text-sm"
        :value="modelValue.pagesPerPart"
        @change="patch({ pagesPerPart: Number(($event.target as HTMLSelectElement).value) })"
      >
        <option v-for="count in PAGE_COUNTS" :key="count" :value="count">
          {{ count }} 页 / 卷
        </option>
      </select>
      <span class="mt-1 block text-xs text-ink-faint">
        决定每个 PDB 装多少页：页数越多，单卷越大、卷数越少。Palm 内存有限，100 页最稳妥。
      </span>
      <span class="mt-0.5 block text-xs text-ink-faint">当前：{{ pagesHint }}</span>
    </div>

    <label class="block">
      <span class="mb-1 block text-xs font-medium uppercase tracking-wide text-ink-soft">显示名称</span>
      <input
        :value="modelValue.displayName"
        maxlength="16"
        placeholder="MYBOOK"
        class="w-full rounded-lg border bg-card px-3 py-2 font-mono text-sm uppercase tracking-wider"
        :class="nameOk ? 'border-line' : 'border-red-400'"
        @input="onNameInput"
      />
      <span class="mt-1 block text-xs" :class="nameOk ? 'text-ink-faint' : 'text-red-700'">
        1–16 位字母或数字。显示在 Palm 书架的行首和阅读页顶部，文件名形如
        <code class="font-mono">LPB_名称_01.pdb</code>。
      </span>
    </label>

    <div>
      <span class="mb-1 block text-xs font-medium uppercase tracking-wide text-ink-soft">字号</span>
      <div class="flex gap-2">
        <button
          v-for="cell in cells"
          :key="cell.value"
          class="flex-1 rounded-lg border px-2 py-2 text-sm"
          :class="modelValue.cell === cell.value
            ? 'border-seal bg-seal text-white'
            : 'border-line bg-card text-ink-soft hover:border-ink-faint'"
          @click="patch({ cell: cell.value })"
        >
          {{ cell.label }}
        </button>
      </div>
      <span class="mt-1 block text-xs text-ink-faint">
        8px 每页 17 行、单卷更小；12px 更易读、每页 11 行、体积约为 2 倍。
      </span>
    </div>

    <div>
      <span class="mb-1 block text-xs font-medium uppercase tracking-wide text-ink-soft">文本编码</span>
      <select
        class="w-full rounded-lg border border-line bg-card px-3 py-2 text-sm"
        :value="encoding"
        @change="emit('encoding', ($event.target as HTMLSelectElement).value as TextEncoding)"
      >
        <option value="utf-8">UTF-8</option>
        <option value="gbk">GBK</option>
      </select>
      <span class="mt-1 block text-xs text-ink-faint">
        按源文件的编码解码。选错会提示解码失败，换另一个再试即可。
      </span>
    </div>
  </div>
</template>

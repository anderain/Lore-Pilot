<script setup lang="ts">
import { computed, onMounted, ref, watch } from 'vue';
import { ChevronLeft, ChevronRight } from 'lucide-vue-next';
import { paginate } from '../lib/paginate';
import { paintToCanvas, renderScreen } from '../lib/render';
import { parseGlyphFont, type GlyphFont } from '../lib/glyphFont';
import { buildPageRecord } from '../lib/textPdb';
import type { BuildResult, PartResult } from '../lib/split';
import type { ConvertResult } from '../lib/convert';

const props = defineProps<{
  result: ConvertResult;
  part: PartResult | null;
  displayName: string;
}>();

const canvas = ref<HTMLCanvasElement | null>(null);
const fonts = ref<Record<number, GlyphFont | null>>({ 8: null, 12: null });
const fontsLoading = ref(true);
const pageIndex = ref(0);

/** Internal render scale; CSS stretches the canvas to the card width. */
const SCALE = 4;

const pages = computed(() => {
  if (!props.part) return [];
  return decodePartPages(props.part);
});

const currentPage = computed(() => pages.value[pageIndex.value] ?? { lines: [] });

const totalPages = computed(() => props.result.totalPages);

const globalPage = computed(() => {
  if (!props.part) return 1;
  return props.part.firstPage + pageIndex.value;
});

function decodePartPages(part: PartResult) {
  const b = part.bytes;
  const numRecords = (b[76] << 8) | b[77];
  const offsets: number[] = [];
  for (let i = 0; i < numRecords; i++) {
    const o = 78 + i * 8;
    offsets.push(((b[o] << 24) | (b[o + 1] << 16) | (b[o + 2] << 8) | b[o + 3]) >>> 0);
  }
  const out: { lines: string[] }[] = [];
  for (let i = 1; i < offsets.length; i++) {
    const start = offsets[i];
    const end = i + 1 < offsets.length ? offsets[i + 1] : b.length;
    out.push(decodePageRecord(b.subarray(start, end)));
  }
  return out;
}

function decodePageRecord(rec: Uint8Array) {
  const be16 = (o: number) => (rec[o] << 8) | rec[o + 1];
  const lineCount = be16(0);
  const lineEnd: number[] = [];
  for (let i = 0; i < lineCount; i++) lineEnd.push(be16(4 + i * 2));
  const textStart = 4 + lineCount * 2 + 2;
  const text = rec.subarray(textStart);

  const lines: string[] = [];
  let prev = 0;
  for (const end of lineEnd) {
    lines.push(decodeGb(text.subarray(prev, end)));
    prev = end;
  }
  return { lines };
}

function decodeGb(bytes: Uint8Array): string {
  const decoder = new TextDecoder('gbk');
  return decoder.decode(bytes);
}

async function loadFonts() {
  fontsLoading.value = true;
  const load = async (file: string) => {
    try {
      const res = await fetch(`${import.meta.env.BASE_URL}fonts/${file}`);
      if (!res.ok) return null;
      const buf = await res.arrayBuffer();
      return parseGlyphFont(buf, file);
    } catch {
      return null;
    }
  };
  fonts.value[8] = await load('LPF08F.pdb');
  fonts.value[12] = await load('LPF12F.pdb');
  fontsLoading.value = false;
}

function draw() {
  if (!canvas.value || !props.part) return;
  const font = fonts.value[props.result.cell] ?? null;
  const bits = renderScreen(
    {
      displayName: props.displayName,
      page: currentPage.value,
      pageNumber: globalPage.value,
      totalPages: totalPages.value,
    },
    font,
    props.result.cell,
  );
  paintToCanvas(canvas.value, bits, SCALE);
}

function prev() {
  if (pageIndex.value > 0) pageIndex.value--;
}

function next() {
  if (pageIndex.value + 1 < pages.value.length) pageIndex.value++;
}

watch([currentPage, () => props.part, () => fonts.value[props.result.cell]], draw, { deep: true });
watch(() => props.part, () => { pageIndex.value = 0; });

onMounted(async () => {
  await loadFonts();
  draw();
});
</script>

<template>
  <div class="flex h-full flex-col rounded-xl border border-line bg-card p-5 shadow-paper">
    <div class="mb-3 flex items-center justify-between">
      <h2 class="text-sm font-semibold uppercase tracking-wide text-ink-soft">预览 160×160</h2>
      <span v-if="part" class="font-mono text-xs text-ink-soft">{{ part.fileName }}</span>
    </div>

    <p v-if="fontsLoading" class="mb-2 text-xs text-ink-soft">正在加载预览字库…</p>
    <p v-else-if="!fonts[result.cell]" class="mb-2 rounded bg-seal-soft p-2 text-xs text-seal">
      未找到 <code>public/fonts/LPF{{ result.cell === 8 ? '08' : '12' }}F.pdb</code>，预览无法显示正文。
      请按 README 生成并复制字体文件。
    </p>

    <canvas
      ref="canvas"
      class="pixelated mx-auto block h-auto w-full rounded border border-line bg-card"
    ></canvas>

    <div class="mt-3 flex items-center justify-between text-sm">
      <button
        class="inline-flex items-center gap-1 rounded-lg border border-line px-3 py-1 disabled:opacity-40"
        :disabled="pageIndex === 0"
        @click="prev"
      >
        <ChevronLeft :size="14" />
        上一页
      </button>
      <span class="font-mono text-xs text-ink-soft">
        {{ globalPage }} / {{ totalPages }}　本卷 {{ pageIndex + 1 }}/{{ pages.length }}
      </span>
      <button
        class="inline-flex items-center gap-1 rounded-lg border border-line px-3 py-1 disabled:opacity-40"
        :disabled="pageIndex + 1 >= pages.length"
        @click="next"
      >
        下一页
        <ChevronRight :size="14" />
      </button>
    </div>
  </div>
</template>

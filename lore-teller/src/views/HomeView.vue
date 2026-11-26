<script setup lang="ts">
import { RouterLink } from 'vue-router';
import { Scissors } from 'lucide-vue-next';
import logo from '../assets/lore-pilot-512.png';
import photo from '../assets/00-photo.webp';
import shotBookshelf from '../assets/01-bookshelf.png';
import shotReader from '../assets/02-reader.png';
import shotGoto from '../assets/03-goto.png';

const shots = [
  { src: photo, label: '实机', alt: 'Palm Pilot 上正在阅读中文' },
  { src: shotBookshelf, label: '书架', alt: '书架：卷列表与字体列表' },
  { src: shotReader, label: '阅读页', alt: '阅读页：正文与四段页脚' },
  { src: shotGoto, label: '跳转', alt: '跳页面板：数字键盘' },
];

const base = import.meta.env.BASE_URL;

interface InstallStep {
  no: number;
  title: string;
  body: string;
  links?: Array<{ label: string; href: string }>;
}

const installSteps: InstallStep[] = [
  {
    no: 1,
    title: '下载',
    body: '下载程序与字库：',
    links: [
      { label: '阅读器程序', href: `${base}LorePilotReader.prc` },
      { label: '8×8 字库', href: `${base}fonts/LPF08F.pdb` },
      { label: '12×12 字库', href: `${base}fonts/LPF12F.pdb` },
    ],
  },
  {
    no: 2,
    title: '安装字体',
    body: '把 LPF08F.pdb（8×8）或 LPF12F.pdb（12×12）同步到机器上。它们是中文点阵字库，没有字体时正文无法显示。',
  },
  {
    no: 3,
    title: '安装程序',
    body: '同步 LorePilotReader.prc，启动器里会出现 Lore-Pilot 图标。',
  },
  {
    no: 4,
    title: '切分书页并导入',
    body: '用分卷工具把 TXT 转成 LPB_<书名>_<卷号>.pdb，按需要只导入想读的那几卷。',
  },
];
</script>

<template>
  <main class="mx-auto max-w-5xl space-y-14 px-4 py-10">
    <!-- Hero: stacked with the logo on top on phones; from 640px up the copy
         sits on the left and the image is pushed to the right edge. The image
         comes first in the DOM so the mobile order needs no extra classes;
         `sm:order-last` moves it to the end of the row on desktop. -->
    <section class="flex flex-col items-start gap-6 sm:flex-row sm:items-center sm:justify-between sm:gap-8">
      <img
        :src="logo"
        alt="Lore-Pilot"
        width="512"
        height="512"
        class="w-28 shrink-0 sm:order-last sm:w-36"
      />

      <div class="space-y-3">
        <h1 class="text-4xl font-semibold tracking-tight">Lore-Pilot</h1>
        <p class="text-lg text-ink-soft">
          为 Palm OS 2.0 打造的中文阅读器实验项目
        </p>
        <p class="text-sm leading-relaxed text-ink-faint">
          目标在 160×160 单色屏、1 MB 内存的 Palm 上运行，更高版本也可用。
        </p>
        <div class="pt-2">
          <RouterLink
            to="/tool"
            class="inline-flex items-center gap-1.5 rounded-lg bg-seal px-5 py-2.5 text-sm font-medium text-white transition hover:bg-seal-hover"
          >
            <Scissors :size="16" />
            打开分卷工具
          </RouterLink>
        </div>
      </div>
    </section>

    <!-- Screenshots -->
    <section class="space-y-4">
      <h2 class="text-sm font-semibold uppercase tracking-wide text-ink-soft">截图</h2>
      <div class="grid grid-cols-2 gap-4 sm:grid-cols-4">
        <figure v-for="shot in shots" :key="shot.label" class="space-y-2">
          <img
            :src="shot.src"
            :alt="shot.alt"
            class="aspect-square w-full border border-line bg-card object-contain shadow-paper"
          />
          <figcaption class="text-center text-xs text-ink-faint">{{ shot.label }}</figcaption>
        </figure>
      </div>
    </section>

    <!-- Install -->
    <section class="space-y-4">
      <h2 class="text-sm font-semibold uppercase tracking-wide text-ink-soft">如何安装</h2>
      <ol class="grid gap-4 sm:grid-cols-2">
        <li
          v-for="step in installSteps"
          :key="step.no"
          class="rounded-xl border border-line bg-card p-5 shadow-paper"
        >
          <div class="flex items-center gap-3">
            <span class="grid h-7 w-7 shrink-0 place-items-center rounded-full bg-seal text-sm font-medium text-white">
              {{ step.no }}
            </span>
            <h3 class="text-sm font-semibold text-ink">{{ step.title }}</h3>
          </div>
          <p class="mt-3 text-sm leading-relaxed text-ink-soft">
            <span>{{ step.body }}</span>
            <span v-if="step.links">
              <template v-for="(link, index) in step.links" :key="link.href">
                <a
                  :href="link.href"
                  download
                  class="ml-1 text-sm text-seal underline decoration-seal/40 underline-offset-2 transition hover:text-seal-hover"
                >
                  {{ link.label }}
                </a>
                <span
                  v-if="index !== step.links.length - 1"
                  class="inline-block px-2"
                >|</span>
              </template>
            </span>
          </p>
        </li>
      </ol>
    </section>

    <!-- Usage -->
    <section class="space-y-4">
      <h2 class="text-sm font-semibold uppercase tracking-wide text-ink-soft">如何使用</h2>
      <div class="grid gap-4 sm:grid-cols-2">
        <article class="rounded-xl border border-line bg-card p-5 shadow-paper">
          <div class="flex items-center gap-3">
            <span class="grid h-7 w-7 shrink-0 place-items-center rounded-full bg-seal text-sm font-medium text-white">
              1
            </span>
            <h3 class="text-sm font-semibold text-ink">管理字体和卷</h3>
          </div>
          <p class="mt-3 text-sm leading-relaxed text-ink-soft">
            书架页上半是「书架」，每行一卷，显示卷号与页范围；下半是「字体」。
            选中后按「阅读」打开、「删除」移除。换过库之后，在菜单
            <code class="font-mono text-xs">Options → Refresh Lists</code> 刷新列表。
          </p>
        </article>

        <article class="rounded-xl border border-line bg-card p-5 shadow-paper">
          <div class="flex items-center gap-3">
            <span class="grid h-7 w-7 shrink-0 place-items-center rounded-full bg-seal text-sm font-medium text-white">
              2
            </span>
            <h3 class="text-sm font-semibold text-ink">阅读</h3>
          </div>
          <p class="mt-3 text-sm leading-relaxed text-ink-soft">
            页脚动作按钮：上一页 / 跳转 / 回书架 / 下一页；点「跳转」用数字键盘输入页码，
            硬件上下键也能翻页。到卷首卷尾会停住，换卷请回书架打开下一卷。
          </p>
        </article>
      </div>
    </section>

    <!-- CTA -->
    <section class="rounded-xl border border-line bg-paper-deep p-6">
      <div class="flex flex-wrap items-center justify-between gap-4">
        <p class="text-sm text-ink-soft">
          还差一步？用分卷工具把 TXT 变成书页 PDB。
        </p>
        <RouterLink
          to="/tool"
          class="inline-flex items-center gap-1.5 rounded-lg bg-seal px-5 py-2.5 text-sm font-medium text-white transition hover:bg-seal-hover"
        >
          <Scissors :size="16" />
          打开工具
        </RouterLink>
      </div>
    </section>
  </main>
</template>

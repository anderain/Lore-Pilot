<script setup lang="ts">
import { RouterLink, RouterView, useRoute } from 'vue-router';
import { computed } from 'vue';

const route = useRoute();

const nav = [
  { name: '首页', to: '/' },
  { name: '工具', to: '/tool' },
] as const;

const github = {
  name: 'Github',
  href: 'https://github.com/anderain/Lore-Pilot',
};

const currentPath = computed(() => route.path);
</script>

<template>
  <div class="min-h-screen bg-paper text-ink antialiased">
    <header class="border-b border-line">
      <div class="mx-auto flex max-w-5xl flex-wrap items-baseline justify-between gap-3 px-4 py-4">
        <RouterLink to="/" class="text-lg font-semibold tracking-tight text-ink hover:text-seal">
          Lore-Pilot
        </RouterLink>

        <nav class="flex items-center gap-1 text-sm">
          <RouterLink
            v-for="item in nav"
            :key="item.to"
            :to="item.to"
            class="rounded px-2.5 py-1 transition"
            :class="currentPath === item.to
              ? 'bg-seal-soft font-medium text-seal'
              : 'text-ink-soft hover:bg-paper-deep hover:text-ink'"
          >
            {{ item.name }}
          </RouterLink>
          <a
            :href="github.href"
            target="_blank"
            rel="noreferrer noopener"
            class="rounded px-2.5 py-1 text-ink-soft transition hover:bg-paper-deep hover:text-ink"
          >
            {{ github.name }}
          </a>
        </nav>
      </div>
    </header>

    <RouterView />
  </div>
</template>

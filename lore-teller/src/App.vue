<script setup lang="ts">
import { RouterLink, RouterView, useRoute } from 'vue-router';
import { computed } from 'vue';
import { Github, House, Wrench } from 'lucide-vue-next';
import logo from './assets/lore-pilot-256.png';

const route = useRoute();

const nav = [
  { name: '首页', to: '/', icon: House },
  { name: '工具', to: '/tool', icon: Wrench },
] as const;

const github = {
  name: 'Github',
  href: 'https://github.com/anderain/Lore-Pilot',
  icon: Github,
};

const currentPath = computed(() => route.path);
</script>

<template>
  <div class="min-h-screen bg-paper text-ink antialiased">
    <header class="border-b border-line">
      <div class="mx-auto flex max-w-5xl flex-wrap items-center justify-between gap-3 px-4 py-3">
        <RouterLink
          to="/"
          class="flex items-center gap-2 text-lg font-semibold tracking-tight text-ink transition hover:text-seal"
        >
          <img :src="logo" alt="" width="256" height="256" class="h-7 w-7" />
          Lore-Pilot
        </RouterLink>

        <nav class="flex items-center gap-1 text-sm">
          <RouterLink
            v-for="item in nav"
            :key="item.to"
            :to="item.to"
            class="inline-flex items-center gap-1.5 rounded px-2.5 py-1 transition"
            :class="currentPath === item.to
              ? 'bg-seal-soft font-medium text-seal'
              : 'text-ink-soft hover:bg-paper-deep hover:text-ink'"
          >
            <component :is="item.icon" :size="15" />
            {{ item.name }}
          </RouterLink>
          <a
            :href="github.href"
            target="_blank"
            rel="noreferrer noopener"
            class="inline-flex items-center gap-1.5 rounded px-2.5 py-1 text-ink-soft transition hover:bg-paper-deep hover:text-ink"
          >
            <component :is="github.icon" :size="15" />
            {{ github.name }}
          </a>
        </nav>
      </div>
    </header>

    <RouterView />
  </div>
</template>

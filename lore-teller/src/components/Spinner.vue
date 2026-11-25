<script setup lang="ts">
withDefaults(
  defineProps<{
    /** Pixel diameter. */
    size?: number;
    /** Extra classes, e.g. colour. */
    tone?: string;
  }>(),
  { size: 16, tone: 'text-seal' },
);
</script>

<template>
  <span
    class="inline-block animate-spin rounded-full border-2 border-current border-t-transparent align-[-2px]"
    :class="tone"
    :style="{ width: `${size}px`, height: `${size}px` }"
    role="status"
    aria-label="loading"
  ></span>
</template>

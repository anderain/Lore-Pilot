import { createRouter, createWebHashHistory } from 'vue-router';
import HomeView from '../views/HomeView.vue';
import ToolView from '../views/ToolView.vue';

export const router = createRouter({
  // Hash history keeps the site working from any static host with no SPA
  // rewrite rule: /#/ and /#/tool.
  history: createWebHashHistory(),
  routes: [
    { path: '/', name: 'home', component: HomeView, meta: { title: 'Home' } },
    { path: '/tool', name: 'tool', component: ToolView, meta: { title: 'Tool' } },
    { path: '/:pathMatch(.*)*', redirect: '/' },
  ],
});

export default router;

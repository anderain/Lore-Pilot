import { mount } from '@vue/test-utils';
import { createRouter, createMemoryHistory } from 'vue-router';
import { describe, expect, it } from 'vitest';
import App from '../src/App.vue';
import HomeView from '../src/views/HomeView.vue';
import ToolView from '../src/views/ToolView.vue';

function makeRouter() {
  return createRouter({
    history: createMemoryHistory(),
    routes: [
      { path: '/', name: 'home', component: HomeView },
      { path: '/tool', name: 'tool', component: ToolView },
    ],
  });
}

async function mountAt(path: string) {
  const router = makeRouter();
  await router.push(path);
  await router.isReady();
  const wrapper = mount(App, { global: { plugins: [router] } });
  await wrapper.vm.$nextTick();
  return wrapper;
}

describe('router shell', () => {
  it('shows the Lore-Teller header and both nav links', async () => {
    const wrapper = await mountAt('/');
    const text = wrapper.text();
    expect(text).toContain('Lore-Teller');
    expect(text).toContain('首页');
    expect(text).toContain('工具');
  });

  it('renders the converter on /tool', async () => {
    const wrapper = await mountAt('/tool');
    expect(wrapper.text()).toContain('生成 PDB');
  });

  it('leaves the home page empty for now', async () => {
    const wrapper = await mountAt('/');
    expect(wrapper.text()).not.toContain('生成 PDB');
    expect(wrapper.find('main').text()).toBe('');
  });
});

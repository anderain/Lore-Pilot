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

  it('opens the tool on step 1', async () => {
    const wrapper = await mountAt('/tool');
    const text = wrapper.text();
    expect(text).toContain('Lore-Teller 分卷工具');
    expect(text).toContain('第 1 步');
    expect(text).toContain('选择文件');
    // Step 1 is the file picker: no page-count control yet.
    expect(text).not.toContain('每卷页数');
  });

  it('describes the project on the home page', async () => {
    const wrapper = await mountAt('/');
    const text = wrapper.text();
    // Hero: project name, one-line summary, then the target hardware.
    expect(wrapper.find('h1').text()).toBe('Lore-Pilot');
    expect(text).toContain('为 Palm OS 2.0 打造的中文阅读器实验项目');
    expect(text).toContain('160×160 单色屏');
    // Screenshot captions and both entry points to the tool.
    for (const label of ['实机', '书架', '阅读页', '跳转']) {
      expect(text).toContain(label);
    }
    expect(text).toContain('如何安装');
    expect(text).toContain('如何使用');
    // Two entry points to the tool: hero button and the closing call to action.
    const toolLinks = wrapper.findAll('a').filter((a) => a.attributes('href')?.includes('/tool'));
    expect(toolLinks.length).toBeGreaterThanOrEqual(2);
  });
});

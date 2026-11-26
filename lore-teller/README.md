# Lore-Teller

把 TXT 排版成 Palm OS 160×160 的书页，切成可单独安装的 `LPBK` 卷 PDB。

- Vue 3 + TypeScript + Vite + Tailwind CSS
- 图标用 [Lucide](https://lucide.dev)（`lucide-vue-next`，ISC 协议，按需 tree-shake）
- 全部处理在浏览器本地完成，不上传文件
- 生成的文件用 Lore-Pilot 的阅读器读取

## 运行

```powershell
cd lore-teller
npm install
npm run dev        # http://localhost:5173/ (首页) / #/tool (分卷工具)
npm test           # 30 个单元测试
npm run build      # 产出 dist/
```

页面：`/#/` 是项目介绍与截图（`src/views/HomeView.vue`），`/#/tool` 是三步分卷工具
（`src/views/ToolView.vue`）。素材都在 `src/assets/`：四张截图（`00-photo.webp`、
`01-bookshelf.png`、`02-reader.png`、`03-goto.png`）和两版项目 logo
（`lore-pilot-256.png` 用于导航栏与 favicon，`lore-pilot-512.png` 用于首页 Hero）。
logo 是透明背景 PNG，直接叠在纸张底色上。

## 必需：准备预览字库

预览会读取 `font-packer` 生成的字体 PDB，站点的「下载」卡片还会把阅读器 PRC 一起
发布。**这些文件不入库**，用一条命令从仓库里拷进来：

```powershell
# 在仓库根目录
node font-packer/src/pack-gb2312.mjs --level full
node font-packer/src/pack-gb2312.mjs --level full --cell 12
cd lore-teller
npm run generate   # LPF08F/LPF12F -> public/fonts/，PRC -> public/
```

- `scripts/copy-release.mjs` 会拷三样：两个字体 PDB 和
  `reader/Release/LorePilotReader.prc`；缺失的源只警告，不中断构建；
- 缺少字体时，预览会提示 “未找到 public/fonts/LPF08F.pdb”，生成 PDB 仍然可用；
- 需要 8px 字体时用 `LPF08F.pdb`，12px 用 `LPF12F.pdb`；
- `public/fonts/.gitkeep` 保留目录，`*.pdb` 与 `public/*.prc` 已被 `.gitignore`
  忽略，它们只在构建时进入 `dist/`。

## 界面流程

工具页是一个三步向导（`/#/tool`）：

1. **第 1 步 · 选择文件** —— 拖入或选择 `.txt`；虚线框里注明支持 UTF-8 / GBK、
   文件只在浏览器本地处理；
2. **第 2 步 · 分卷设置** —— 每卷页数（100 / 200 / 500 / 1000）、显示名称
   （1–16 位字母数字）、字号（8px / 12px）、文本编码（UTF-8 / GBK），每项都有一行说明；
3. **第 3 步 · 预览与导出** —— 进入即自动生成；生成中显示旋转指示器与
   `解码 / 排版 / 打包 N%` 进度，完成后得到卷列表与 160×160 预览；
   可点「重新生成」或「上一步」改参数。

生成跑在 **Web Worker** 里（`src/workers/convert.worker.ts`），几十 MB 的书也不会
卡住界面；不支持 Worker 的环境（jsdom 测试）自动回退到主线程同步执行。

**卷是独立文件**：`LPB_<NAME>_<NN>.pdb` 可以单独装上 Palm、单独打开、单独删除，
不需要同一本书的其他卷。这正是为 1MB 内存设计的——机器上只需要放当前在读的那一卷。

## 排版规则

- 单个换行按原样换行，**不缩进**；空行保留为空行；
- 正文全部转全角（ASCII → U+FF01–FF5E，空格 → U+3000），保证每格一字；
- 8px：20 字/行 × **17 行/页**；12px：13 字/行 × **11 行/页**；
- 正文在剩余高度内垂直居中，不再紧贴上下 UI；
- 禁则：`，。、；：？！）》」』】…·` 不置行首，`（《「『【` 不置行尾；
- 不在 GB2312 内的字符替换为全角 `？` 并在界面列出警告。

## 版面

```
y 0..7     头部（黑底白字）：displayName + page/total（内置 6×8 ASCII）
y 8..149   正文（白底黑字，垂直居中）：8px 17 行 或 12px 11 行
y 150..157 底部（黑底白字）：上一页  跳转  回书架  下一页（内置 8×8 中文，四等分）
y 158      进度条边框（整行白线）
y 159      进度条填充（白色，长度 = 160 × page / total）
```

6×8 `FontHybird` 只用于头部的 ASCII 界面文字，不渲染正文；页脚中文来自
`reader/Src/UiGlyphs.c` 的内置字形表，由 `scripts/gen-ui-glyphs.mjs` 生成本仓库
的 `src/fonts/uiGlyphs.ts`，所以预览与设备一致。
上下两条黑带同时充当正文的视觉分隔，正文不再紧贴 UI。

## 目录

```
src/
  App.vue       站点头部 + 导航 + RouterView
  router/       vue-router（hash 模式：/#/ 与 /#/tool）
  views/        HomeView（暂时留空）、ToolView（三步向导）
  components/   FileDrop, OptionsForm, PartList, PreviewPanel, Spinner
  lib/          decode, gb2312, toFullwidth, layout, paginate,
                textPdb, split, glyphFont, render, convert,
                convertAsync（Worker 代理）、progress
  workers/      convert.worker.ts（在后台跑整条流水线）
  fonts/        hybrid6x8.ts、uiGlyphs.ts（均由脚本生成，入库）
scripts/
  gen-hybrid-font.mjs    从 reader/Src/FontHybird6x8.h 生成 6×8 表
  gen-ui-glyphs.mjs      从 reader/Src/UiGlyphs.c 生成内置中文字形表
  inspect-text-pdb.mjs   独立解析并打印页表
  make-sample.ts         无浏览器生成样例 PDB
  preview-bmp.ts         把某一页渲染成 BMP，便于人工检查
  bench-convert.ts       大文件转换耗时与阶段分布
  check-palm-reader.ts   逐像素比对 Palm 版面与网页预览
  check-screenbuf.ts     校验帧缓冲的字节位移与裁剪逻辑（fuzz）
  show-footer.ts         终端打印阅读页页脚
  show-shelf.ts          终端打印书架
  show-goto.ts           终端打印跳页面板
docs/text-pdb.md         格式说明（Palm 端实现依据）
```

`npm run gen:font` 会重新生成 `src/fonts/hybrid6x8.ts`（255 个字形，仅 ASCII
部分用于界面）。

## 命令行辅助

```powershell
# 生成样例（可选 cell 与分卷大小）
npx tsx scripts/make-sample.ts book.txt out 8 64
npx tsx scripts/make-sample.ts book.txt out 12 128

# 检查 PDB 结构
node scripts/inspect-text-pdb.mjs out/LPB_BOOK_01.pdb

# 渲染第 1 页为 4 倍 BMP
npx tsx scripts/preview-bmp.ts out/LPB_BOOK_01.pdb 0 page1.bmp

# 逐像素比对设备版面与网页预览
npx tsx scripts/check-palm-reader.ts out/LPB_BOOK_01.pdb public/fonts/LPF08F.pdb 0

# 校验 reader/Src/ScreenBuf.c 的 packed 像素算术
npx tsx scripts/check-screenbuf.ts
```

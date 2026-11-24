# lore-teller

把 TXT 排版成 Palm OS 160×160 的书页，并打包成可分卷的 `LPBK` PDB。

- Vue 3 + TypeScript + Vite + Tailwind CSS
- 全部处理在浏览器本地完成，不上传文件
- 生成的文件用 `lore-pilot` 的阅读器读取

## 运行

```powershell
cd lore-teller
npm install
npm run dev        # http://localhost:5173
npm test           # 18 个单元测试
npm run build      # 产出 dist/
```

## 必需：准备预览字库

预览会读取 `font-packer` 生成的字体 PDB。**这些文件不入库**，需要手动生成并复制：

```powershell
# 在仓库根目录
node font-packer/src/pack-gb2312.mjs --level full
node font-packer/src/pack-gb2312.mjs --level full --cell 12

# 复制到 web app 的静态目录
New-Item -ItemType Directory -Force lore-teller/public/fonts
Copy-Item font-packer\out\LPF08F.pdb lore-teller\public\fonts\
Copy-Item font-packer\out\LPF12F.pdb lore-teller\public\fonts\
```

- 缺少字体时，预览会提示 “未找到 public/fonts/LPF08F.pdb”，生成 PDB 仍然可用；
- 需要 8px 字体时用 `LPF08F.pdb`，12px 用 `LPF12F.pdb`；
- `public/fonts/.gitkeep` 保留目录，`*.pdb` 已被 `.gitignore` 忽略。

## 界面流程

1. 拖入或选择 `.txt`（UTF-8 / GBK 自动检测，可手动切换）；
2. 选择单文件大小（64K / 128K / 256K）、字号（8px / 12px）、display name（1–16 位字母数字）；
3. 点“生成 PDB”，得到分卷列表；
4. 每个分卷可以预览（160×160，4× 放大）或下载；多分卷时可“下载全部”。

## 排版规则

- 单个换行按原样换行，**不缩进**；空行保留为空行；
- 正文全部转全角（ASCII → U+FF01–FF5E，空格 → U+3000），保证每格一字；
- 8px：20 字/行 × 18 行/页；12px：13 字/行 × 12 行/页；
- 禁则：`，。、；：？！）》」』】…·` 不置行首，`（《「『【` 不置行尾；
- 不在 GB2312 内的字符替换为全角 `？` 并在界面列出警告。

## 版面

```
y 0..7     头部：左侧 displayName，右侧 page/total（6×8 ASCII）
y 8..151   正文：8px 18 行 或 12px 12 行
y 152..159 底部：PREV  MENU  NEXT（6×8 ASCII）
```

6×8 `FontHybird` 只用于头部和底部的 ASCII 界面文字，不渲染正文。

## 目录

```
src/
  components/   FileDrop, OptionsForm, PartList, PreviewPanel
  lib/          decode, gb2312, toFullwidth, layout, paginate,
                textPdb, split, glyphFont, render, convert
  fonts/        hybrid6x8.ts（由脚本生成，入库）
scripts/
  gen-hybrid-font.mjs    从 reader/Src/FontHybird6x8.h 生成 6×8 表
  inspect-text-pdb.mjs   独立解析并打印页表
  make-sample.ts         无浏览器生成样例 PDB
  preview-bmp.ts         把某一页渲染成 BMP，便于人工检查
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
```

# lore-teller

把 TXT 排版成 Palm OS 160×160 的书页，切成可单独安装的 `LPBK` 卷 PDB。

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
2. 选择每卷页数（50 / 100 / 200，默认 100）、字号（8px / 12px）、display name（1–16 位字母数字）；
3. 点“生成 PDB”，得到卷列表；
4. 每卷可以预览（160×160，4× 放大）或下载；多卷时可“下载全部”。

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
y 0..7     头部（黑底白字）：displayName + page/total（6×8 ASCII）
y 8..149   正文（白底黑字，垂直居中）：8px 17 行 或 12px 11 行
y 150..157 底部（黑底白字）：PREV  MENU  NEXT（6×8 ASCII）
y 158      进度条边框（整行白线）
y 159      进度条填充（白色，长度 = 160 × page / total）
```

6×8 `FontHybird` 只用于头部和底部的 ASCII 界面文字，不渲染正文。
上下两条黑带同时充当正文的视觉分隔，正文不再紧贴 UI。

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

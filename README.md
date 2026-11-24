# lore-pilot

中文阅读器可行性验证，目标设备是 **Palm OS 2.0 / Pilot Professional**
（160×160 单色、1MB RAM、16MHz 68000）。仓库里包含三部分：

- `reader/` — CodeWarrior for Palm OS 9.x 工程，产出 `LorePilotReader.prc`；
- `font-packer/` — Node 18+ 零依赖脚本，把点阵字模打包成 Palm OS PDB；
- 生成的头文件（`reader/Src/GlyphAtlas.h`、`GbSamples.h`、`GbSamples12.h`、
  `GbText.h`）已入库，保证没有字体的人也能直接编译。

## 前置条件

1. **CodeWarrior for Palm OS 9.x**（含 Palm OS 5 SDK 68K R2）。工程按 Palm OS 2.0
   最小版本、68000 CPU、单段、无浮点配置。
2. **Node 18+**（只用标准库，无需 `npm install`）。
3. **Fusion Pixel 8px / 12px Mono zh-Hans 字体**（SIL OFL 1.1）。

### 字体获取

字体**不在本仓库分发**。从上游下载：

<https://github.com/TakWolf/fusion-pixel-font/releases>

需要两个文件：

- `fusion-pixel-8px-monospaced-zh_hans.ttf`（800 units/em）用于 8×8；
- `fusion-pixel-12px-monospaced-zh_hans.ttf`（1200 units/em）用于 12×12。

放到仓库根目录，或打包时用 `--font <path>` 指定。字号与网格不匹配时脚本会报错。

旁路：如果只想重编 PRC、不改字模，`reader/Src/` 里已入库的四个生成头文件足够，
不需要字体。

## 构建

```powershell
# 8x8 图形 + 诗句示例，产出 LPT08A.pdb 与 GlyphAtlas.h
node font-packer/src/pack.mjs

# GB2312 8x8：常用 LPF08B.pdb / 全量 LPF08F.pdb
node font-packer/src/pack-gb2312.mjs --level basic
node font-packer/src/pack-gb2312.mjs --level full

# GB2312 12x12：常用 LPF12B.pdb / 全量 LPF12F.pdb
node font-packer/src/pack-gb2312.mjs --level basic --cell 12
node font-packer/src/pack-gb2312.mjs --level full  --cell 12
```

然后在 CodeWarrior 里 Build Release，得到
`reader/Release/LorePilotReader.prc`，把 PRC 与需要的 `.pdb` 一起装进
模拟器或真机。

### PDB 命名

统一规则：**`LPF<字号><级别>`**，文件名与 Palm 数据库名一致。

| 名字 | 字号 | 范围 | 说明 |
|---|---|---|---|
| `LPT08A` | 8×8 | — | 图形/诗句测试库（layout 0，不出现在字体列表） |
| `LPF08B` | 8×8 | A1–D7 | 常用字 + 中文标点 |
| `LPF08F` | 8×8 | A1–F7 | 加二级汉字 |
| `LPF08H` | 8×8 | B0–D7 | 纯汉字，无标点 |
| `LPF12B` | 12×12 | A1–D7 | 常用字 + 中文标点 |
| `LPF12F` | 12×12 | A1–F7 | 加二级汉字 |
| `LPF12H` | 12×12 | B0–D7 | 纯汉字，无标点 |

字号补零到两位，列表里按名字排序即按字号排序（`LPF08B`、`LPF08F`、`LPF08H`、
`LPF12B`、`LPF12F`、`LPF12H`）。`LPT08A` 是 layout-0 的图形测试库，不参与字体
列表排序。旧版本的 `LPGlyph8`、`LPGB2312B/F`、`LPGB12B/F` 仍能被识别；升级后
可先在书架菜单 `Options → Refresh Lists`，再用字体列表的 `Del` 删除旧库腾出空间。

详细的数据格式、校验规则和故障排查见 `font-packer/README.md`。

## 设备端验证

主界面标题为 **Bookshelf**，上下两块：

- **Books** —— 扫描 `LPBK`/`LPRD` 书籍数据库（`lore-teller` 生成的
  `LPB_<NAME>_<NN>`），**每一卷占一行**：`BOOKA  2/7  101-200`，行内显示卷号、
  本卷全局页范围。`Read` 只打开选中的这一卷，`Del` 也只删除这一卷，不需要同一本
  书的其他卷在场。
- **Fonts** —— 扫描 `GLYF`/`LPRD` 字体数据库，显示名称、字号与体积
  （如 `LPF12F 12x12 192K`），`Del` 删除选中的字体。

阅读页按 web 预览的版面绘制：

```
y 0..7      黑底白字：书名 + 全局页码/总页数
y 8..149    白底正文，按剩余高度垂直居中（8px 17 行 / 12px 11 行）
y 150..157  黑底白字：PREV  MENU  NEXT
y 158       进度条边框（整行白线）
y 159       进度条填充（长度 = 160 × 页码 / 总页数）
```

底部 148..159 的触控区横向三等分：左侧上一页、中间回书架、右侧下一页；
设备上/下硬件键同样是上一页/下一页。**翻页只在本卷内进行**：到卷首/卷尾就停住，
不会去打开其他卷。页脚的页码与进度条仍按全书计算（如 `201/300`），所以读者知道
自己在整本书的什么位置。

所有测试入口在菜单里：

- **Test → Glyph 8x8** — 21 个 8×8 字形（11 个手绘图形 + 10 个汉字），逐字节
  比对 PDB 与内置答案，1×/2× 对照，另有奇数偏移与裁切测试；
- **Test → GB2312 8x8** — 《千字文》1250 单元（1000 汉字 + 250 标点）、32 页、
  每页 40 单元，`Prev`/`Next` 翻页，1× 与 2× 同屏；
- **Test → GB2312 12x12** — 同样的 32 页千字文，12×12 字体，1× 13 字/行、
  2× 6 字/行；
- **Options → Refresh Lists / About**。

状态行会显示数据库名、字号、字形数、页数、抽样比对结果与当前页范围；缺库时
提示 `NO FONT 12x12` 而不崩溃。

## 目录结构

```
font-packer/
  data/qianziwen.txt      千字文源文本（UTF-8）
  src/pack.mjs            8x8 图形 + 诗句打包
  src/pack-gb2312.mjs     GB2312 字库打包（--cell 8|12）
  src/ttf.mjs             零依赖 TrueType 读取 / 像素栅格化
  src/pdb.mjs             Palm OS PDB 读写
  tools/dump-prc.mjs      PRC 资源表与 2.0 trap 审计
  out/                    生成物（已 .gitignore）
reader/
  Rsc/                    PilRC 资源、图标位图、生成的资源 ID 头
  Src/                    应用源码、字体列表与生成的字模头
  LorePilotReader.mcp     CodeWarrior 工程
lore-teller/              Vue 3 网页：TXT → 卷书页 PDB
  src/lib/                解码、GB2312、断行、分页、PDB 写入、渲染
  docs/text-pdb.md        LPBK 格式（Palm 端实现依据）
  public/fonts/           预览字库（不入库，见 lore-teller/README）
```

## 书页 PDB（lore-teller）

`lore-teller` 把 TXT 排版成 160×160 书页，按每卷固定页数输出
`LPB_<NAME>_<NN>.pdb`（type `LPBK`，creator `LPRD`）。每页一条记录，行偏移表在
记录内，翻页即记录跳转；卷头带 `partIndex / partCount / firstPage / totalPages`，
所以页码与进度条按全书连续。**每一卷都是独立文件**：可以只装一卷、只读一卷，
1MB 的机器上不需要同时放整本书。

```powershell
cd lore-teller
npm install
node ../font-packer/src/pack-gb2312.mjs --level full
node ../font-packer/src/pack-gb2312.mjs --level full --cell 12
Copy-Item ..\font-packer\out\LPF08F.pdb public\fonts\
Copy-Item ..\font-packer\out\LPF12F.pdb public\fonts\
npm run dev
```

格式与字段见 `lore-teller/docs/text-pdb.md`。Palm 端读取书页 PDB 是下一步。

## 已知限制

- 8×8 中文在 1× 下偏小，2× 是主要阅读视图；12×12 的 1× 已经清楚可读；
- 一级字库（`basic`）不覆盖二级汉字，打包脚本会把文本里超出范围的字放进
  “extras” 记录，阅读器仍能显示，但单库不再“完整”；
- 目标设备没有 Unicode，正文按 GB2312/GBK 双字节存储；
- 字体列表只识别 `GLYF`/`LPRD` 数据库；其它应用的数据不会被列出或删除；
- 删除字体只删数据库本身，不会动 PRC。

## 许可

本仓库代码未声明许可证。第三方字体 Fusion Pixel 使用 SIL OFL 1.1，版权归
其作者 TakWolf，不在本仓库分发；使用时请遵守上游许可证。

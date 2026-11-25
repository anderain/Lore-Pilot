# Lore-Pilot

An experimental Chinese reader for **Palm OS 2.0** (Pilot Professional: 160x160
monochrome, 1 MB RAM, 16 MHz 68000). Higher Palm OS versions run it too. Text is
laid out into 160x160 pages and split into standalone volumes, so a 1 MB device
only ever holds the volume being read. The whole UI is drawn from a built-in 8x8
glyph table, so the reader works without any installed font.

中文阅读器可行性验证，目标设备是 **Palm OS 2.0 / Pilot Professional**
（160×160 单色、1MB RAM、16MHz 68000）。仓库里包含三部分：

- `reader/` — CodeWarrior for Palm OS 9.x 工程，产出 `LorePilotReader.prc`；
- `font-packer/` — Node 18+ 零依赖脚本，把点阵字模打包成 Palm OS PDB；
- 生成的头文件（`reader/Src/GlyphAtlas.h`、`GbSamples.h`、`GbSamples12.h`、
  `GbText.h`）已入库，保证没有字体的人也能直接编译。

## 快速开始

### 安装

1. **安装字体** —— 把 `LPF08F.pdb`（8×8）或 `LPF12F.pdb`（12×12）同步到机器上。
   它们是中文点阵字库，没有字体时正文无法显示。
2. **安装程序** —— 同步 `LorePilotReader.prc`，启动器里会出现 Lore-Pilot 图标。
3. **切分书页并导入** —— 用 [Lore-Teller](#书页-pdblore-teller) 把 TXT 转成
   `LPB_<书名>_<卷号>.pdb`，按需要只导入想读的那几卷。

### 使用

- **管理字体和卷** —— 书架页上半是「书架」（每行一卷，显示卷号与页范围），
  下半是「字体」。选中后按「阅读」打开、「删除」移除。换过库之后，在菜单
  `Options → Refresh Lists` 刷新列表。
- **阅读** —— 页脚四等分：上一页 / 跳转 / 回书架 / 下一页。点「跳转」用数字键盘
  输入页码，硬件上下键也能翻页。到卷首卷尾会停住，换卷请回书架打开下一卷。

更细的版面、触控与测试菜单说明见 [设备端细节](#设备端细节)。

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

## 设备端细节

主界面标题为 **Bookshelf**，上下两块：

- **书架** —— 扫描 `LPBK`/`LPRD` 书籍数据库（Lore-Teller 生成的
  `LPB_<NAME>_<NN>`），**每一卷占一行**：`BOOKA  2/7  101-200`，行内显示卷号、
  本卷全局页范围。`阅读` 只打开选中的这一卷，`删除` 也只删除这一卷，不需要同一本
  书的其他卷在场。
- **字体** —— 扫描 `GLYF`/`LPRD` 字体数据库，显示名称、字号与体积
  （如 `LPF12F 12x12 192K`），`删除` 删除选中的字体。

书架的两个标签（`书架` / `字体`）和三个按钮（`阅读` / `删除`）都是**自绘**的，
字形来自 `reader/Src/UiGlyphs.c` 内置的 8×8 表，所以不装任何字库也能显示中文；
按钮保留按下反色与禁用灰显。列表行里的书名是 PDB 头里的 ASCII 名称，仍按 ASCII 显示。

阅读页按 web 预览的版面绘制：

```
y 0..7      黑底白字：书名 + 全局页码/总页数（内置 6×8 ASCII）
y 8..149    白底正文，按剩余高度垂直居中（8px 17 行 / 12px 11 行）
y 150..157  黑底白字，四等分列：上一页 跳转 回书架 下一页（内置 8×8 中文）
y 158       进度条边框（整行白线）
y 159       进度条填充（长度 = 160 × 页码 / 总页数）
```

页脚四个按钮用的是 `reader/Src/UiGlyphs.c` 里内置的 8×8 中文字形，
**不依赖任何已安装字库**；`lore-teller/scripts/gen-ui-glyphs.mjs` 从该 .c 生成
web 预览用的同一份数据，两边不会漂移。

底部触控区横向四等分（每段 40px）：上一页 / 跳转 / 回书架 / 下一页。
「跳转」打开一个**全自绘的数字键盘面板**：顶部输入框显示已输入数字与本卷可跳
范围（如 `599-999`），下面是 `7 8 9 / 4 5 6 / 1 2 3 / 0 <-` 与「跳转」「取消」。
只接受本卷范围内的全局页码，空输入或越界**静默返回阅读页**，不弹提示。

> 面板不使用 `FIELD` 资源：PilRC 生成的字段属性位域与 Palm OS 2.0 读取的
> `FormFieldType` 不一致，含 `FIELD` 的 form 会被 ROM 误解对象表（模拟器会报
> 对象跑到屏幕外、按钮消失）。自绘全套控件绕开了这个工具链问题。

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
lore-teller/              Vue 3 网页（Lore-Teller）：TXT → 卷书页 PDB
  src/views/              HomeView（暂空）、ToolView（分割器）
  src/router/             vue-router，hash 模式（/#/ 与 /#/tool）
  src/lib/                解码、GB2312、断行、分页、PDB 写入、渲染
  docs/text-pdb.md        LPBK 格式（Palm 端实现依据）
  public/fonts/           预览字库（不入库，见 Lore-Teller README）
```

## 书页 PDB（Lore-Teller）

Lore-Teller 把 TXT 排版成 160×160 书页，按每卷固定页数输出
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
npm run dev        # 首页 /#/ ，分割器 /#/tool
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

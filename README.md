# lore-pilot

中文阅读器可行性验证，目标设备是 **Palm OS 2.0 / Pilot Professional**
（160×160 单色、1MB RAM、16MHz 68000）。仓库里包含三部分：

- `reader/` — CodeWarrior for Palm OS 9.x 工程，产出 `LorePilotReader.prc`；
- `font-packer/` — Node 18+ 零依赖脚本，把点阵字模打包成 Palm OS PDB；
- 生成的头文件（`reader/Src/GlyphAtlas.h`、`GbSamples.h`、`GbText.h`）已入库，
  保证没有字体的人也能直接编译。

## 前置条件

1. **CodeWarrior for Palm OS 9.x**（含 Palm OS 5 SDK 68K R2）。工程按 Palm OS 2.0
   最小版本、68000 CPU、单段、无浮点配置。
2. **Node 18+**（只用标准库，无需 `npm install`）。
3. **Fusion Pixel 8px Mono zh-Hans 字体**（SIL OFL 1.1）。

### 字体获取

字体**不在本仓库分发**。从上游下载：

<https://github.com/TakWolf/fusion-pixel-font/releases>

取 `fusion-pixel-8px-monospaced-zh_hans.ttf`，放到仓库根目录，或打包时用
`--font <path>` 指定。文件名、字号（800 units/em）不对时脚本会报错。

旁路：如果只想重编 PRC、不改字模，`reader/Src/` 里已入库的三个生成头文件足够，
不需要字体。

## 构建

```powershell
# 8x8 图形 + 诗句示例，产出 LPGlyph8.pdb 与 GlyphAtlas.h
node font-packer/src/pack.mjs

# GB2312 常用字库 A1-D7，产出 LPGB2312B.pdb
node font-packer/src/pack-gb2312.mjs --level basic

# GB2312 全字库 A1-F7，产出 LPGB2312F.pdb
node font-packer/src/pack-gb2312.mjs --level full
```

然后在 CodeWarrior 里 Build Release，得到
`reader/Release/LorePilotReader.prc`，把 PRC 与需要的 `.pdb` 一起装进
模拟器或真机。

详细的数据格式、校验规则和故障排查见 `font-packer/README.md`。

## 设备端验证

主界面三个按钮：

- **Glyph Test** — 21 个 8×8 字形（11 个手绘图形 + 10 个汉字），逐字节比对
  PDB 与内置答案，1×/2× 对照，另有奇数偏移与裁切测试；
- **GB Test** — 《千字文》1250 单元（1000 汉字 + 250 标点）、32 页、每页 40 单元，
  `Prev`/`Next` 翻页，1× 与 2× 同屏，点字显示 GB 码与 8 字节；
- **About** — 版本信息。

状态行会显示数据库名、字形数、页数、抽样比对结果与当前页范围；缺库时提示
`NO DB` 而不崩溃。

## 目录结构

```
font-packer/
  data/qianziwen.txt      千字文源文本（UTF-8）
  src/pack.mjs            8x8 图形 + 诗句打包
  src/pack-gb2312.mjs     GB2312 两级字库打包
  src/ttf.mjs             零依赖 TrueType 读取 / 8px 栅格化
  src/pdb.mjs             Palm OS PDB 读写
  tools/dump-prc.mjs      PRC 资源表与 2.0 trap 审计
  out/                    生成物（已 .gitignore）
reader/
  Rsc/                    PilRC 资源、图标位图、生成的资源 ID 头
  Src/                    应用源码与生成的字模头
  LorePilotReader.mcp     CodeWarrior 工程
```

## 已知限制

- 8×8 中文在 1× 下偏小，2× 是主要阅读视图；
- 一级字库（`basic`）不覆盖二级汉字，打包脚本会把文本里超出范围的字放进
  “extras” 记录，阅读器仍能显示，但单库不再“完整”；
- 目标设备没有 Unicode，正文按 GB2312/GBK 双字节存储；
- 12×12 字库是下一阶段，PDB 头与 `GlyphFont` 接口已预留字段。

## 许可

本仓库代码未声明许可证。第三方字体 Fusion Pixel 使用 SIL OFL 1.1，版权归
其作者 TakWolf，不在本仓库分发；使用时请遵守上游许可证。

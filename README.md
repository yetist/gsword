# libsword

基于 C / GObject 重新实现的 SWORD 模块库，同时包含基于该库的 GTK4 桌面应用
`gsword`。

## 目录结构

- `lib/`：`libgsword` 库实现
- `include/gsword/`：`libgsword` 公共 API
- `src/`：`gsword` GTK4 桌面应用和可复用的书卷/章/节引用选择器
- `data/locales.d/`：libgsword 自有 locale 数据
- `tools/`：locale 导入、校验和 v11n 生成工具
- `examples/`：库示例和 GTK4 前端原型
- `tests/`：库测试、引用选择器测试和 demo
- `docs/`：库设计、格式、里程碑、应用、引用选择器和窗口重构文档

`gsword` 应用只依赖 `include/gsword/` 的公共接口。

本项目不链接上游 C++ `libsword`，而是按 SWORD 1.9 的公开行为、目录布局和存储格式重新实现一套可被 GObject Introspection 使用的 C 库，目标包括：

- 通过 HTTP/HTTPS/FTP 下载 SWORD 模块，并提供进度回调和取消能力；
- 解析本地/远端 `mods.d/*.conf`，在数据文件不存在时也能列出模块元数据；
- 读取常见 SWORD 模块（RawText、zText、RawCom、zCom、RawLD、zLD、RawGenBook、RawFiles、HREFCom）；
- 提供 `GswManager`、`GswModule`、`GswModuleInfo`、`GswInstallManager`、`GswStatusReporter` 等 GObject API。

## 当前状态

`libgsword` 的 M0–M13 和 `gsword` 桌面应用阶段 1–10 已完成，当前仓库可以构建、
测试并运行完整桌面应用。

### libgsword

- Meson 工程、GObject 公共 API、`pkg-config`、GIR/typelib、Vala API 和 gi-docgen 文档
- 本地模块发现、`mods.d` 配置解析、模块元数据、模块配置更新
- HTTP/HTTPS/FTP 传输、进度/取消、`mods.d.tar.gz` 安全解包、安装/卸载、本地目录安装
- `GswVerseKey` 和全部上游 versification 系统
- 读取 RawText/RawText4/RawCom/RawCom4、zText/zText4/zCom/zCom4、
  RawLD/RawLD4/zLD、RawGenBook、RawFiles、HREFCom
- LZSS/ZIP/BZIP2/XZ、Sapphire 解密、SCSU 等编码转换
- 基础渲染、Strong's/脚注/标题/red letter 等渲染选项、EntryAttributes、
  OSIS/ThML/GBF 输出
- phrase/multiword/regex/属性搜索、范围/多模块搜索、取消和进度、可选持久化倒排索引
- RawFiles 和 RawGenBook 写入、树结构编辑、文件锁、失败回滚和崩溃恢复
- zText/zCom/zLD LRU 块缓存、独立模块实例、并发读取测试
- 常规测试、ASan 测试、本地 HTTP 安装集成测试、locale 校验和 introspection 测试

### gsword 桌面应用

- GTK4/libadwaita 应用，按页面对象拆分；单行自适应主导航和可隐藏模块侧栏，窄窗口切换到底部导航和覆盖式侧栏
- 整章阅读、书卷/章/节选择器、键盘输入和本地化书名/缩写
- 阅读历史前进/后退和最近阅读列表，跨重启保存
- 1–4 栏多译本对照、同步滚动、独立滚动、锁定、拖拽和持久化工作区
- 切换圣经版本时保持当前 passage；新版本缺少该经文时回到同一书卷首章首节
- 研经面板：注释、词典、字典、Strong's、交叉引用、原文和笔记
- 笔记、标签、经文列表、高亮、Markdown 编辑/导出，以及 SQLite 持久化
- 阅读计划、每日灵修、背经复习、讲道/教学布局
- 模块库筛选（类型、语言、状态）、模块详情、安装/更新/删除、进度和取消；
  对照栏只列出圣经模块
- 引用选择器拆分出独立的 `gsword-reference-parser.c`，路径栏和选择器 popover
  由 reader 和对照栏共享

## 快速开始

```sh
meson setup build
meson compile -C build
meson test -C build

# 列出本地模块
./build/examples/gsword-list-modules ~/.sword

# 读取经文
./build/examples/gsword-lookup ~/.sword KJV "John 3:16"
./build/examples/gsword-lookup ~/.sword KJV "启示录 1:1" zh-Hans

# 模块信息、章节、字典、通用书
./build/examples/gsword-module-info ~/.sword KJV
./build/examples/gsword-chapter ~/.sword KJV "John 3"
./build/examples/gsword-dict-lookup ~/.sword StrongsGreek G26
./build/examples/gsword-genbook-walk ~/.sword BookChapter 1
./build/examples/gsword-render-options ~/.sword KJV "John 3:16" "Strong's Numbers=On"
./build/examples/gsword-locale ~/.sword zh-Hans KJV "来 4:1"
./build/examples/gsword-search ~/.sword KJV "God so loved" phrase
./build/examples/gsword-search ~/.sword KJV "God.*world" regex
./build/examples/gsword-index-module ~/.sword KJV build
./build/examples/gsword-index-module ~/.sword KJV status
./build/examples/gsword-parallel ~/.sword KJV "John 3:16" "Rom 8:1"

# GTK4 桌面应用
GSWORD_PREFIX=~/.sword ./build/src/gsword
# 或指定模块库
./build/src/gsword --library ~/.sword

# 引用选择器 demo 和测试
./build/tests/gsword-reference-chooser-demo en KJV "Gen 1:1"
meson test -C build reference-chooser

# 开发时使用构建目录中的中文界面翻译
LANG=zh_CN.UTF-8 GSW_LOCALEDIR=$PWD/build/po \
  ./build/src/gsword --library ~/.sword

# 安装后，应用文本翻译位于 ${prefix}/share/locale
meson install -C build

# 安装（需要预先配置 InstallMgr.conf）
./build/examples/gsword-install-sources ~/.sword/InstallMgr
./build/examples/gsword-install-module ~/.sword ~/.sword/InstallMgr CrossWire KJV
./build/examples/gsword-async-install ~/.sword ~/.sword/InstallMgr CrossWire KJV
```

开发构建尚未安装 locale 数据时，可以指向源码数据目录：

```sh
GSW_LOCALE_PATH=$PWD/data/locales.d \
  ./build/examples/gsword-lookup ~/.sword KJV "启示录 1:1" zh-Hans
```

使用 gi-docgen 生成 API 文档：

```sh
meson setup build-doc -Ddocs=true
meson compile -C build-doc docs
# 输出：build-doc/docs/reference/gsword/index.html
```

公共头文件中的文档注释包含每个参数的用途、返回值、内存所有权
（`(transfer none)` / `(transfer full)` / `(transfer container)`）以及异步
API 的 `finish` 对应关系；枚举值、类结构和信号也有说明。`-Ddocs=true`
会自动启用 GObject Introspection，以便 gi-docgen 从 GIR 读取 API。

生成 GObject Introspection 数据和 Vala API：

```sh
meson setup build-gir -Dintrospection=true
meson compile -C build-gir
meson test -C build-gir introspection
# GIR：build-gir/GSword-0.1.gir
# typelib：build-gir/GSword-0.1.typelib
# Vala：build-gir/gsword.vapi
```

GIR 命名空间为 `GSword`；VAPI 命名空间仍为 `Gsw`（由 C 类型前缀生成）。

`gsword.vapi` 会连同 `gsword.deps` 安装到 `share/vala/vapi`，可在 Vala
工程中直接使用：

```vala
using Gsw;

var key = Gsw.verse_key_new("John 3:16", "");
print("%s\n", key.get_text());
key.unref();
```

## 构建选项

- `-Dtests=true`：构建并注册测试（默认开启）
- `-Ddocs=true`：启用 gi-docgen API 文档，同时启用 introspection
- `-Dintrospection=true`：生成 GIR/typelib 和 Vala API
- `-Dbzip2=auto` / `-Dlzma=auto`：自动检测可选压缩库
- 检测到 GTK4、libadwaita 和 SQLite3 时构建桌面应用；检测到 GTK4 时构建引用选择器 demo，`-Dtests=true` 时同时注册其测试

## 文档

- [`docs/design.md`](docs/design.md)：libgsword 总体架构、对象模型、API 草案、流程、里程碑、风险。
- [`docs/milestones.md`](docs/milestones.md)：M0–M13 实现记录和验收条件。
- [`docs/formats.md`](docs/formats.md)：配置文件、模块目录、Raw/z 存储格式、压缩、安装源等字节级说明。
- [`docs/upstream-analysis.md`](docs/upstream-analysis.md)：上游 SWORD 1.9 源码分析与缺陷总结。
- [`docs/app-design.md`](docs/app-design.md)：gsword GTK4 应用的功能与界面设计。
- [`docs/workspace-design.md`](docs/workspace-design.md)：阅读工作区、活动经文和多资源协同设计。
- [`docs/reference-chooser.md`](docs/reference-chooser.md)：书卷/章/节引用选择器的使用和设计说明。
- [`docs/window-refactor-plan.md`](docs/window-refactor-plan.md)：GTK4 窗口页面对象拆分计划。
- [`docs/window-refactor-acceptance.md`](docs/window-refactor-acceptance.md)：窗口重构各阶段验收记录。

上游源码位于 `sword-1.9.0/`，仅作为参考，不参与构建。旧 C++ wrap 不包含在当前源码树中；本项目从头实现。

## 依赖

- GLib / GObject / GIO（>= 2.66）
- libcurl（>= 7.68）
- zlib
- 可选：libbz2、liblzma、gobject-introspection（GIR）、vala/vapigen（VAPI）、gi-docgen（API 文档）
- GTK4 应用：GTK4、libadwaita、SQLite3、可选 GtkSourceView 5
- Meson / Ninja、pkg-config

## 许可证

GPL-2.0-or-later，与上游 SWORD 及旧 `gsword` 保持一致。

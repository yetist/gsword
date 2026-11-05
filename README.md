# GSword

<p align="center">
  <img src="data/icons/hicolor/scalable/apps/org.gsword.GSword.svg" width="128" height="128" alt="GSword logo">
</p>

**GSword 是一款使用 GTK4 / libadwaita 编写的现代研经软件。**

它围绕经文阅读、跨译本对照、注释与原文研究、个人笔记和阅读计划组织界面，并直接读取 SWORD 模块。

GSword 的底层是 **libgsword**：一个使用 C / GObject 从零重新实现的 SWORD 模块库。它不链接上游 C++ `libsword`，同时提供 GObject Introspection 和 Vala API，便于其他 GTK/GNOME 应用复用。

- 源码仓库：<https://github.com/yetist/gsword>
- 问题反馈：<https://github.com/yetist/gsword/issues>

> 当前项目仍在持续开发中，尚未正式发布。

## 功能

### 阅读

- 整章阅读、书卷/章/节选择器和引用输入
- 1–4 栏多译本对照，支持同步滚动、固定、独立和拖拽重排
- 阅读历史前进/后退、最近阅读列表和阅读位置恢复
- 专注阅读模式、阅读宽度、字体大小、行距和正文衬线/无衬线选择
- 切换译本时保持当前 passage；新译本缺少该经文时回退到同一书卷首章
- 本地化书名和缩写，可选择“跟随模块语言”或“跟随界面语言”

### 研经

- 注释书、词典、字典、Strong's、交叉引用和原文资源协同
- 选中经文后，研经面板自动刷新对应注释、词典和原文信息
- 支持脚注、标题、红字、Strong's、形态标签等渲染选项
- 经文选择、复制、分享和高亮

### 个人内容

- 个人中心：今日、笔记、祷告、计划、收藏与列表
- 今日聚合继续阅读、今日计划、祷告摘要、背经复习和最近笔记
- 逐节笔记、标签、经文列表和经文高亮
- 祷告事项管理：进行中/已应允/已归档、搜索、筛选、关联经文和祷告日志
- Markdown 编辑、语法高亮和导出
- 阅读计划、进度跟踪、日程筛选、背经复习和教学布局
- 笔记、祷告、计划、背经队列和教学讲义保存在本地 SQLite 数据库

### 搜索

- phrase、multiword、regex 和属性搜索
- 按模块、旧约/新约、当前书卷/章节限定范围
- 异步搜索、进度、取消和结果计数
- 搜索结果右键菜单：复制、打开、添加笔记、加入背经/经文列表、查看模块信息
- 可选的持久化倒排索引，索引存放在 XDG cache 目录

### 模块管理

- 读取 RawText、zText、RawCom、zCom、RawLD、zLD、RawGenBook、RawFiles、HREFCom 等常见 SWORD 模块
- 已安装、可安装、可更新视图
- 从 HTTP/HTTPS/FTP 源安装和更新模块，支持进度、取消、代理和失败重试
- 本地目录安装、加密模块解锁、依赖/冲突提示
- 用户模块库可写，系统模块库只读叠加，用户库优先

## libgsword

`libgsword` 是 GSword 的底层模块库，也是本项目的重要组成部分，但产品重心是桌面研经体验。

它提供：

- `GswManager`、`GswModule`、`GswModuleInfo`
- `GswVerseKey`、`GswStrKey`、`GswTreeKey`
- `GswInstallManager`、`GswInstallSource`、`GswStatusReporter`
- `GswLocaleMgr` 和本地化书卷名
- GObject Introspection、GIR/typelib、Vala API 和 gi-docgen 文档

设计目标：

- 不链接上游 C++ `libsword`，按 SWORD 1.9 的公开行为、目录布局和存储格式重新实现
- 读取常见 SWORD 模块，并支持安装、更新、删除和写入
- 核心 API 使用 GObject 风格，便于 GTK、GNOME 和其他语言绑定使用

更详细的库设计见 [`docs/design.md`](docs/design.md)。

## 项目结构

```text
libsword/
├── src/                  GSword GTK4 应用和引用选择器
├── lib/                  libgsword 库实现
├── include/gsword/       libgsword 公共 API
├── data/locales.d/       libgsword locale 数据
├── tools/                locale 导入、校验和 versification 生成工具
├── examples/             库示例和 GTK4 原型
├── tests/                库测试、引用选择器测试和 demo
├── docs/                 设计、格式、里程碑和应用文档
└── sword-1.9.0/          上游 SWORD 源码，仅作参考，不参与构建
```

GSword 应用只依赖 `include/gsword/` 的公共接口。

## 构建

```sh
git clone https://github.com/yetist/gsword.git
cd gsword

meson setup build
meson compile -C build
```

只构建库和测试、不构建桌面应用时，可以缺少 GTK4/libadwaita/SQLite3。桌面应用需要：

- GTK4
- libadwaita
- SQLite3
- 可选 GtkSourceView 5

## 运行 GSword

```sh
./build/src/gsword
```

默认使用 XDG 用户模块库。也可以显式指定：

```sh
./build/src/gsword --library ~/.sword
# 或
GSWORD_LIBRARY=~/.sword ./build/src/gsword
```

开发构建使用源码中的界面翻译：

```sh
LANG=zh_CN.UTF-8 GSW_LOCALEDIR=$PWD/build/po \
  ./build/src/gsword --library ~/.local/share/gsword/library
```

## 用户数据目录

GSword 使用 XDG 目录，并按职责分离模块库、配置和缓存：

```text
~/.local/share/gsword/library/   可写的 SWORD 模块库
~/.local/share/gsword/app.db     SQLite：笔记、计划、背经、教学讲义
~/.local/share/gsword/exports/   默认导出目录
~/.config/gsword/installmgr/     InstallMgr.conf
~/.cache/gsword/installmgr/      下载缓存和远端模块列表
~/.cache/gsword/search/          搜索索引
```

系统模块库（例如 `/usr/share/sword`）以只读方式叠加，用户库优先。
`~/.sword` 不再隐式使用；如需使用旧库，可在偏好设置中切换，或通过
`--library PATH` / `GSWORD_LIBRARY=PATH` 指定。

## 测试与开发

```sh
meson test -C build

# ASan 构建
meson setup build-asan -Db_sanitize=address
meson test -C build-asan
```

库示例：

```sh
# 列出本地模块
./build/examples/gsword-list-modules ~/.local/share/gsword/library

# 读取经文
./build/examples/gsword-lookup ~/.local/share/gsword/library KJV "John 3:16"
./build/examples/gsword-lookup ~/.local/share/gsword/library KJV "启示录 1:1" zh-Hans

# 模块信息、章节、字典、通用书
./build/examples/gsword-module-info ~/.local/share/gsword/library KJV
./build/examples/gsword-chapter ~/.local/share/gsword/library KJV "John 3"
./build/examples/gsword-dict-lookup ~/.local/share/gsword/library StrongsGreek G26
./build/examples/gsword-genbook-walk ~/.local/share/gsword/library BookChapter 1
./build/examples/gsword-render-options ~/.local/share/gsword/library KJV "John 3:16" "Strong's Numbers=On"
./build/examples/gsword-html ~/.local/share/gsword/library KJV "John 3:16" verse > verse.html
./build/examples/gsword-html ~/.local/share/gsword/library KJV "John 3" chapter > chapter.html
./build/examples/gsword-html ~/.local/share/gsword/library KJV "John" book > john.html
./build/examples/gsword-locale ~/.local/share/gsword/library zh-Hans KJV "来 4:1"
./build/examples/gsword-search ~/.local/share/gsword/library KJV "God so loved" phrase
./build/examples/gsword-search ~/.local/share/gsword/library KJV "God.*world" regex
./build/examples/gsword-index-module ~/.local/share/gsword/library KJV build
./build/examples/gsword-index-module ~/.local/share/gsword/library KJV status
./build/examples/gsword-parallel ~/.local/share/gsword/library KJV "John 3:16" "Rom 8:1"
```

安装模块示例：

```sh
./build/examples/gsword-install-sources ~/.config/gsword/installmgr
./build/examples/gsword-install-module \
  ~/.local/share/gsword/library ~/.config/gsword/installmgr CrossWire KJV
./build/examples/gsword-async-install \
  ~/.local/share/gsword/library ~/.config/gsword/installmgr CrossWire KJV
```

## GObject Introspection 与文档

生成 GIR/typelib 和 Vala API：

```sh
meson setup build-gir -Dintrospection=true
meson compile -C build-gir
meson test -C build-gir introspection
# GIR：build-gir/GSword-0.1.gir
# typelib：build-gir/GSword-0.1.typelib
# Vala：build-gir/gsword.vapi
```

GIR 命名空间为 `GSword`；VAPI 命名空间为 `Gsw`。

生成 API 文档：

```sh
meson setup build-doc -Ddocs=true
meson compile -C build-doc docs
# 输出：build-doc/docs/reference/gsword/index.html
```

## 构建选项

- `-Dtests=true`：构建并注册测试（默认开启）
- `-Ddocs=true`：启用 gi-docgen API 文档，同时启用 introspection
- `-Dintrospection=true`：生成 GIR/typelib 和 Vala API
- `-Dbzip2=auto` / `-Dlzma=auto`：自动检测可选压缩库
- 检测到 GTK4、libadwaita 和 SQLite3 时构建桌面应用；检测到 GTK4 时构建引用选择器 demo

## 文档

- [`docs/design.md`](docs/design.md)：libgsword 架构、对象模型和 API 设计
- [`docs/milestones.md`](docs/milestones.md)：M0–M13 实现记录和验收条件
- [`docs/formats.md`](docs/formats.md)：配置文件、模块目录、Raw/z 存储格式和安装源
- [`docs/upstream-analysis.md`](docs/upstream-analysis.md)：上游 SWORD 1.9 源码分析
- [`docs/app-design.md`](docs/app-design.md)：GSword 桌面应用的功能与界面设计
- [`docs/personal-prayer-design.md`](docs/personal-prayer-design.md)：全局信息架构与个人中心规划（笔记、祷告、计划、收藏、书签、收获）
- [`docs/workspace-design.md`](docs/workspace-design.md)：阅读工作区、活动经文和多资源协同设计
- [`docs/reference-chooser.md`](docs/reference-chooser.md)：书卷/章/节引用选择器设计
- [`docs/window-refactor-plan.md`](docs/window-refactor-plan.md)：GTK4 窗口页面对象拆分计划
- [`docs/window-refactor-acceptance.md`](docs/window-refactor-acceptance.md)：窗口重构验收记录

## 依赖

- GLib / GObject / GIO（>= 2.66）
- libcurl（>= 7.68）
- zlib
- 可选：libbz2、liblzma、gobject-introspection、vala/vapigen、gi-docgen
- GSword 桌面应用：GTK4、libadwaita、SQLite3、可选 GtkSourceView 5
- Meson / Ninja、pkg-config

## 参与贡献

欢迎通过 GitHub Issues 报告问题、提出建议或提交改进：

<https://github.com/yetist/gsword/issues>

## 许可证

GPL-2.0-or-later，与上游 SWORD 及旧 `gsword` 保持一致。完整许可证文本见
[`COPYING`](COPYING)。

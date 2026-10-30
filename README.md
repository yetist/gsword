# libsword

基于 C / GObject 重新实现的 SWORD 模块库，同时包含基于该库的 GTK4 桌面应用
`gsword`。

## 目录结构

- `lib/`：`libgsword` 库实现
- `include/gsword/`：`libgsword` 公共 API
- `src/`：`gsword` GTK4 桌面应用
- `data/locales.d/`：libgsword 自有 locale 数据
- `tools/`：locale 导入、校验和 v11n 生成工具
- `examples/`：库示例和 GTK4 前端原型
- `tests/`：库测试
- `docs/app-design.md`：`gsword` 应用功能与界面设计

`gsword` 应用只依赖 `include/gsword/` 的公共接口。

本项目不链接上游 C++ `libsword`，而是按 SWORD 1.9 的公开行为、目录布局和存储格式重新实现一套可被 GObject Introspection 使用的 C 库，目标包括：

- 通过 HTTP/HTTPS/FTP 下载 SWORD 模块，并提供进度回调和取消能力；
- 解析本地/远端 `mods.d/*.conf`，在数据文件不存在时也能列出模块元数据；
- 读取常见 SWORD 模块（RawText、zText、RawCom、zCom、RawLD、zLD、RawGenBook）；
- 提供 `GswManager`、`GswModule`、`GswModuleInfo`、`GswInstallManager`、`GswStatusReporter` 等 GObject API。

## 当前状态

M0–M5 已实现并通过单元/集成测试：

- Meson 工程、GObject 公共 API、`pkg-config`
- 本地模块发现、`mods.d` 配置解析、模块元数据
- HTTP/HTTPS/FTP 传输、进度/取消、`mods.d.tar.gz` 安全解包、安装/卸载
- `GswVerseKey` + 全部上游 versification 系统
- RawText/RawText4/RawCom/RawCom4、zText/zText4/zCom/zCom4 读取
- RawLD/RawLD4/zLD、RawGenBook、RawFiles、HREFCom
- LZSS/ZIP/BZIP2/XZ、Sapphire 解密、SCSU -> UTF-8
- 基础渲染选项、内建英文 locale 与 XDG locale 包、本地化书名与缩写、LRU 块缓存、GI 与异步 API
- 基础搜索 API（phrase/multiword/regex）、可选持久化倒排索引、大模块性能测试
- GTK4/libadwaita 桌面应用 `gsword`（`src/`）

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

M5–M13 已完成：GIR/typelib 与 Vala API（`-Dintrospection=true`）、GTask
异步安装 API、Sapphire 解密、基础渲染选项（Strong's/脚注/标题/red letter）、
`locales.d` 本地化书名与缩写、zText/zCom/zLD LRU 块缓存、独立模块实例、BOTTOM 导航、
testament 识别、HTML 渲染片段、EntryAttributes、全局渲染选项、经文范围/OSIS、
TreeKey 定位、本地 DIR 安装源、运行期 CipherKey、选项持久化、可取消/可限定范围
的多模块搜索、持久化倒排搜索索引、RawFiles/RawGenBook 条目写入、RawGenBook
创建、树结构编辑、跨进程文件锁、写失败回滚和树事务崩溃恢复，以及可选 GTK4
前端原型和更多示例。

## 文档

- [`docs/design.md`](docs/design.md)：libgsword 总体架构、对象模型、API 草案、流程、里程碑、风险。
- [`docs/app-design.md`](docs/app-design.md)：gsword GTK4 应用的功能与界面设计。
- [`docs/formats.md`](docs/formats.md)：配置文件、模块目录、Raw/z 存储格式、压缩、安装源等字节级说明。
- [`docs/upstream-analysis.md`](docs/upstream-analysis.md)：上游 SWORD 1.9 源码分析与缺陷总结。

上游源码位于 `sword-1.9.0/`，仅作为参考，不参与构建。旧 C++ wrap 位于 `gsword/`，可参考其 API 命名，但本项目从头实现。

## 计划中的依赖

- GLib / GObject / GIO（>= 2.66）
- libcurl（>= 7.68）
- zlib
- 可选：libbz2、liblzma、gobject-introspection（GIR）、vala/vapigen（VAPI）、gi-docgen（API 文档）
- Meson / Ninja

## 许可证

GPL-2.0-or-later，与上游 SWORD 及旧 `gsword` 保持一致。

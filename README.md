# libsword

基于 C / GObject 重新实现的 SWORD 模块库。

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
- 基础渲染选项、本地化书名、4 块 zVerse 缓存、GI 与异步 API
- 基础搜索 API（phrase/multiword/regex）

## 快速开始

```sh
meson setup build
meson compile -C build
meson test -C build

# 列出本地模块
./build/examples/gsword-list-modules ~/.sword

# 读取经文
./build/examples/gsword-lookup ~/.sword KJV "John 3:16"

# 模块信息、章节、字典、通用书
./build/examples/gsword-module-info ~/.sword KJV
./build/examples/gsword-chapter ~/.sword KJV "John 3"
./build/examples/gsword-dict-lookup ~/.sword StrongsGreek G26
./build/examples/gsword-genbook-walk ~/.sword BookChapter 1
./build/examples/gsword-render-options ~/.sword KJV "John 3:16" "Strong's Numbers=On"
./build/examples/gsword-locale ~/.sword zh_CN KJV "John 3:16"
./build/examples/gsword-search ~/.sword KJV "God so loved" phrase
./build/examples/gsword-search ~/.sword KJV "God.*world" regex

# 安装（需要预先配置 InstallMgr.conf）
./build/examples/gsword-install-sources ~/.sword/InstallMgr
./build/examples/gsword-install-module ~/.sword ~/.sword/InstallMgr CrossWire KJV
./build/examples/gsword-async-install ~/.sword ~/.sword/InstallMgr CrossWire KJV
```

生成 GObject Introspection 数据：

```sh
meson setup build-gir -Dintrospection=true
meson compile -C build-gir
meson test -C build-gir introspection
```

M5/M6 已完成：GIR/typelib（`-Dintrospection=true`）、GTask 异步安装 API、Sapphire 解密、基础渲染选项（Strong's/脚注/标题/red letter）、`locales.d` 本地化书名、zVerse 多块缓存，以及更多示例。

## 文档

- [`docs/design.md`](docs/design.md)：总体架构、对象模型、API 草案、流程、里程碑、风险。
- [`docs/formats.md`](docs/formats.md)：配置文件、模块目录、Raw/z 存储格式、压缩、安装源等字节级说明。
- [`docs/upstream-analysis.md`](docs/upstream-analysis.md)：上游 SWORD 1.9 源码分析与缺陷总结。

上游源码位于 `sword-1.9.0/`，仅作为参考，不参与构建。旧 C++ wrap 位于 `gsword/`，可参考其 API 命名，但本项目从头实现。

## 计划中的依赖

- GLib / GObject / GIO（>= 2.66）
- libcurl（>= 7.68）
- zlib
- 可选：libbz2、liblzma
- Meson / Ninja

## 许可证

GPL-2.0-or-later，与上游 SWORD 及旧 `gsword` 保持一致。

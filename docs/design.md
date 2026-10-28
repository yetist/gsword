# libsword（GObject/C）设计

- 状态：设计草案 v0.5（M0–M5 已实现）
- 日期：2026-10-28
- 目标代码名：`gsword`（库名 `libgsword`，C 前缀 `Gsw`）
- 参考实现：`sword-1.9.0/`（C++，仅作为行为与格式参考，不链接、不包含其头文件）

---

## 1. 目标与边界

### 1.1 必须实现

1. **远程下载**
   - 支持 HTTP、HTTPS、FTP。
   - 支持从 SWORD 兼容仓库读取模块列表（`mods.d.tar.gz`，失败时回退到目录列表）。
   - 支持下载模块数据并安装到本地 SWORD 兼容目录。
   - 支持通过回调/信号获取当前文件、累计字节数、总字节数等进度。
   - 支持取消。
2. **解析模块信息**
   - 解析 `mods.d/*.conf`（INI 风格，允许重复键）。
   - 在模块数据文件不存在时也能列出模块元数据（远端模块列表的必要条件）。
   - 提供名称、描述、类型、语言、版本、驱动、编码、安装大小、版权、是否加密等信息。
3. **读取模块内容**
   - 至少支持 SWORD 常见驱动：
     - `RawText` / `RawText4`
     - `zText` / `zText4`（LZSS/ZIP/BZIP2/XZ）
     - `RawCom` / `RawCom4`
     - `zCom` / `zCom4`
     - `RawLD` / `RawLD4`
     - `zLD`
     - `RawGenBook`
   - 支持按经文键、字典键、树键定位并读取原始条目。
   - 提供 UTF-8 文本输出；对常见标记（OSIS/ThML/GBF）提供基础纯文本剥离。
4. **GObject API**
   - 使用 GLib/GObject 类型系统、`GError`、信号、属性。
   - 可生成 GObject Introspection（GIR/typelib），方便 Python/JS/Vala 等绑定。
   - 命名参考 SWORD，但按 GObject 惯例组织。

### 1.2 非目标（v1）

以下功能不在第一版承诺范围内，但架构应预留：

- 写入/编辑模块（`setEntry`、`linkEntry`、`deleteEntry`）。
- 完整 Lucene/索引搜索框架；当前提供基础 phrase/multiword/regex 搜索。
- 完整渲染过滤器（Strong's、脚注、red letter、headings 等所有 OSIS/ThML/GBF 选项）。
- 模块加密/解密已作为 M5 功能实现（Sapphire、`CipherKey`）。
- 所有本地化书名、界面翻译。
- SFTP、HTTP 代理认证 UI、断点续传（可后续增加）。
- `VerseTreeKey` 等冷门驱动。
- 与旧 `gsword` 0.1.x 的源代码/ABI 兼容。

### 1.3 基本假设

- 本地目录布局与 SWORD 兼容：
  ```
  <prefix>/mods.d/*.conf
  <prefix>/modules/texts/ztext/<module>/...
  ```
- 远端仓库布局与 CrossWire 兼容。
- 输入的模块配置和数据来自不可信网络，必须按不可信数据做路径、大小、解包安全校验。
- 第一版以 Linux/Unix 为主，但代码避免依赖 Unix 专有接口（路径分隔符、文件权限除外）。

---

## 2. 上游 SWORD 关键机制摘要

详细分析见 `docs/upstream-analysis.md`，存储格式见 `docs/formats.md`。这里只列影响架构的结论。

### 2.1 上游对象

| 上游类 | 职责 | 本项目对应 |
|---|---|---|
| `SWMgr` | 发现配置、加载模块、管理过滤器 | `GswManager` |
| `SWConfig` | INI 配置读写、合并 | `GswConfig` |
| `SWModule` | 模块基类、键、过滤器、条目访问 | `GswModule` + 内部 backend |
| `SWText` / `SWCom` / `SWLD` / `SWGenBook` | 模块语义基类 | 由 backend 的键类型体现 |
| `VerseKey` / `StrKey` / `TreeKeyIdx` | 定位 | `GswVerseKey` / `GswStrKey` / `GswTreeKey` |
| `VersificationMgr` | 经文卷章偏移 | `GswVersificationMgr` |
| `InstallMgr` | 安装/卸载/远端源 | `GswInstallManager` |
| `InstallSource` | 远端源配置 | `GswInstallSource` |
| `RemoteTransport` | HTTP/FTP 传输 | `GswTransport` |
| `StatusReporter` | 下载进度 | `GswStatusReporter` |
| `SWCompress` 及子类 | LZSS/ZIP/BZIP2/XZ | 内部 `GswCompressor` 接口 |

### 2.2 上游加载流程

```
SWMgr::load()
  -> findConfig()                 // 查找 sword.conf / SWORD_PATH / ~/.sword / 系统路径
  -> loadConfigDir()              // 读取 mods.d/*.conf，合并为 SectionMap
  -> createAllModules()           // 对每个 section 创建 SWModule 子类
       -> createModule(name, driver, section)
            -> 计算 PrefixPath / AbsoluteDataPath
            -> new RawText / zText / RawLD / RawGenBook ...
```

关键点：**上游在创建模块对象时就打开数据文件**。远端模块列表场景下数据文件并不存在，因此上游驱动构造函数必须容忍打开失败，靠“打开失败但不报错”的方式继续。本项目改为**元数据与后端分离、后端懒加载**，从根上消除这个矛盾。

### 2.3 上游安装流程

```
InstallMgr::refreshRemoteSource(is)
  -> 删除 <private>/<uid>/mods.d
  -> 下载 <source>/<dir>/mods.d.tar.gz 并解包
  -> 失败则递归下载 <source>/<dir>/mods.d/ 下所有 .conf

InstallSource::getMgr()
  -> new SWMgr(<private>/<uid>)    // 只有元数据，无数据文件

InstallMgr::installModule(destMgr, fromLocation, modName, is)
  -> 在源 shadow 库中找到模块 section
  -> 若有 File= 条目：逐个下载/复制
  -> 否则按 DataPath 递归下载/复制整个目录
  -> 复制 mods.d/<file>.conf 到目标
```

上游实现存在若干需要修正的问题：

- 解包 tar.gz 不检查 `../`、绝对路径、符号链接。
- 安装不是原子的，失败/取消会留下半成品。
- 配置文件最后写入这一点没有明确保证。
- 下载时把数据先落到 shadow 再复制，重复 I/O。
- `remoteCopy` 的进度只有“当前文件”语义，UI 需要自己累计。
- HTTP 目录列表靠解析 `<a href>` 和 `<td>`，脆弱。
- 多处 `new/delete`、C 字符串、全局状态，不利于绑定和线程使用。

### 2.4 上游格式结论

- 文本/注释模块：`RawText` 为 6 字节索引；`RawText4` 为 8 字节索引。
- 压缩文本：`.zv` 是逐经文索引（zText 10 字节，zText4 12 字节），`.zs` 是块索引（12 字节），`.zz` 是压缩块数据。
- 字典：`RawLD` 6 字节索引，`RawLD4` 8 字节索引；`zLD` 为 8 字节索引 + `EntriesBlock` 压缩块。
- 书签/通用书：`.bdt` 数据 + TreeKeyIdx 的 `.idx`/`.dat`。
- 压缩算法：LZSS（SWORD 自有位流）、zlib、bzip2、xz。
- 经文索引由 versification 的“模块标题/约标题/书标题/章标题/经文”偏移决定。

这些格式在 `docs/formats.md` 中给出字节级说明和测试向量要求。

---

## 3. 总体架构

### 3.1 分层

```
┌──────────────────────────────────────────────────────────────┐
│ Public GObject API                                           │
│ GswManager GswModule GswModuleInfo GswInstallManager ...      │
├──────────────────────────────────────────────────────────────┤
│ Core                                                         │
│ 配置/元数据  模块注册表  路径与版本  错误/日志                 │
├───────────────────────────┬──────────────────────────────────┤
│ Keys & Versification      │ Module Backends                   │
│ GswKey/GswVerseKey        │ rawtext ztext rawcom zcom         │
│ GswTreeKey/GswStrKey      │ rawld zld rawgenbook              │
│ GswVersificationMgr       │ EntriesBlock / block cache       │
├───────────────────────────┴──────────────────────────────────┤
│ Codecs & Filters                                             │
│ LZSS ZIP BZIP2 XZ  Latin1/UTF16→UTF8  basic OSIS/ThML/GBF      │
├──────────────────────────────────────────────────────────────┤
│ Networking & Install                                         │
│ GswTransport(curl) 目录列表 归档解包 原子安装 进度/取消        │
├──────────────────────────────────────────────────────────────┤
│ Platform                                                     │
│ GLib/GObject  libcurl  zlib  (libbz2)  (liblzma)              │
└──────────────────────────────────────────────────────────────┘
```

依赖方向只能自上而下。`GswInstallManager` 依赖 `GswManager`、`GswTransport`，但 `GswManager` 不依赖安装器。

### 3.2 建议目录结构

```
libsword/
├── AGENTS.md
├── README.md
├── meson.build
├── meson_options.txt
├── docs/
│   ├── design.md
│   ├── formats.md
│   └── upstream-analysis.md
├── include/gsword/
│   ├── gsword.h
│   ├── gsw-version.h
│   ├── gsw-error.h
│   ├── gsw-enums.h
│   ├── gsw-config.h
│   ├── gsw-module-info.h
│   ├── gsw-manager.h
│   ├── gsw-module.h
│   ├── gsw-key.h
│   ├── gsw-verse-key.h
│   ├── gsw-str-key.h
│   ├── gsw-tree-key.h
│   ├── gsw-versification.h
│   ├── gsw-status-reporter.h
│   ├── gsw-transport.h
│   ├── gsw-install-source.h
│   └── gsw-install-manager.h
├── src/
│   ├── gsw-config.c
│   ├── gsw-module-info.c
│   ├── gsw-manager.c
│   ├── gsw-module.c
│   ├── gsw-error.c
│   ├── gsw-utils.c / gsw-utils.h
│   ├── gsw-path.c / gsw-path.h
│   ├── gsw-version.c
│   ├── keys/
│   │   ├── gsw-key.c
│   │   ├── gsw-verse-key.c
│   │   ├── gsw-str-key.c
│   │   ├── gsw-tree-key.c
│   │   └── gsw-versification.c
│   ├── modules/
│   │   ├── gsw-module-backend.c / .h
│   │   ├── backends/
│   │   │   ├── gsw-rawtext.c
│   │   │   ├── gsw-ztext.c
│   │   │   ├── gsw-rawstr.c
│   │   │   ├── gsw-zstr.c
│   │   │   └── gsw-rawgenbook.c
│   │   └── formats/
│   │       ├── gsw-rawverse.c
│   │       ├── gsw-zverse.c
│   │       └── gsw-entries-block.c
│   ├── codecs/
│   │   ├── gsw-compressor.c / .h
│   │   ├── gsw-lzss.c
│   │   ├── gsw-zip.c
│   │   ├── gsw-bzip2.c
│   │   └── gsw-xz.c
│   ├── filters/
│   │   ├── gsw-strip.c
│   │   └── gsw-encoding.c
│   ├── net/
│   │   ├── gsw-curl.c
│   │   ├── gsw-dir-list.c
│   │   └── gsw-tar.c
│   └── install/
│       ├── gsw-install-manager.c
│       └── gsw-install-source.c
├── data/
│   └── v11n/                    # 生成或手工维护的经文表
├── tools/
│   └── gen-v11n.py              # 从 canon_*.h 生成 C 表
├── tests/
│   ├── data/                    # 合成模块、conf、归档
│   ├── unit/
│   ├── integration/
│   └── compat/
└── examples/
```

### 3.3 依赖

| 依赖 | 必需 | 用途 | 最低建议版本 |
|---|---|---|---|
| GLib/GObject/GIO | 是 | 类型、容器、文件、GBytes、GTask、TLS 辅助（可选） | 2.66 |
| libcurl | 是 | HTTP/HTTPS/FTP 传输 | 7.68 |
| zlib | 是 | `mods.d.tar.gz`、CompressType=ZIP | 1.2.11 |
| libbz2 | 可选 | CompressType=BZIP2 | 1.0.8 |
| liblzma | 可选 | CompressType=XZ | 5.2 |
| Meson/Ninja | 构建 | 构建系统 | 0.63 |
| gobject-introspection | 可选 | GIR/typelib | 1.68 |

不依赖 `libsword`、`libxml2`、`libsoup`。配置是 SWORD 自有 INI 格式，不需要 XML 解析器。

### 3.4 关键设计原则

1. **元数据与数据分离**：`GswModuleInfo` 完全由 conf 构造，不打开任何数据文件。`GswModule` 的后端懒加载。
2. **不透明的内部后端**：公共 API 只暴露 `GswModule`，不暴露驱动类；驱动通过内部 vtable/`GswModuleBackend` 实现。
3. **失败必须可诊断**：所有可能失败的操作返回 `gboolean` + `GError`，禁止上游的全局 `error` 状态。
4. **网络数据不可信**：路径、归档成员、文件大小、uid 全部校验。
5. **安装原子化**：先写临时目录，校验后 rename，conf 最后写。
6. **面向绑定设计**：GObject 属性/信号、`GPtrArray`、`GBytes`、`GError`；避免裸 C 数组和回调函数指针。

---

## 4. 对象模型

### 4.1 类图（逻辑）

```
GObject
├── GswConfig                    配置文档（section -> key -> [values]）
├── GswModuleInfo                不可变元数据
├── GswManager                   本地模块库
├── GswModule                    可读取的模块实例
├── GswInstallSource             远端源
├── GswInstallManager            安装/卸载/刷新
├── GswTransport (derivable)     传输抽象；内置 curl 实现
└── GswStatusReporter (derivable)进度信号

引用计数结构（boxed）
├── GswKey (base)                键基类
│   ├── GswVerseKey             经文键
│   ├── GswStrKey               字典字符串键
│   └── GswTreeKey              树键
├── GswVersification            经文系统
├── GswBook                     书卷
└── GswDirEntry                 目录项
```

### 4.2 为什么键用 boxed 而不是 GObject

- 键是值语义、生命周期短、迭代频繁；GObject 的构造/销毁开销偏大。
- 键需要可克隆、可比较、可复制，boxed + 引用计数更自然。
- 仍可通过 `G_DEFINE_BOXED_TYPE` / `G_DEFINE_AUTOPTR_CLEANUP_FUNC` 生成绑定。

### 4.3 所有权约定

| 对象 | 拥有者 | 返回约定 |
|---|---|---|
| `GswConfig` | 调用者；可 `g_object_ref` | `gsw_config_new*` 返回 transfer full |
| `GswModuleInfo` | `GswManager` 缓存；调用者可 ref | getter 返回 transfer none |
| `GswModule` | `GswManager` 缓存；调用者可 ref | getter 返回 transfer none |
| `GswKey` | 调用者/模块 | `create_key` transfer full；`get_key` transfer none |
| `GPtrArray` | 调用者 | 返回 transfer container；元素 transfer none |
| `GBytes` | 调用者 | 返回 transfer full |
| `gchar*` | 调用者 | 标注 transfer full；`const gchar*` 为 transfer none |

所有公共头文件使用 GI 注释（`(transfer none)`、`(element-type ...)`、`(nullable)`、`(out)`）。

### 4.4 命名与兼容

- 保留旧 `gsword` 的 `Gsw` 前缀，便于已有示例迁移。
- 新库**不保证**与旧 `gsword` ABI/API 兼容；旧库是 C++ wrap，本项目是独立实现。
- 头文件安装到 `gsword/`；pkg-config 建议：
  - 包名 `gsword`
  - 库 `libgsword.so`
  - 若需要与旧库并存，可改用 `gsword-2` / `gsword2.pc`，并在设计评审时决定。

---

## 5. 公共 API 草案

以下为接口形状，不是最终头文件。实现时补充 `GSW_AVAILABLE_IN_*`、GI 注解和 `G_BEGIN_DECLS`。

### 5.1 错误与枚举

```c
/* gsw-error.h */
typedef enum {
  GSW_ERROR_CONFIG,
  GSW_ERROR_IO,
  GSW_ERROR_NETWORK,
  GSW_ERROR_PROTOCOL,
  GSW_ERROR_NOT_FOUND,
  GSW_ERROR_INVALID_DATA,
  GSW_ERROR_UNSUPPORTED,
  GSW_ERROR_CANCELLED,
  GSW_ERROR_CIPHERED,
  GSW_ERROR_PERMISSION,
  GSW_ERROR_INSTALL,
  GSW_ERROR_OUT_OF_BOUNDS
} GswError;

GQuark gsw_error_quark(void);
#define GSW_ERROR (gsw_error_quark())
```

```c
/* gsw-enums.h */
typedef enum {
  GSW_MODULE_TYPE_UNKNOWN,
  GSW_MODULE_TYPE_BIBLES,
  GSW_MODULE_TYPE_COMMENTARIES,
  GSW_MODULE_TYPE_LEXDICTS,
  GSW_MODULE_TYPE_GENBOOKS,
  GSW_MODULE_TYPE_DAILY_DEVOS,
  GSW_MODULE_TYPE_MAPS,
  GSW_MODULE_TYPE_OTHER
} GswModuleType;

typedef enum {
  GSW_DRIVER_UNKNOWN,
  GSW_DRIVER_RAW_TEXT,
  GSW_DRIVER_RAW_TEXT4,
  GSW_DRIVER_Z_TEXT,
  GSW_DRIVER_Z_TEXT4,
  GSW_DRIVER_RAW_COM,
  GSW_DRIVER_RAW_COM4,
  GSW_DRIVER_Z_COM,
  GSW_DRIVER_Z_COM4,
  GSW_DRIVER_RAW_LD,
  GSW_DRIVER_RAW_LD4,
  GSW_DRIVER_Z_LD,
  GSW_DRIVER_RAW_GENBOOK,
  GSW_DRIVER_RAW_FILES,
  GSW_DRIVER_HREF_COM
} GswDriver;

typedef enum {
  GSW_SOURCE_FTP,
  GSW_SOURCE_HTTP,
  GSW_SOURCE_HTTPS,
  GSW_SOURCE_SFTP
} GswSourceType;

typedef enum {
  GSW_MODULE_STATUS_NEW            = 1 << 0,
  GSW_MODULE_STATUS_UPDATED        = 1 << 1,
  GSW_MODULE_STATUS_OLDER          = 1 << 2,
  GSW_MODULE_STATUS_SAME_VERSION   = 1 << 3,
  GSW_MODULE_STATUS_CIPHERED       = 1 << 4,
  GSW_MODULE_STATUS_KEY_PRESENT    = 1 << 5
} GswModuleStatusFlags;

typedef enum {
  GSW_ENCODING_UNKNOWN,
  GSW_ENCODING_LATIN1,
  GSW_ENCODING_UTF8,
  GSW_ENCODING_SCSU,
  GSW_ENCODING_UTF16
} GswEncoding;

typedef enum {
  GSW_DIRECTION_LTR,
  GSW_DIRECTION_RTL,
  GSW_DIRECTION_BIDI
} GswDirection;

typedef enum {
  GSW_MARKUP_UNKNOWN,
  GSW_MARKUP_PLAIN,
  GSW_MARKUP_THML,
  GSW_MARKUP_GBF,
  GSW_MARKUP_OSIS,
  GSW_MARKUP_TEI
} GswMarkup;

typedef enum {
  GSW_POSITION_TOP,
  GSW_POSITION_BOTTOM
} GswPosition;

typedef enum {
  GSW_SEARCH_PHRASE,
  GSW_SEARCH_MULTIWORD,
  GSW_SEARCH_REGEX
} GswSearchType;
```

### 5.2 `GswConfig`

```c
#define GSW_TYPE_CONFIG (gsw_config_get_type())
G_DECLARE_FINAL_TYPE(GswConfig, gsw_config, GSW, CONFIG, GObject)

GswConfig *gsw_config_new(void);
GswConfig *gsw_config_new_from_file(const gchar *path, GError **error);
GswConfig *gsw_config_new_from_data(const gchar *data, gssize len, GError **error);
GswConfig *gsw_config_copy(GswConfig *self);

gboolean gsw_config_load(GswConfig *self, const gchar *path, GError **error);
gboolean gsw_config_save(GswConfig *self, const gchar *path, GError **error);

gboolean      gsw_config_has_section(GswConfig *self, const gchar *section);
gchar       **gsw_config_get_sections(GswConfig *self);          /* NULL 结尾 */
gboolean      gsw_config_remove_section(GswConfig *self, const gchar *section);

gboolean      gsw_config_has_key(GswConfig *self, const gchar *section, const gchar *key);
const gchar  *gsw_config_get(GswConfig *self, const gchar *section, const gchar *key);
gchar       **gsw_config_get_all(GswConfig *self, const gchar *section, const gchar *key);
gboolean      gsw_config_set(GswConfig *self, const gchar *section,
                             const gchar *key, const gchar *value);
gboolean      gsw_config_add(GswConfig *self, const gchar *section,
                             const gchar *key, const gchar *value);
gboolean      gsw_config_remove_key(GswConfig *self, const gchar *section, const gchar *key);

void          gsw_config_augment(GswConfig *self, GswConfig *other);
```

语义：

- section/key 大小写敏感；与 SWORD 一致。
- 重复 key 保留顺序（或按键排序保存，但读取返回全部值）。
- 值两端空白去除；`#` 开头行为注释；忽略 UTF-8 BOM。
- `augment()` 合并另一个配置；重复 key 按 SWORD 规则追加/覆盖，不丢失多值。

### 5.3 `GswModuleInfo`

```c
#define GSW_TYPE_MODULE_INFO (gsw_module_info_get_type())
G_DECLARE_FINAL_TYPE(GswModuleInfo, gsw_module_info, GSW, MODULE_INFO, GObject)

GswModuleInfo *gsw_module_info_new_from_config(const gchar *name,
                                               GswConfig *config,
                                               const gchar *config_file,
                                               const gchar *prefix_path,
                                               GError **error);

const gchar   *gsw_module_info_get_name(GswModuleInfo *self);
const gchar   *gsw_module_info_get_description(GswModuleInfo *self);
const gchar   *gsw_module_info_get_type_string(GswModuleInfo *self);
GswModuleType  gsw_module_info_get_module_type(GswModuleInfo *self);
GswDriver      gsw_module_info_get_driver(GswModuleInfo *self);
const gchar   *gsw_module_info_get_driver_name(GswModuleInfo *self);
const gchar   *gsw_module_info_get_language(GswModuleInfo *self);
const gchar   *gsw_module_info_get_version(GswModuleInfo *self);
const gchar   *gsw_module_info_get_minimum_version(GswModuleInfo *self);
guint64        gsw_module_info_get_install_size(GswModuleInfo *self);
const gchar   *gsw_module_info_get_data_path(GswModuleInfo *self);
const gchar   *gsw_module_info_get_absolute_data_path(GswModuleInfo *self);
const gchar   *gsw_module_info_get_prefix_path(GswModuleInfo *self);
const gchar   *gsw_module_info_get_config_file(GswModuleInfo *self);
const gchar   *gsw_module_info_get_encoding(GswModuleInfo *self);
const gchar   *gsw_module_info_get_source_type(GswModuleInfo *self);
const gchar   *gsw_module_info_get_versification(GswModuleInfo *self);
const gchar   *gsw_module_info_get_copyright(GswModuleInfo *self);
const gchar   *gsw_module_info_get_distribution_license(GswModuleInfo *self);
const gchar   *gsw_module_info_get_about(GswModuleInfo *self);
gboolean       gsw_module_info_is_ciphered(GswModuleInfo *self);
gboolean       gsw_module_info_is_supported(GswModuleInfo *self);

const gchar   *gsw_module_info_get_value(GswModuleInfo *self, const gchar *key);
gchar        **gsw_module_info_get_keys(GswModuleInfo *self);
gchar        **gsw_module_info_get_values(GswModuleInfo *self, const gchar *key);
```

`GswModuleInfo` 应是不可变快照。它只依赖 `[Name]` section 的键值，不依赖数据文件。

`is_supported` 的判定同时考虑驱动是否实现、以及编译期是否包含对应压缩库（例如未编译 bzip2 时，`CompressType=BZIP2` 的 zText 模块不可读）。

### 5.4 `GswManager`

```c
#define GSW_TYPE_MANAGER (gsw_manager_get_type())
G_DECLARE_FINAL_TYPE(GswManager, gsw_manager, GSW, MANAGER, GObject)

GswManager *gsw_manager_new(GError **error);
GswManager *gsw_manager_new_for_path(const gchar *prefix_path, GError **error);
GswManager *gsw_manager_new_for_config(const gchar *config_path, GError **error);

gboolean gsw_manager_load(GswManager *self, GError **error);
gboolean gsw_manager_reload(GswManager *self, GError **error);

void gsw_manager_add_augment_path(GswManager *self, const gchar *path);
const gchar *gsw_manager_get_prefix_path(GswManager *self);
const gchar *gsw_manager_get_config_path(GswManager *self);

GPtrArray     *gsw_manager_get_module_infos(GswManager *self);   /* GswModuleInfo* */
GswModuleInfo *gsw_manager_lookup_module_info(GswManager *self, const gchar *name);
gboolean       gsw_manager_has_module(GswManager *self, const gchar *name);

GswModule     *gsw_manager_get_module(GswManager *self, const gchar *name); /* lazy */
GswConfig     *gsw_manager_get_config(GswManager *self);                    /* borrowed */

gboolean       gsw_manager_remove_module_files(GswManager *self,
                                               const gchar *name,
                                               GError **error);
```

发现顺序（与 SWORD 兼容，但简化）：

1. 显式 `prefix_path` / `config_path`。
2. 当前目录 `mods.conf` 或 `mods.d/`。
3. `SWORD_PATH` 环境变量。
4. 当前目录 `sword.conf`、`/etc/sword.conf`、`~/.sword/sword.conf` 中的 `[Install] DataPath` 和 `AugmentPath`。
5. `~/.sword/mods.d`、`~/sword/mods.d`。
6. 编译期默认前缀（如 `/usr/share/sword`）。

`GswManager::load()` 只做元数据合并和索引；不实例化后端。`gsw_manager_get_module()` 第一次调用时才创建 `GswModule` 并打开后端。

### 5.5 `GswModule`

```c
#define GSW_TYPE_MODULE (gsw_module_get_type())
G_DECLARE_FINAL_TYPE(GswModule, gsw_module, GSW, MODULE, GObject)

GswModuleInfo *gsw_module_get_info(GswModule *self);
const gchar   *gsw_module_get_name(GswModule *self);
gboolean       gsw_module_is_open(GswModule *self);
gboolean       gsw_module_is_supported(GswModule *self);

GswKey        *gsw_module_create_key(GswModule *self);       /* transfer full */
GswKey        *gsw_module_get_key(GswModule *self);          /* transfer none */
gboolean       gsw_module_set_key(GswModule *self, GswKey *key, GError **error);
gboolean       gsw_module_set_key_text(GswModule *self, const gchar *text, GError **error);
const gchar   *gsw_module_get_key_text(GswModule *self);

GBytes        *gsw_module_read_raw(GswModule *self, GError **error);   /* transfer full */
const gchar   *gsw_module_peek_raw(GswModule *self, GError **error);   /* 借出，直到下次调用 */
gchar         *gsw_module_get_text(GswModule *self, GError **error);   /* 纯文本，transfer full */
gchar         *gsw_module_get_render_text(GswModule *self, GError **error);

gboolean       gsw_module_next(GswModule *self, gint steps, GError **error);
gboolean       gsw_module_prev(GswModule *self, gint steps, GError **error);
gboolean       gsw_module_set_position(GswModule *self, GswPosition pos, GError **error);

gboolean       gsw_module_has_entry(GswModule *self);
gsize          gsw_module_get_entry_size(GswModule *self);
gboolean       gsw_module_is_writable(GswModule *self);

/**
 * gsw_module_search:
 * Returns: (transfer full) (array zero-terminated=1) (element-type utf8):
 *   matching key texts
 */
gchar **gsw_module_search(GswModule *self, const gchar *text,
                          GswSearchType type, GError **error);
```

错误语义：

- 未知/不支持的驱动：模块对象仍存在，`is_supported()` 为 `FALSE`，读取返回 `GSW_ERROR_UNSUPPORTED`。
- 数据文件缺失：`read_raw()` 返回 `GSW_ERROR_NOT_FOUND`；元数据仍可访问。
- 键越界：`set_key_text()` 返回 `FALSE` + `GSW_ERROR_OUT_OF_BOUNDS`，但键会 snap 到边界（与 SWORD 行为一致），`gsw_verse_key_is_valid()` 可用于区分。
- 不保留模块级 `last_error`，避免隐藏状态。

### 5.6 键

```c
/* GswKey: boxed base */
typedef struct _GswKey GswKey;

GType         gsw_key_get_type(void);
GswKey       *gsw_key_ref(GswKey *self);
void          gsw_key_unref(GswKey *self);
GswKey       *gsw_key_clone(GswKey *self);

const gchar  *gsw_key_get_text(GswKey *self);
gboolean      gsw_key_set_text(GswKey *self, const gchar *text);
gboolean      gsw_key_next(GswKey *self, gint steps);
gboolean      gsw_key_prev(GswKey *self, gint steps);
gint          gsw_key_compare(GswKey *self, GswKey *other);
gboolean      gsw_key_equals(GswKey *self, GswKey *other);
```

```c
/* GswVerseKey */
typedef struct _GswVerseKey GswVerseKey;

GType         gsw_verse_key_get_type(void);
GswVerseKey  *gsw_verse_key_new(const gchar *text, const gchar *versification);
GswVerseKey  *gsw_verse_key_new_with_bounds(const gchar *lower,
                                            const gchar *upper,
                                            const gchar *versification);

gint          gsw_verse_key_get_testament(GswVerseKey *self);
gint          gsw_verse_key_get_book(GswVerseKey *self);
gint          gsw_verse_key_get_chapter(GswVerseKey *self);
gint          gsw_verse_key_get_verse(GswVerseKey *self);

gboolean      gsw_verse_key_set_testament(GswVerseKey *self, gint testament);
gboolean      gsw_verse_key_set_book(GswVerseKey *self, gint book);
gboolean      gsw_verse_key_set_chapter(GswVerseKey *self, gint chapter);
gboolean      gsw_verse_key_set_verse(GswVerseKey *self, gint verse);

const gchar  *gsw_verse_key_get_book_name(GswVerseKey *self);
gboolean      gsw_verse_key_set_book_name(GswVerseKey *self, const gchar *name);
const gchar  *gsw_verse_key_get_versification(GswVerseKey *self);
gint          gsw_verse_key_get_chapter_max(GswVerseKey *self);
gint          gsw_verse_key_get_verse_max(GswVerseKey *self);

void          gsw_verse_key_set_auto_normalize(GswVerseKey *self, gboolean enabled);
gboolean      gsw_verse_key_get_auto_normalize(GswVerseKey *self);
void          gsw_verse_key_set_intros(GswVerseKey *self, gboolean enabled);
gboolean      gsw_verse_key_get_intros(GswVerseKey *self);
gboolean      gsw_verse_key_is_valid(GswVerseKey *self);

void          gsw_verse_key_set_lower_bound(GswVerseKey *self, GswVerseKey *bound);
void          gsw_verse_key_set_upper_bound(GswVerseKey *self, GswVerseKey *bound);
void          gsw_verse_key_clear_bounds(GswVerseKey *self);
```

```c
/* GswTreeKey */
typedef struct _GswTreeKey GswTreeKey;

GType         gsw_tree_key_get_type(void);
const gchar  *gsw_tree_key_get_text(GswTreeKey *self);
gboolean      gsw_tree_key_set_text(GswTreeKey *self, const gchar *path);

gboolean      gsw_tree_key_first_child(GswTreeKey *self);
gboolean      gsw_tree_key_next_sibling(GswTreeKey *self);
gboolean      gsw_tree_key_parent(GswTreeKey *self);
gboolean      gsw_tree_key_has_children(GswTreeKey *self);
gint          gsw_tree_key_get_level(GswTreeKey *self);
```

字典键：

```c
typedef struct _GswStrKey GswStrKey;
GType         gsw_str_key_get_type(void);
GswStrKey    *gsw_str_key_new(const gchar *text);
```

`GswModule::create_key()` 根据模块类型返回 `GswVerseKey` / `GswStrKey` / `GswTreeKey`，调用者用 `gsw_key_unref()` 释放。

### 5.7 经文系统

```c
#define GSW_TYPE_VERSIFICATION_MGR (gsw_versification_mgr_get_type())
G_DECLARE_FINAL_TYPE(GswVersificationMgr, gsw_versification_mgr, GSW,
                     VERSIFICATION_MGR, GObject)

GswVersificationMgr *gsw_versification_mgr_get_default(void);
GPtrArray           *gsw_versification_mgr_get_systems(GswVersificationMgr *self);
GswVersification    *gsw_versification_mgr_get_system(GswVersificationMgr *self,
                                                      const gchar *name);

const gchar      *gsw_versification_get_name(GswVersification *self);
gint              gsw_versification_get_book_count(GswVersification *self);
GswBook          *gsw_versification_get_book(GswVersification *self, gint number);
gint              gsw_versification_get_book_number_by_osis(GswVersification *self,
                                                            const gchar *osis);
gint              gsw_versification_get_nt_start_offset(GswVersification *self);

const gchar      *gsw_book_get_name(GswBook *self);
const gchar      *gsw_book_get_osis_name(GswBook *self);
const gchar      *gsw_book_get_preferred_abbreviation(GswBook *self);
gint              gsw_book_get_chapter_max(GswBook *self);
gint              gsw_book_get_verse_max(GswBook *self, gint chapter);
```

`GswVersification` 与 `GswBook` 为不可变 boxed，由 `GswVersificationMgr` 拥有；返回 transfer none。第一版内置 KJV，并从 `canon_*.h` 生成其余常用系统（Leningrad、MT、KJVA、NRSV、Synodal、Vulg、German、Luther、Catholic、LXX、Orthodox 等）。

### 5.7.1 `GswLocaleMgr`

```c
GswLocaleMgr *gsw_locale_mgr_get_default(void);
gboolean      gsw_locale_mgr_load_file(GswLocaleMgr *self, const gchar *path,
                                       GError **error);
guint         gsw_locale_mgr_load_directory(GswLocaleMgr *self,
                                            const gchar *path, GError **error);
const gchar  *gsw_locale_mgr_get_default_locale(GswLocaleMgr *self);
void          gsw_locale_mgr_set_default_locale(GswLocaleMgr *self, const gchar *name);
const gchar  *gsw_locale_mgr_translate(GswLocaleMgr *self, const gchar *text,
                                       const gchar *locale);
```

加载 SWORD `locales.d/*.conf` 的 `[Text]` 映射；`GswVerseKey` 的书名、缩写解析和显示会使用默认 locale。

### 5.8 `GswStatusReporter`

```c
#define GSW_TYPE_STATUS_REPORTER (gsw_status_reporter_get_type())
G_DECLARE_DERIVABLE_TYPE(GswStatusReporter, gsw_status_reporter, GSW,
                         STATUS_REPORTER, GObject)

struct _GswStatusReporterClass {
  GObjectClass parent_class;
  void (*pre_status)(GswStatusReporter *self, guint64 total_bytes,
                     guint64 completed_bytes, const gchar *message);
  void (*progress)(GswStatusReporter *self, guint64 total_bytes,
                   guint64 completed_bytes);
};

GswStatusReporter *gsw_status_reporter_new(void);
void gsw_status_reporter_set_main_context(GswStatusReporter *self,
                                          GMainContext *context);
void gsw_status_reporter_emit_pre_status(GswStatusReporter *self,
                                         guint64 total, guint64 completed,
                                         const gchar *message);
void gsw_status_reporter_emit_progress(GswStatusReporter *self,
                                       guint64 total, guint64 completed);
```

信号：

- `pre-status(guint64 total, guint64 completed, const gchar *message)`：开始一个新文件/阶段；`total`/`completed` 为**整体**累计值。
- `progress(guint64 total, guint64 completed)`：当前传输的字节进度；`total` 可能为 0（服务器未给长度）。
- `file-changed(const gchar *path)`：可选，便于 UI 显示当前文件。

如果设置了 `main-context`，信号通过 `g_main_context_invoke_full()` 派发到该上下文；否则在调用线程直接发出。这样调用者可以把安装放在工作线程，同时安全更新 UI。

取消：`GswInstallManager` 持有/接受 `GCancellable`；传输回调定期检查 `g_cancellable_is_cancelled()`。

### 5.9 `GswTransport`

```c
typedef struct _GswDirEntry GswDirEntry;   /* boxed: name, size, is_dir */

#define GSW_TYPE_TRANSPORT (gsw_transport_get_type())
G_DECLARE_DERIVABLE_TYPE(GswTransport, gsw_transport, GSW, TRANSPORT, GObject)

struct _GswTransportClass {
  GObjectClass parent_class;
  gboolean (*get)(GswTransport *self, const gchar *url, GOutputStream *out,
                  GCancellable *cancellable, GError **error);
  GPtrArray *(*list_dir)(GswTransport *self, const gchar *url,
                         GCancellable *cancellable, GError **error);
};

GswTransport *gsw_transport_new_curl(GswSourceType type,
                                     const gchar *host,
                                     GError **error);
void gsw_transport_set_credentials(GswTransport *self,
                                   const gchar *user, const gchar *password);
void gsw_transport_set_passive(GswTransport *self, gboolean passive);
void gsw_transport_set_timeout(GswTransport *self, guint timeout_msec);
void gsw_transport_set_verify_peer(GswTransport *self, gboolean verify);
```

v1 只提供内置 curl 实现；`GswTransport` 可派生，便于测试注入 fake transport。

### 5.10 `GswInstallSource`

```c
#define GSW_TYPE_INSTALL_SOURCE (gsw_install_source_get_type())
G_DECLARE_FINAL_TYPE(GswInstallSource, gsw_install_source, GSW,
                     INSTALL_SOURCE, GObject)

GswInstallSource *gsw_install_source_new(GswSourceType type,
                                         const gchar *caption,
                                         const gchar *host,
                                         const gchar *directory,
                                         const gchar *user,
                                         const gchar *password,
                                         const gchar *uid);
GswInstallSource *gsw_install_source_new_from_config(GswSourceType type,
                                                     const gchar *value,
                                                     GError **error);
gchar            *gsw_install_source_to_config(GswInstallSource *self);

GswSourceType  gsw_install_source_get_source_type(GswInstallSource *self);
const gchar   *gsw_install_source_get_caption(GswInstallSource *self);
const gchar   *gsw_install_source_get_host(GswInstallSource *self);
const gchar   *gsw_install_source_get_directory(GswInstallSource *self);
const gchar   *gsw_install_source_get_user(GswInstallSource *self);
const gchar   *gsw_install_source_get_password(GswInstallSource *self);
const gchar   *gsw_install_source_get_uid(GswInstallSource *self);
gboolean       gsw_install_source_is_refreshed(GswInstallSource *self);
```

`uid` 是本地 shadow 目录名（SWORD 里通常是时间戳），必须校验为安全文件名；`caption` 是用户可见名称。

### 5.11 `GswInstallManager`

```c
#define GSW_TYPE_INSTALL_MANAGER (gsw_install_manager_get_type())
G_DECLARE_FINAL_TYPE(GswInstallManager, gsw_install_manager, GSW,
                     INSTALL_MANAGER, GObject)

GswInstallManager *gsw_install_manager_new(const gchar *private_path,
                                           GError **error);

/* 构造时自动读取 <private_path>/InstallMgr.conf；load_config() 用于重新读取。 */

void gsw_install_manager_set_status_reporter(GswInstallManager *self,
                                             GswStatusReporter *reporter);
GswStatusReporter *gsw_install_manager_get_status_reporter(GswInstallManager *self);

void gsw_install_manager_set_cancellable(GswInstallManager *self,
                                         GCancellable *cancellable);
void gsw_install_manager_set_user_disclaimer_confirmed(GswInstallManager *self,
                                                       gboolean confirmed);
gboolean gsw_install_manager_is_user_disclaimer_confirmed(GswInstallManager *self);

void gsw_install_manager_set_passive_ftp(GswInstallManager *self, gboolean passive);
void gsw_install_manager_set_timeout(GswInstallManager *self, guint timeout_msec);
void gsw_install_manager_set_verify_peer(GswInstallManager *self, gboolean verify);

gboolean gsw_install_manager_load_config(GswInstallManager *self, GError **error);
gboolean gsw_install_manager_save_config(GswInstallManager *self, GError **error);

GPtrArray        *gsw_install_manager_get_sources(GswInstallManager *self);
GswInstallSource *gsw_install_manager_lookup_source(GswInstallManager *self,
                                                    const gchar *caption);
gboolean          gsw_install_manager_add_source(GswInstallManager *self,
                                                 GswInstallSource *source);
gboolean          gsw_install_manager_remove_source(GswInstallManager *self,
                                                    const gchar *caption);

gboolean gsw_install_manager_refresh_remote_source_configuration(
             GswInstallManager *self, GError **error);
gboolean gsw_install_manager_refresh_remote_source(
             GswInstallManager *self, GswInstallSource *source, GError **error);

GPtrArray     *gsw_install_manager_get_remote_module_infos(
                  GswInstallManager *self, GswInstallSource *source,
                  GError **error);
GswModuleInfo *gsw_install_manager_lookup_remote_module_info(
                  GswInstallManager *self, GswInstallSource *source,
                  const gchar *name, GError **error);

gboolean gsw_install_manager_install_module(GswInstallManager *self,
                                            GswManager *destination,
                                            GswInstallSource *source,
                                            const gchar *module_name,
                                            GError **error);
gboolean gsw_install_manager_remove_module(GswInstallManager *self,
                                           GswManager *destination,
                                           const gchar *module_name,
                                           GError **error);

/* GTask 异步变体；finish 返回布尔结果。 */
void     gsw_install_manager_refresh_remote_source_async(...);
gboolean gsw_install_manager_refresh_remote_source_finish(...);
void     gsw_install_manager_install_module_async(...);
gboolean gsw_install_manager_install_module_finish(...);
void     gsw_install_manager_remove_module_async(...);
gboolean gsw_install_manager_remove_module_finish(...);

guint gsw_install_manager_get_module_status(GswManager *local,
                                            GswModuleInfo *remote);
```

安装完成后：

- `destination` 自动 `gsw_manager_reload()`。
- 失败/取消时清理临时目录，不写 conf。
- 成功时 conf 最后写入，作为“提交标记”。

### 5.12 使用示例

读取本地模块：

```c
g_autoptr(GError) error = NULL;
g_autoptr(GswManager) mgr = gsw_manager_new_for_path("/home/me/.sword", &error);
if (!mgr) { g_printerr("%s\n", error->message); return 1; }

GswModule *mod = gsw_manager_get_module(mgr, "KJV");
if (!mod) { /* 列出 gsw_manager_get_module_infos() */ }

gsw_module_set_key_text(mod, "John 3:16", &error);
g_print("%s\n", gsw_module_get_text(mod, &error));
```

安装远端模块：

```c
g_autoptr(GError) error = NULL;
g_autoptr(GswManager) mgr = gsw_manager_new_for_path("/home/me/.sword", &error);
g_autoptr(GswInstallManager) im =
    gsw_install_manager_new("/home/me/.sword/InstallMgr", &error);

g_autoptr(GswStatusReporter) reporter = gsw_status_reporter_new();
g_signal_connect(reporter, "progress",
                 G_CALLBACK(on_progress), NULL);
gsw_install_manager_set_status_reporter(im, reporter);
gsw_install_manager_set_user_disclaimer_confirmed(im, TRUE);

gsw_install_manager_refresh_remote_source_configuration(im, &error);
GswInstallSource *src = gsw_install_manager_lookup_source(im, "CrossWire");
gsw_install_manager_refresh_remote_source(im, src, &error);

g_autoptr(GPtrArray) infos =
    gsw_install_manager_get_remote_module_infos(im, src, &error);
for (guint i = 0; i < infos->len; i++) {
  GswModuleInfo *info = g_ptr_array_index(infos, i);
  g_print("%s: %s\n",
          gsw_module_info_get_name(info),
          gsw_module_info_get_description(info));
}

gsw_install_manager_install_module(im, mgr, src, "KJV", &error);
```

---

## 6. 核心流程

### 6.1 本地库发现与加载

```
GswManager::load()
  1. 解析 prefix/config path（见 5.4）
  2. 读取 mods.d/*.conf -> 合并 GswConfig
  3. 读取 sword.conf 的 AugmentPath，按顺序 augment
  4. 遍历 sections，构造 GswModuleInfo，加入 name -> info 索引
  5. 记录每个 section 来自哪个 conf 文件（安装/卸载需要）
  6. 不打开数据文件
```

冲突策略：

- 同名 section：后加载的覆盖先加载（与 SWORD augment 行为一致）。
- 多模块模式下可保留多个副本，v1 不做，后加载覆盖。
- `reload()` 会使已缓存的 `GswModule` 后端失效：关闭文件句柄，重新构造元数据；调用者持有的 `GswModuleInfo` 快照仍有效，但 `GswModule` 的当前键/缓存条目不再保证有效。

### 6.2 远端源刷新

```
refresh_remote_source(source)
  1. 校验 source、uid、directory
  2. 在 <private>/<uid>.tmp-XXXX 建立 staging
  3. 尝试 GET <source>/<directory>/mods.d.tar.gz
     - 成功：校验 gzip/tar，安全解包到 staging
     - 失败且不是连接错误/取消：回退到列目录
  4. 回退：GET <source>/<directory>/mods.d/ 列表
     - 只接受 *.conf；逐个下载到 staging/mods.d/
  5. 校验 staging/mods.d 至少有一个 conf
  6. 原子替换 <private>/<uid>
  7. 构造 metadata-only GswManager，缓存到 source
```

进度：

- 若用 tar.gz：只有一个文件，`progress` 表示该文件下载进度；解包阶段可发 `pre-status`。
- 若列目录：先列目录得到总大小，再逐文件下载；`pre-status` 给出整体累计值。

### 6.3 读取远端模块列表

```
get_remote_module_infos(source)
  -> 确保 source 已 refresh（或返回 GSW_ERROR_NOT_FOUND）
  -> source->shadow_mgr->module_infos
  -> 返回 GPtrArray（元素 borrowed）
```

因为元数据来自 shadow 库的 `mods.d`，不需要数据文件。

### 6.4 安装模块

```
install_module(destination, source, name)
  1. 校验 disclaimer、source 已刷新、目标库可写
  2. 从 source shadow 找到 GswModuleInfo + conf 文件
  3. 若 info.is_ciphered 且无 CipherKey：
       返回 GSW_ERROR_CIPHERED；有密钥时安装照常，读取时执行 Sapphire 解密
  4. 解析 AbsoluteDataPath / DataPath / File= 列表
  5. 计算需要下载的相对目录或文件集合
  6. 在目标 prefix 下创建临时目录 <prefix>/.gsw-tmp-XXXX
  7. 逐文件下载到临时目录（保持相对路径）
     - 每次写入前校验路径在临时目录内
     - 计算总大小（listing / InstallSize）
     - 发 pre-status / progress
  8. 校验下载完整性（大小；可选 hash）
  9. 将临时目录内容 rename/移动到最终位置
     - 已存在同名文件：按策略覆盖（v1 覆盖；可选备份）
 10. 写 <prefix>/mods.d/<name>.conf（最后写）
 11. destination->reload()
 12. 清理临时目录
```

`File=` 兼容规则：

- 上游把 `File=` 解释为相对仓库根（`<prefix>/<File>`）而不是相对 `DataPath`；同时远端下载 URL 为 `<source>/<directory>/<File>`。
- 为兼容旧模块，本项目沿用上游语义，但在文档和代码中明确。
- 新模块通常没有 `File=`，走 `DataPath` 目录复制。

`DataPath` 规则：

- 必须是相对路径；拒绝绝对路径和 `..`。
- 文本/注释：`DataPath` 是数据目录。
- `RawLD`/`RawLD4`/`zLD`/`RawGenBook`：`DataPath` 的最后一个路径段是文件 basename；`AbsoluteDataPath` 是去掉 basename 的目录。
- **安装按目录进行**：使用 `AbsoluteDataPath` 相对 `<prefix>` 的部分作为远端目录路径，递归下载整个目录。对字典/通用书，这个目录是包含 `<base>.idx`、`<base>.dat`、`<base>.bdt` 等的父目录；对文本模块就是 `DataPath` 目录。上游 `installModule` 即如此，而不是只下载 basename 对应的几个文件。
- 目录中可能包含无关文件（例如同目录多个模块）；v1 按上游行为整体复制，并在卸载时按 conf 记录处理。

### 6.5 卸载与更新

```
remove_module(destination, name)
  1. 从 destination config 找到 section 和 conf 文件
  2. 关闭对应的 GswModule（释放文件句柄）
  3. 若存在 File=：删除每个 <prefix>/<File>
     否则：删除 AbsoluteDataPath 目录（校验在 prefix 内）
  4. 删除 mods.d/<name>.conf
  5. reload
```

更新：

- 用 `gsw_install_manager_get_module_status()` 比较远端与本地版本。
- 安装流程覆盖旧文件；旧模块多余文件不自动删除（与 SWORD 一致），可在后续版本增加“按清单清理”。

### 6.6 读取条目

```
gsw_module_set_key_text(mod, "John 3:16")
  -> backend->set_key_text(key)
  -> key 可能 snap 到最近条目

gsw_module_read_raw(mod)
  -> backend->open()（首次）
  -> backend->read_entry(key)
  -> 原始字节（可能带标记/压缩已解、未过渲染过滤器）
  -> 编码转换到 UTF-8
  -> 基础 strip（get_text）
```

---

## 7. 网络与进度

### 7.1 libcurl 配置

- 每个传输使用独立 `CURL*` easy handle；安装器串行下载。
- 选项：
  - `CURLOPT_URL`
  - `CURLOPT_FOLLOWLOCATION`（限制重定向次数）
  - `CURLOPT_CONNECTTIMEOUT_MS` / `CURLOPT_TIMEOUT_MS`（默认 10s/可配置）
  - `CURLOPT_LOW_SPEED_LIMIT` / `CURLOPT_LOW_SPEED_TIME` 防止挂死
  - `CURLOPT_FAILONERROR`
  - `CURLOPT_NOSIGNAL`（线程安全）
  - `CURLOPT_USERAGENT`（`gsword/<version>`）
  - `CURLOPT_SSL_VERIFYPEER` / `CURLOPT_SSL_VERIFYHOST`（默认开启；显式选项可关闭）
  - FTP：`CURLOPT_FTP_USE_EPSV`、被动/主动模式
  - `CURLOPT_WRITEFUNCTION` 写 `GOutputStream`
  - `CURLOPT_XFERINFOFUNCTION` 进度/取消
- 对于 `file://`（本地测试）也可支持。
- 凭据：`user:password`；默认 `ftp` / `installmgr@user.com`（与 SWORD 兼容）。
- URL 构造：统一去掉重复 `/`，保留目录语义；不把用户输入直接拼进 URL 而不做检查。

### 7.2 目录列表

- FTP：
  - 优先 `MLSD`（`CURLOPT_CUSTOMREQUEST "MLSD"`），解析 `type=file/dir;size=...; name`。
  - 回退 `LIST`，移植/重写 `ftpparse` 风格的 Unix/DOS 解析。
  - 忽略 `.`、`..`。
- HTTP：
  - 优先 `mods.d.tar.gz`，避免 HTML 解析。
  - 回退解析 Apache/nginx autoindex：
    - 提取 `<a href="...">`；
    - 忽略查询串、父目录、绝对 URL；
    - 从同一行/相邻 `<td>` 提取大小（可选）。
  - 如果服务器提供 `mods.d.tar.gz` 但损坏，回退到目录列表。
- 失败语义：连接错误/超时/取消返回不同错误码，便于 UI 决定是否回退（与上游 `-1/-2/-3` 类似，但使用 `GError` code）。

### 7.3 进度与取消

- 总大小：
  - 单文件：`Content-Length` / FTP SIZE。
  - 目录：先列目录求和。
  - 未知：`total = 0`，UI 显示不确定进度。
- 信号节流：默认 100ms 一次，避免回调风暴；文件开始/结束强制发。
- 取消：
  - `GCancellable` 作为安装器属性。
  - curl 的 `XFERINFOFUNCTION` 每 callback 检查取消。
  - 解包/复制循环也定期检查。
  - 取消后清理临时目录，返回 `GSW_ERROR_CANCELLED`。

### 7.4 重试

v1 不做自动重试；可选的单文件重试次数作为后续选项。TLS 校验默认开启，若用户显式关闭则记录警告。

---

## 8. 模块驱动

### 8.1 后端接口

内部接口（不安装）：

```c
/* src/modules/gsw-module-backend.h */
typedef struct _GswModuleBackend GswModuleBackend;
typedef struct _GswModuleBackendClass GswModuleBackendClass;

struct _GswModuleBackendClass {
  GObjectClass parent_class;

  gboolean (*open)(GswModuleBackend *self, GError **error);
  void     (*close)(GswModuleBackend *self);

  GswKey  *(*create_key)(GswModuleBackend *self);
  gboolean (*set_key)(GswModuleBackend *self, GswKey *key, GError **error);

  GBytes  *(*read_entry)(GswModuleBackend *self, GError **error);
  gboolean (*has_entry)(GswModuleBackend *self);
  gsize    (*get_entry_size)(GswModuleBackend *self);

  gboolean (*next)(GswModuleBackend *self, gint steps, GError **error);
  gboolean (*prev)(GswModuleBackend *self, gint steps, GError **error);
  gboolean (*set_position)(GswModuleBackend *self, GswPosition pos, GError **error);
};
```

`GswModule` 持有 `GswModuleBackend *`，对公共 API 做统一包装：

- 懒 `open`
- 编码转换
- `GBytes` 缓存/借出
- 错误转换
- 线程/重入检查（简单 `GMutex` 或文档约束）

后端实现共享：

- `GswRawVerse`：RawText/RawCom 索引读取。
- `GswZVerse`：zText/zCom 索引 + 块缓存。
- `GswRawStr`：RawLD。
- `GswZStr`：zLD。
- `GswRawGenBook`：TreeKeyIdx。

### 8.2 驱动映射

| `ModDrv` | 键类型 | 后端 | 说明 |
|---|---|---|---|
| `RawText` | VerseKey | RawVerse 6B | 未压缩经文 |
| `RawText4` | VerseKey | RawVerse 8B | 4 字节大小 |
| `zText` | VerseKey | ZVerse 10B/12B | 压缩经文 |
| `zText4` | VerseKey | ZVerse 12B/12B | 压缩经文 4B |
| `RawCom` | VerseKey | RawVerse 6B | 注释 |
| `RawCom4` | VerseKey | RawVerse 8B | 注释 4B |
| `zCom` | VerseKey | ZVerse 10B/12B | 压缩注释 |
| `zCom4` | VerseKey | ZVerse 12B/12B | 压缩注释 4B |
| `RawLD` | StrKey | RawStr 6B | 字典 |
| `RawLD4` | StrKey | RawStr 8B | 字典 4B |
| `zLD` | StrKey | ZStr | 压缩字典 |
| `RawGenBook` | TreeKey | RawGenBook | 通用书 |
| `RawFiles` | VerseKey | 已支持 | 每条目一个文件 |
| `HREFCom` | VerseKey | 已支持 | Prefix + 条目 URL |

`zCom` 与 `zText` 的区别只在语义/类型，存储格式相同；`GswModuleInfo` 的 `Type` 决定 `GswModuleType`。

### 8.2.1 加密与渲染选项

- 加密：`CipherKey` 存在时，对 zVerse 压缩块、zLD 块、RawVerse/RawLD/RawGenBook 条目执行 Sapphire 解密。
- 渲染：`gsw_module_set_option()` 支持 `Strong's Numbers`、`Footnotes`、`Headings`、`Red Letter Words`、`Morphology`、`Variants`、`Glosses`；`gsw_module_get_render_text()` 返回选项处理后的纯文本。

### 8.3 编码与文本

- 内部统一 UTF-8。
- `Encoding`：
  - `UTF-8`：原样。
  - `Latin-1`/空：`g_convert`。
  - `UTF-16`：`g_convert`。
  - `SCSU`：内置 SCSU -> UTF-8 解码。
- 原始条目中的 `\0`：按字节读取，不依赖 `strlen`；返回 `GBytes` 保证二进制安全。
- `get_text`：
  - 去除 XML/HTML 标签（OSIS/ThML/GBF/HTML）。
  - 解码常见实体（`&amp;`、`&#...;` 等）。
  - 处理 `\par`、多余空白。
  - 这不是完整渲染，只是“可搜索/可显示”的纯文本。

### 8.4 不支持项的处理

- 未知驱动：`GswModuleInfo::is_supported = FALSE`；`GswModule` 可创建但读取报 `GSW_ERROR_UNSUPPORTED`。
- 加密：`CipherKey` 存在时标记 `is_ciphered`；有密钥时执行 Sapphire 解密，无密钥时报 `GSW_ERROR_CIPHERED`。
- 缺失数据文件：读取报 `GSW_ERROR_NOT_FOUND`，不崩溃。
- 格式损坏：报 `GSW_ERROR_INVALID_DATA`，附带文件偏移（便于诊断）。

---

## 9. 配置、路径与安全

### 9.1 路径规则

不可信输入：

- 远端 `DataPath`、`File=`、tar 成员、目录项名称。
- `InstallMgr.conf` 的 `uid`、`directory`。

规则：

1. 所有相对路径必须是“安全的相对路径”：
   - 非空、无 NUL；
   - 不是绝对路径（Unix `/`、Windows `X:\`、UNC）；
   - 规范化后不包含 `..`；
   - 不包含 Windows 保留字符（可选，跨平台时）。
2. 拼接后必须仍在允许根目录内：
   ```c
   gchar *canonical = g_canonicalize_filename(joined, NULL);
   if (!g_str_has_prefix(canonical, root_with_slash)) reject;
   ```
   注意 `g_canonicalize_filename` 不解析符号链接；安装/解包时对最终路径用 `realpath()` 或 `g_file_read_link` 检查。
3. `uid` 只允许 `[A-Za-z0-9._-]`，不能以 `.` 开头（避免 `.`/`..`/隐藏冲突）。
4. 写入前确保父目录存在，且不是符号链接（或解析后仍在根内）。
5. 不跟随模块数据中的符号链接。

### 9.2 解包安全

`mods.d.tar.gz` 解包：

- gzip 解压使用 zlib streaming。
- tar 只接受：
  - 普通文件（`'0'`、`'\0'`）；
  - 目录（`'5'`）。
- 拒绝：符号链接、硬链接、设备、FIFO、绝对路径、`..`、超出 staging 的路径。
- 限制：
  - 单文件最大（如 64 MiB，可配置）；
  - 总解压大小（如 512 MiB，可配置）；
  - 成员数量（如 100k）；
  - 压缩比异常检测。
- 解包后校验 `mods.d/` 存在且至少一个 `.conf`。

### 9.3 原子安装

- 下载到 `<prefix>/.gsw-tmp-<pid>-<random>/`。
- 所有文件写完后 `fsync`（可选），再 `rename()` 到最终位置。
- 跨文件系统时 `rename` 可能失败，回退到“复制 + 删除临时”。
- conf 最后写；如果 conf 写失败，回滚已移动的文件（可选，v1 至少不写 conf）。
- 失败/取消：递归删除临时目录。
- 覆盖升级：先移动到临时备份目录，成功后删除；失败可恢复（v1 可简化，但需文档说明）。

### 9.4 版本比较

实现 `gsw_version_compare()`，兼容 SWORD `SWVersion`：

- 按 `.` 分割，最多 4 段，缺失段视为 `-1`。
- 非数字段 `atoi` 行为（与上游一致）。
- 返回 `<0`、`0`、`>0`。
- 用于 `NEW/UPDATED/OLDER/SAME_VERSION` 判定。

---

## 10. 错误、日志、线程与内存

### 10.1 错误

- 所有公共 API 使用 `GError`。
- 错误 domain `GSW_ERROR`，code 见 5.1。
- 错误消息包含：操作、路径/URL、底层错误（curl/zlib/errno）。
- 不使用全局 `last error`；避免上游 `popError()` 的隐式状态。

### 10.2 日志

- 使用 `g_log`/`g_debug`，domain `"gsword"`。
- 可通过 `G_MESSAGES_DEBUG=gsword` 开启调试。
- 不在库中直接 `printf`/`std::cout`。
- 敏感信息（密码）不写日志。

### 10.3 线程

- `GswManager`、`GswModule`、`GswConfig` 默认**非线程安全**；一个实例同一时间只被一个线程使用。
- `GswInstallManager` 非线程安全；一次只允许一个安装/刷新操作。
- 推荐模式：工作线程执行安装；通过 `GswStatusReporter` 的 `main-context` 把信号派发到 UI 线程。
- 如需并发读取多个模块，创建多个 `GswManager` 或按模块分线程；后续可增加内部锁。
- 不使用全局可变状态（versification 默认管理器除外，初始化后只读）。

### 10.4 内存

- 所有 `gchar*` 返回值明确 transfer full；`const gchar*` 为 borrow。
- `GBytes` 用于二进制安全的条目数据。
- 模块块缓存有上限（例如每个模块缓存 1 个 zText 块，zLD 缓存 1 个 EntriesBlock）；后续可配置 LRU。
- 避免在热路径分配 GObject；键为 boxed。

---

## 11. 构建、安装与绑定

### 11.1 Meson

```meson
project('gsword', 'c',
  version: '0.1.0',
  license: 'GPL-2.0-or-later',
  default_options: ['c_std=c11', 'warning_level=2'])

glib_dep = dependency('glib-2.0', version: '>=2.66')
gobject_dep = dependency('gobject-2.0')
gio_dep = dependency('gio-2.0')
curl_dep = dependency('libcurl', version: '>=7.68')
zlib_dep = dependency('zlib')
# optional: bzip2, liblzma
```

选项：

- `-Dcurl=enabled|disabled`（v1 必需，保留选项）
- `-Dbzip2=auto|enabled|disabled`
- `-Dlzma=auto|enabled|disabled`
- `-Dintrospection=true|false`
- `-Dtests=true|false`
- `-Dgtk_doc=true|false`（gtk-doc API reference）

### 11.2 安装产物

- `libgsword.so.0`
- `include/gsword/*.h`
- `gsword.pc`
- 可选：`Gsword-0.1.gir`、`Gsword-0.1.typelib`
- 可选：示例程序 `gsword-lookup`、`gsword-install`

### 11.2.1 gtk-doc

- `docs/reference/`：`gsword-docs.xml`、`gsword-overview.xml`、`gsword-sections.txt`、`gsword.types`。
- `-Dgtk_doc=true` 启用，`meson compile -C build docs` 构建。
- 安装到 `share/gtk-doc/html/gsword`。

### 11.3 GI 注解

安装/生成 GIR：

```meson
gir = gnome.generate_gir(gsword_lib,
  sources: gsword_sources + gsword_headers,
  namespace: 'Gsw', nsversion: '0.1',
  identifier_prefix: 'Gsw', symbol_prefix: 'gsw_',
  includes: ['GObject-2.0', 'Gio-2.0'],
  install: true)
```

使用 `-Dintrospection=true` 启用；GIR 安装在 `share/gir-1.0`，typelib 安装在 `lib/girepository-1.0`。


示例：

```c
/**
 * gsw_manager_get_module_infos:
 * @self: a #GswManager
 *
 * Returns: (transfer container) (element-type GswModuleInfo):
 *   the module metadata list
 */
```

注意 `GswKey` boxed 类型的 `(transfer none)`/`(transfer full)` 注解。

---

## 12. 测试策略

### 12.1 单元测试

- `GswConfig`：
  - BOM、CRLF、注释、空行、重复 key、值中含 `=`、空 section。
  - 保存/重新读取 round-trip。
  - augment 覆盖/多值。
- 路径安全：
  - `../`、绝对路径、Windows 盘符、符号链接、前缀攻击（`/root2` vs `/root`）。
- `gsw_version_compare`。
- `GswVerseKey`：
  - `Gen 1:1`、`1Sam1.20`、`Jn.3.16`、`Ps 119:176`、范围/列表。
  - 边界 snap、越界、intros、autonormalize。
  - 与上游 `verse` 解析结果对比（golden）。
- 压缩：
  - LZSS 解码已知向量（由上游生成）；
  - LZSS 编码→解码 round-trip；
  - ZIP/BZIP2/XZ 解压已知数据；
  - 损坏数据必须报错而不是崩溃。
- `EntriesBlock`：构造、读取、越界、删除/压缩 round-trip。
- 驱动：
  - 合成 RawText/RawText4/RawCom/zText/zText4/RawLD/RawLD4/zLD/RawGenBook 模块，读取每个条目并比对。
  - 缺失文件、截断索引、非法大小、链接条目。
- 解包：
  - 正常 tar.gz；
  - `../`、绝对路径、符号链接、超大文件、截断归档必须拒绝。

### 12.2 集成测试

- 本地 HTTP 服务器（Python `http.server` 或测试内嵌 GIO）提供 fixture 仓库：
  - `mods.d.tar.gz`
  - `mods.d/*.conf`
  - 小模块数据
- 测试：
  - `refresh_remote_source` 成功/回退/取消。
  - 远端模块列表数量与字段。
  - 安装到临时 prefix，读取条目。
  - 卸载。
  - 进度信号顺序与单调性（总大小、累计值）。
- FTP 测试可选（CI 中用 `pyftpdlib`），避免默认依赖外部网络。

### 12.3 兼容性测试

- 使用真实的 `~/.sword` 模块（KJV、ChiUns2、GerAugustinus）做可选集成测试。
- 与上游 `diatheke` 输出对比（如果环境有 `libsword`）：
  - 经文文本、键 snap、条目大小。
- 生成 golden 文件并纳入仓库，避免 CI 依赖 libsword。

### 12.4 健壮性

- 在 CI 中启用 ASan/UBSan。
- 对配置解析、tar 解包、键解析、压缩解码做 fuzz（libFuzzer 或 AFL）。
- 所有网络/文件错误路径必须有测试。

---

## 13. 里程碑

### M0：设计与骨架

- 完成三份文档（本文件、`formats.md`、`upstream-analysis.md`）。
- 建立 Meson 工程、公共头文件骨架、空实现。
- 验收：能编译出 `libgsword.so`，示例可链接。

### M1：配置与元数据

- `GswConfig`、`GswModuleInfo`、`GswManager`（metadata-only）。
- 本地 `mods.d` 发现、augment、模块列表。
- 单元测试覆盖配置解析和模块元数据。
- 验收：能列出本机 `~/.sword` 所有模块的名称/类型/版本，不打开数据文件。

### M2：网络与安装

- `GswTransport`（curl HTTP/HTTPS/FTP）、目录列表、tar.gz 安全解包。
- `GswStatusReporter` 信号、取消。
- `GswInstallSource`、`GswInstallManager`：刷新源、列远端模块、安装、卸载。
- 集成测试使用本地 HTTP 仓库。
- 验收：从 fixture 仓库安装一个模块到临时目录，进度回调正确，可取消，安装后可列出。

### M3：经文键、RawText/zText

- `GswVersificationMgr` + KJV 表（后续生成其他系统）。
- `GswVerseKey` 解析/规范化/边界。
- `RawText`/`RawText4`、`RawCom`/`RawCom4` 读取。
- LZSS/ZIP 解码，`zText`/`zText4`、`zCom`/`zCom4` 读取。
- 验收：读取 KJV/ChiUns2 的指定经文，与 diatheke/上游输出一致。

### M4：字典、通用书、文本处理

- `RawLD`/`RawLD4`/`zLD` + `EntriesBlock` + BZIP2/XZ。
- `RawGenBook` + `GswTreeKey`。
- `GswStrKey`。
- 基础 OSIS/ThML/GBF strip、Latin-1/UTF-16 转 UTF-8。
- 验收：读取 Strong's 字典、GerAugustinus 通用书、中文模块。

### M5：打磨与扩展

- GIR/typelib、文档、示例。
- 异步包装（GTask）。
- 已实现：Sapphire 解密、渲染选项、搜索、SCSU、RawFiles/HREFCom。
- 性能优化（块缓存、目录扫描）。

---

## 14. 风险与未决问题

### 14.1 风险

| 风险 | 影响 | 缓解 |
|---|---|---|
| zText/zVerse 格式细节多（10/12 字节、额外哨兵、块缓存） | 读取错误 | 严格按 `formats.md`；用上游生成 golden；真实模块测试 |
| LZSS 位流实现偏差 | 解压乱码 | 移植算法并做上游向量对比；round-trip + fuzz |
| versification 表庞大/易错 | 经文偏移错误 | 从 `canon_*.h` 生成；用上游 `refsystest` 对照 |
| FTP 目录列表格式差异 | 刷新失败 | 优先 mods.d.tar.gz；MLSD + LIST 回退；测试多种服务器 |
| HTTP 目录列表脆弱 | 刷新失败 | 优先 tar.gz；解析器容错；记录错误 |
| 加密模块 | 缺密钥时无法读取 | 已实现 Sapphire 解密；缺密钥时明确报错 |
| 过滤器范围蔓延 | 工期 | v1 只做基础 strip；渲染过滤器后置 |
| 安全漏洞（路径/解包） | 严重 | 专门工具函数 + 测试 + fuzz |
| 旧 gsword 命名冲突 | 安装冲突 | 评审库名/包名；必要时 `gsword-2` |

### 14.2 已确认的决策

1. **v1 范围**：全量目标——下载 + 安装 + 元数据 + 读取经文（RawText/zText/RawLD/zLD/RawGenBook）。
2. **依赖策略**：libcurl、zlib 为必需；libbz2、liblzma 为可选依赖（缺失时对应模块标记为不支持）。
3. **许可证**：GPL-2.0-or-later。
4. **命名**：保留 `Gsw` 前缀与 `libgsword` 库名；不保证与旧 C++ wrap 的 ABI 兼容。

### 14.3 仍待确认的细节

1. **最低平台**：GLib 2.66、libcurl 7.68 是否满足目标发行版？
2. **加密模块**：已实现 Sapphire 解密；缺密钥时报 `GSW_ERROR_CIPHERED`。
3. **渲染**：v1 的 `get_text` 只做基础去标签是否足够，还是必须支持 Strong's/脚注等选项？（设计默认基础去标签。）
4. **安装冲突策略**：升级时是否保留旧文件备份（设计默认直接覆盖，失败清理临时目录）。

---

## 15. 附录：上游到本项目的映射

| 上游 | 本项目 | 备注 |
|---|---|---|
| `SWMgr::findConfig` | `gsw_manager_new*` + 内部发现函数 | 保留 SWORD_PATH/sword.conf |
| `SWMgr::loadConfigDir` | `gsw_manager_load` | 合并 conf |
| `SWMgr::createAllModules` | `GswModuleInfo` 索引 + 懒后端 | 元数据不打开数据 |
| `SWMgr::createModule` | `gsw_module_backend_new` | 驱动映射 |
| `SWModule::setKey` | `gsw_module_set_key` | 返回 GError |
| `SWModule::getRawEntry` | `gsw_module_read_raw` | GBytes |
| `SWModule::renderText` | `gsw_module_get_render_text` | 支持基础选项过滤 |
| `SWConfig` | `GswConfig` | GObject |
| `VerseKey` | `GswVerseKey` | boxed |
| `StrKey` | `GswStrKey` | boxed |
| `TreeKeyIdx` | `GswTreeKey` | boxed |
| `VersificationMgr` | `GswVersificationMgr` | 只读 |
| `InstallMgr` | `GswInstallManager` | GObject |
| `InstallSource` | `GswInstallSource` | GObject |
| `RemoteTransport` | `GswTransport` | derivable |
| `StatusReporter` | `GswStatusReporter` | 信号 |
| `SWCompress` | `GswCompressor`（内部） | 不公开 |

---

## 16. 参考

- 上游源码：`sword-1.9.0/`
- 格式细节：`docs/formats.md`
- 上游分析：`docs/upstream-analysis.md`
- SWORD API 文档：<https://crosswire.org/sword/develop/swordapi/>
- CrossWire 模块仓库：<https://www.crosswire.org/sword/modules/>

# SWORD 模块格式说明（供 libsword 实现使用）

- 状态：设计草案 v0.1
- 日期：2026-10-28
- 目的：以字节级方式固定本项目需要兼容的 SWORD 1.9 存储格式，作为实现与测试依据。
- 参考源码：`sword-1.9.0/src/modules/`、`sword-1.9.0/src/keys/`、`sword-1.9.0/src/mgr/`

> 约定：除特别说明外，所有整数均为**小端序**（SWORD 在 x86 上直接写本机字节序；实际发布的模块都是小端）。读取时应使用显式小端解码，不要依赖主机字节序。字符串在索引/配置中一般为 UTF-8，具体由模块 `Encoding=` 决定。

---

## 1. 配置（INI）格式

SWORD 的 `mods.d/*.conf`、`InstallMgr.conf`、`sword.conf` 都使用同一种简单 INI 格式：

```ini
# 注释（行首）
[SectionName]
Key=Value
RepeatedKey=One
RepeatedKey=Two
```

规则（以 `SWConfig::load` 为准，本项目做少量安全增强）：

- 行首 UTF-8 BOM（`EF BB BF`）忽略。
- 以 `#` 开头的行是注释；本项目允许前导空白后再 `#`（比上游宽松）。
- `[name]` 开始新 section；section 名是括号内第一个 `]` 之前的文本（去掉首尾空白）。
- `Key=Value` 在**第一个** `=` 处分割；key 和 value 都去掉首尾空白。
- 没有 `=` 的非空行：忽略（或报错，v1 选择忽略并警告）。
- 同一个 section 内同一个 key 可以出现多次；读取时返回全部值。
- 空行忽略。
- 不支持续行、转义、引号；值可以包含 `=`、`|`、`:` 等。
- section/key 大小写敏感。
- 保存时每个 section 前输出一个空行，然后 `[name]`，每行 `key=value`，文件末尾换行。

### 1.1 配置合并（augment）

- 同名 section 合并。
- 同名 key：如果目标已有该 key 且只有一个值，则覆盖；如果有多值，则按“不重复追加”处理。实现时保持与上游 `SWConfig::augment` 等价即可。
- 后加载的配置覆盖先加载的标量值；多值保留。

---

## 2. 模块配置字段

模块 section 名即模块内部名（例如 `[KJV]`）。常用字段：

| 字段 | 说明 |
|---|---|
| `Description` | 默认描述；`Description_<lang>` 为本地化描述 |
| `Lang` | 语言代码（`en`、`zh_CN` 等） |
| `ModDrv` | 驱动名，见下表 |
| `DataPath` | 相对 `<prefix>` 的数据路径；对字典/通用书通常包含文件 basename |
| `Encoding` | `UTF-8`、`Latin-1`、`UTF-16`、`SCSU`；缺省按 Latin-1 |
| `Direction` | `LTR`、`RTL`、`BIDI` |
| `SourceType` | `OSIS`、`ThML`、`GBF`、`TEI`、`Plain` 等 |
| `Versification` | 经文系统名，默认 `KJV` |
| `BlockType` | `VERSE`、`CHAPTER`、`BOOK`；zText/zCom 默认 `CHAPTER` |
| `CompressType` | `LZSS`（默认）、`ZIP`、`BZIP2`、`XZ` |
| `BlockCount` | zLD 每个压缩块的条目数，默认 200 |
| `CaseSensitiveKeys` | `true`/`false`，字典键是否区分大小写，默认 `false` |
| `StrongsPadding` | `true`/`false`，字典键是否做 Strong's 补零，默认 `true` |
| `KeyType` | `RawGenBook` 用，默认 `TreeKey`；`VerseKey` 为特殊变体 |
| `CipherKey` | 加密模块的密钥；存在即视为加密 |
| `Version` | 模块版本，用于更新比较 |
| `MinimumVersion` | 所需 SWORD 最低版本 |
| `InstallSize` | 安装后大小（字节），用于进度估计 |
| `Type` | 模块类型字符串（`Biblical Texts`、`Commentaries`、`Lexicons / Dictionaries`、`Generic Books`、`Daily Devotional` 等） |
| `GlobalOptionFilter` | 全局过滤器名，可重复 |
| `LocalOptionFilter` | 局部过滤器名，可重复 |
| `LocalStripFilter` | 局部 strip 过滤器，可重复 |
| `Feature` | 特性标记，可重复 |
| `DisplayLevel` | 显示层级 |
| `Obsoletes` | 被本模块取代的模块 |
| `CopyrightHolder` / `CopyrightNotes` / `CopyrightContactName` / `CopyrightContactEmail` | 版权信息 |
| `DistributionLicense` | 分发许可 |
| `About` | 详细说明，可能含 `\par` |
| `TextSource` | 文本来源 |
| `SwordVersionDate` | 模块打包日期 |
| `History_<version>` | 版本历史，可重复 |
| `LCSH` | 主题词 |

由 `SWMgr::createModule` 在运行时补充（不写回文件）：

- `PrefixPath`：模块库前缀，末尾带 `/`。
- `AbsoluteDataPath`：`PrefixPath + DataPath` 的绝对路径；对 `RawLD`/`RawLD4`/`zLD`/`RawGenBook`，再去掉最后一个路径段（文件名 basename），保留目录。

驱动名映射：

| `ModDrv` | 数据组织 |
|---|---|
| `RawText` | 未压缩经文，6 字节索引 |
| `RawText4` | 未压缩经文，8 字节索引 |
| `zText` | 压缩经文，10 字节经文索引 + 12 字节块索引 |
| `zText4` | 压缩经文，12 字节经文索引 + 12 字节块索引 |
| `RawCom` / `RawCom4` | 注释，同 RawText 索引 |
| `zCom` / `zCom4` | 压缩注释，同 zText 索引 |
| `RawLD` / `RawLD4` | 字典，6/8 字节索引 |
| `zLD` | 压缩字典，8 字节索引 + EntriesBlock |
| `RawGenBook` | 通用书，`.bdt` + TreeKeyIdx |
| `RawFiles` | 文件型通用书（v1 不支持） |
| `HREFCom` | 链接型注释（v1 不支持） |

---

## 3. 目录布局与路径

标准库：

```
<prefix>/
├── mods.d/
│   ├── KJV.conf
│   └── ChiUns.conf
└── modules/
    ├── texts/
    │   ├── rawtext/<module>/{ot,nt,ot.vss,nt.vss}
    │   └── ztext/<module>/{ot,nt}.{b,c,v}{zs,zz,zv}
    ├── comments/
    │   ├── rawcom/<module>/{ot,nt,ot.vss,nt.vss}
    │   └── zcom/<module>/{ot,nt}.{b,c,v}{zs,zz,zv}
    ├── lexdict/
    │   ├── rawld/<module>/<base>.{idx,dat}
    │   └── zld/<module>/<base>.{idx,dat,zdx,zdt}
    └── genbook/
        └── rawgenbook/<module>/<base>.{bdt,idx,dat}
```

路径规则：

- `DataPath=./modules/texts/ztext/kjv/`：文本模块，绝对数据目录是 `<prefix>/modules/texts/ztext/kjv`。
- `DataPath=./modules/lexdict/rawld/strongs/strongs`：字典模块，驱动打开 `<prefix>/modules/lexdict/rawld/strongs/strongs.idx` 和 `.dat`；`AbsoluteDataPath` 记录为 `<prefix>/modules/lexdict/rawld/strongs`。
- `DataPath=./modules/genbook/rawgenbook/geraugustinus/geraugustinus`：通用书，打开 `<prefix>/.../geraugustinus.bdt`，TreeKeyIdx 打开 `.idx`/`.dat`；`AbsoluteDataPath` 为 `<prefix>/.../geraugustinus` 目录。
- 路径中的 `./` 前缀在比较/显示时可去掉；不要保留不安全的 `..`。
- `PrefixPath` 末尾带 `/`；`AbsoluteDataPath` 末尾不带分隔符（上游在构造后 trim）。

---

## 4. 经文索引（VerseKey / versification）

### 4.1 偏移模型

一个经文系统包含 OT 和 NT 的书卷、每卷章数、每章最大节数。SWORD 在 `VersificationMgr::System::loadFromSBook` 中为每卷的每章计算 `offsetPrecomputed[chapter]`，并为全文计算全局偏移：

```
global offset 0            = 模块标题（Module Heading）
global offset 1            = OT 约标题（Testament 1 Heading）
global offset 2            = OT 第 1 卷书标题（Genesis 0:0）
global offset 3            = OT 第 1 卷第 1 章章标题（Genesis 1:0）
global offset 4            = Gen 1:1
...
global offset N            = OT 最后一节
global offset N+1          = NT 约标题（Testament 2 Heading）
global offset N+2          = NT 第 1 卷书标题（Matthew 0:0）
global offset N+3          = Matthew 1:0
global offset N+4          = Matthew 1:1
...
```

`getIndex()`：

```
module heading:        0
testament heading:     testament == 2 ? ntStartOffset + 1 : 1
book/chapter/verse:    refSys->getOffsetFromVerse(globalBookIndex, chapter, verse)
```

其中 `globalBookIndex` 从 0 开始，OT 为 `book-1`，NT 为 `BMAX[0] + book - 1`。

`getOffsetFromVerse(book, chapter, verse)`：

```
chapter0 = chapter - 1
if chapter0 >= 0:
    offset = offsetPrecomputed[chapter0]
else:
    offset = offsetPrecomputed[0] - 1
return offset + verse
```

因此：

- 书标题：`chapter=0, verse=0` -> `offsetPrecomputed[0] - 1`。
- 章标题：`chapter=c, verse=0` -> `offsetPrecomputed[c-1]`。
- 章内第 v 节：`offsetPrecomputed[c-1] + v`。

`getTestamentIndex()`：

```
index = getIndex()
if testament == 2: index -= ntStartOffset
```

OT 和 NT 使用各自的 `ot.vss` / `nt.vss`（或 `ot.*` / `nt.*`）文件，文件内索引从 0 开始。

KJV 参考值（用于测试）：

- `BMAX = { 39, 27 }`
- `ntStartOffset = 24114`
- `Gen 1:1`：global 4，OT index 4
- `Matt 1:1`：global 24118，NT index 4
- `[ Testament 2 Heading ]`：global 24115，NT index 1
- `[ Module Heading ]`：global 0，OT index 0

### 4.2 书卷缩写

`VerseKey::getBookFromAbbrev` 在本地化缩写表中做前缀匹配，再通过 OSIS 名映射到当前经文系统的书卷号。第一版内置英文/OSIS 缩写和中文常用缩写；找不到时返回错误/ snap 到最近书卷。

键解析支持：

- `Gen 1:1`、`Genesis 1:1`
- `1Sam1.20`、`Jn.3.16`、`Ps.119.176`
- `Matt 1:1-3`、`Matt 1:1,3,5`、`Matt 1:1-2:3`
- 范围/列表：`GswVerseKey` 单键取第一个元素；后续可增加 `GswVerseRange`/`GswListKey`。
- 特殊标题：`[ Module Heading ]`、`[ Testament 1 Heading ]`、`[ Testament 2 Heading ]`。
- 后缀 `a`/`b`：部分模块用节后缀；v1 保留字段，解析 `16a`。

### 4.3 自动规范化

- `auto_normalize` 默认开启：越界时 snap 到最近合法节，并标记 `is_valid=false`。
- `intros` 默认关闭（与常见前端一致）：迭代时跳过 0 节/0 章标题；需要时可开启。
- 边界：可设置 lower/upper bound，超出时 snap 并报 `GSW_ERROR_OUT_OF_BOUNDS`。

---

## 5. RawVerse（RawText / RawText4 / RawCom / RawCom4）

### 5.1 文件

```
<data-dir>/ot        OT 原始字节数据
<data-dir>/nt        NT 原始字节数据
<data-dir>/ot.vss    OT 索引
<data-dir>/nt.vss    NT 索引
```

### 5.2 索引记录

`RawText`、`RawCom`（2 字节大小）：

```
偏移 0  u32 start   数据文件中的字节偏移
偏移 4  u16 size    条目字节数
记录大小 6
```

`RawText4`、`RawCom4`（4 字节大小）：

```
偏移 0  u32 start
偏移 4  u32 size
记录大小 8
```

索引记录按 `testamentIndex` 排列；记录数等于该约的 key 数（标题 + 书 + 章 + 节）。NT 文件末尾可能有一个额外的哨兵记录（上游 `createModule` 在 NT 额外写一条），读取时应允许“索引记录数 > 实际 key 数”。

### 5.3 读取

```
if testament == 0: testament = 2 if nt index exists else 1
seek(index_file, testamentIndex * recordSize)
start = u32; size = u16/u32
if size == 0: return empty
seek(data_file, start)
read exactly size bytes
```

- 条目没有 NUL 结尾；读取端补 NUL。
- 写入时会在数据后追加一个 `\n` 方便编辑器查看；`size` 不包含该换行。
- 如果 size 记录截断/读取失败，上游回退为“从 start 到文件末尾”；本项目应报 `GSW_ERROR_INVALID_DATA`，或为兼容选择同样回退并警告。
- 空条目：`start=0,size=0`。
- 链接：`RawText::linkEntry` 直接把源条目的 `(start,size)` 复制到目标索引，因此两个 key 指向同一段数据；不是 `@LINK` 文本。

### 5.4 写入（v1 不实现，仅说明）

- 新条目追加到数据文件末尾，写 `size` 字节 + `\n`，索引写 `(start,size)`。
- 删除条目写 `(0,0)`。
- 创建模块时按 versification 迭代所有 key，为 OT/NT 写入全 0 索引。

---

## 6. zVerse（zText / zText4 / zCom / zCom4）

### 6.1 文件

块类型字母：

| `BlockType` | 字母 | 文件名 |
|---|---|---|
| `VERSE` | `v` | `ot.vzs`, `ot.vzz`, `ot.vzv` |
| `CHAPTER` | `c` | `ot.czs`, `ot.czz`, `ot.czv` |
| `BOOK` | `b` | `ot.bzs`, `ot.bzz`, `ot.bzv` |

NT 同理前缀为 `nt`。

含义：

- `.zv`：逐经文索引（verse index），每个 testamentIndex 一条。
- `.zs`：压缩块索引（block index），每个块一条。
- `.zz`：压缩块数据（zdata）。

### 6.2 逐经文索引 `.zv`

`zText` / `zCom`（2 字节 size）：

```
偏移 0  u32 blockNum    压缩块编号
偏移 4  u32 start       条目在解压块内的起始偏移
偏移 8  u16 size        条目字节数
记录大小 10
```

`zText4` / `zCom4`（4 字节 size）：

```
偏移 0  u32 blockNum
偏移 4  u32 start
偏移 8  u32 size
记录大小 12
```

### 6.3 块索引 `.zs`

两种驱动相同，12 字节：

```
偏移 0  u32 compOffset    压缩数据在 .zz 中的偏移
偏移 4  u32 compSize      压缩字节数
偏移 8  u32 uncompSize    解压后字节数（供校验/分配）
```

### 6.4 读取流程

```
1. 若 key.size == 0：空条目。
2. 读 .zv 的 testamentIndex * (10 或 12)：
     blockNum, start, size
3. 若 blockNum 不是当前缓存块：
     a. 读 .zs 的 blockNum * 12：
          compOffset, compSize, uncompSize
     b. seek .zz 到 compOffset，读 compSize 字节
     c. 可选：解密（rawZFilter，Sapphire）
     d. 解压为 uncompSize 字节（或按压缩库返回的长度）
     e. 缓存该解压块
4. 从缓存块 [start, start+size) 取条目。
```

- 上游 `zVerse::findOffset` 对 `.zv` 的 seek 是 `idxoff * 10`（zText）或 `idxoff * 12`（zText4）；对 `.zs` 的 seek 是 `blockNum * 12`。
- 缓存通常只保留一个块；v1 可保留一个块。
- `uncompSize` 用于日志/校验；解压器应允许实际长度与之不同（上游不强制）。
- `.zz` 中块之间可能有额外的 `\r\n`（上游写入时追加），但 `.zs` 的偏移/大小是权威。
- 块编号由写入顺序分配，不直接等于书/章号；读取只依赖 `.zv` 中的 `blockNum`。
- NT 的 `.zv` 可能比 key 数多一条哨兵记录；读取时按 key 数访问即可。

### 6.5 压缩类型

| `CompressType` | 算法 | 说明 |
|---|---|---|
| `LZSS` | SWORD 自有 LZSS | 默认；无头部 |
| `ZIP` | zlib `compress2`/`uncompress` | zlib 格式（含 2 字节头 + adler32） |
| `BZIP2` | libbz2 buffer API | 原始 bzip2 流 |
| `XZ` | liblzma buffer API | 原始 xz 流 |

没有统一的压缩头；必须由 conf 的 `CompressType` 决定。

### 6.6 加密

- 若 conf 有非空 `CipherKey`，`.zz` 数据在压缩前/解压后经过 `rawZFilter`（SWORD `SWCipher`，Sapphire）。
- 本项目已实现 Sapphire 解密：读取时先解密 `.zz` 数据，再按 `CompressType` 解压。
- 加密发生在压缩之后（写入时先压缩再加密；读取时先解密再解压）。
- 若 `CipherKey` 缺失，读取返回 `GSW_ERROR_CIPHERED`；安装/列出元数据不受影响。

---

## 7. RawStr（RawLD / RawLD4）

### 7.1 文件

```
<base>.idx    索引
<base>.dat    数据
```

`<base>` 来自 `DataPath` 的最后一段。

### 7.2 索引

`RawLD`（2 字节 size）：

```
偏移 0  u32 datOffset
偏移 4  u16 size
记录大小 6
```

`RawLD4`（4 字节 size）：

```
偏移 0  u32 datOffset
偏移 4  u32 size
记录大小 8
```

### 7.3 数据记录

```
key + "\r\n" + value
```

- `size` 包含 `key\r\nvalue` 的总字节数。
- key 以 `\r` 或 `\n` 结束；value 可以包含任意字节（包括换行）。
- 索引按 key 排序；默认不区分大小写，写入时 key 会被转成大写（UTF-8 感知的 `upperUTF8`），查询时也转大写。
- `CaseSensitiveKeys=true` 时保留原大小写，按字节比较。

### 7.4 读取

```
1. 查询 key（必要时 strongsPad + upper）
2. 在 .idx 上二分查找：
     每条记录读 datOffset/size -> 读 .dat 中 key 行 -> 比较
3. 命中后 seek datOffset，读 size 字节
4. 跳过第一个 '\n'，剩余即 value
5. 若 value 以 "@LINK " 开头：
     取下一行前的 key，递归查找
```

- `.idx` 记录数 = 条目数；没有单独的计数头。
- 二分查找使用记录偏移（6/8 的倍数）。
- 未命中时上游会 snap 到最近条目并返回错误；v1 可返回 `GSW_ERROR_NOT_FOUND` 或 snap（由 API 选择）。

### 7.5 Strong's 补零

`strongsPad` 规则（用于 `StrongsPadding=true`，默认）：

- 长度 1..8；
- 可带前缀 `G`/`H`（大小写）；
- 去掉前缀后，若剩余部分（除最后一位可 alpha 外）全为数字，则把数字部分补零到 5 位；
- 保留最后一位字母后缀（如 `123B` -> `G0123B` 的具体形式依实现）。

第一版按上游算法实现并用字典模块测试。

---

## 8. zStr（zLD）

### 8.1 文件

```
<base>.idx    条目索引
<base>.dat    条目数据（key + block/entry）
<base>.zdx    压缩块索引
<base>.zdt    压缩块数据
```

### 8.2 条目索引 `.idx`

8 字节：

```
偏移 0  u32 datOffset
偏移 4  u32 size
```

### 8.3 条目数据 `.dat`

```
key + "\r\n" + block(u32) + entry(u32)
```

- `size` 包含 key 行和 8 字节引用。
- 默认 key 大写；`CaseSensitiveKeys` 控制。
- `@LINK` 链接与 RawStr 类似：value 以 `@LINK ` 开头时解析目标 key。

### 8.4 压缩块索引 `.zdx`

8 字节：

```
偏移 0  u32 start    压缩块在 .zdt 中的偏移
偏移 4  u32 size     压缩字节数
```

### 8.5 压缩块 `.zdt`

- 每个块是一个 `EntriesBlock` 压缩后的数据，压缩算法由 `CompressType` 决定。
- 块内条目数由 `BlockCount` 控制（写入时，默认 200）。
- 读取时按 `block` 从 `.zdx` 取 `(start,size)`，读 `.zdt`，解密（可选），解压，构造 `EntriesBlock`，再取第 `entry` 项。

### 8.6 EntriesBlock 布局

```
偏移 0                  u32 count
偏移 4                  count * { u32 offset, u32 size }
偏移 4 + count*8        条目数据区
```

- `offset` 从块起始算起；`size` 包含结尾 NUL。
- `offset == 0` 表示已删除条目。
- 条目数据是 NUL 结尾字符串；`size` 包含 NUL。
- 示例（`alpha`、`beta`）：

```
02000000                 count=2
14000000 06000000        entry0 offset=20 size=6
1a000000 05000000        entry1 offset=26 size=5
616c70686100             "alpha\0"
6265746100               "beta\0"
```

总长 31 字节。

### 8.7 读取流程

```
1. 在 .idx 二分查找 key
2. 读 .dat 的 (datOffset,size)，跳过 key 行，取 block/entry
3. 若 block 不是当前缓存：
     a. 读 .zdx 的 block*8: start,size
     b. 读 .zdt[start,start+size)
     c. 可选解密
     d. 解压为 EntriesBlock 原始字节
     e. 缓存
4. 返回 EntriesBlock.getEntry(entry)
```

---

## 9. RawGenBook / TreeKeyIdx

### 9.1 文件

```
<base>.bdt    条目数据
<base>.idx    树节点索引（4 字节记录）
<base>.dat    树节点数据
```

### 9.2 `.idx`

- 每条记录 4 字节：`u32 datOffset`，指向 `.dat` 中的节点记录。
- 节点号 = 记录偏移 / 4。
- 根节点在偏移 0。
- `increment`/`decrement` 按记录顺序移动（`offset ± 4`），不是树遍历；树遍历用 firstChild/nextSibling/parent。

### 9.3 `.dat` 节点记录

```
偏移 0   s32 parent       父节点 idx 偏移；根为 -1
偏移 4   s32 next         下一个兄弟节点 idx 偏移；无则 -1
偏移 8   s32 firstChild   第一个子节点 idx 偏移；无则 -1
偏移 12  name             以 NUL 结尾的节点名
...      u16 userDataSize
...      userData[userDataSize]
```

- `parent`/`next`/`firstChild` 存的是 `.idx` 文件中的偏移（不是节点号）。
- 根节点名通常为空。
- 节点路径通过 parent 链拼成 `A/B/C`；`/` 是分隔符。

### 9.4 条目引用

`RawGenBook` 的节点 `userData` 为 8 字节：

```
偏移 0  u32 bdtOffset
偏移 4  u32 size
```

- `bdtOffset`/`size` 指向 `.bdt` 中的条目。
- 读取时 seek `.bdt`，读 `size` 字节；空/无 userData 返回空。

### 9.5 特殊 `KeyType=VerseKey`

- `RawGenBook` 可配置 `KeyType=VerseKey`，此时节点键为经文键（`VerseTreeKey`），模块类型被设为 `Biblical Texts`。
- v1 先支持标准 TreeKey；`VerseKey` 变体后置。

---

## 10. LZSS 位流

SWORD 的 LZSS 参数：

- `N = 4096`（环形缓冲区，12 位位置）
- `F = 18`（最大匹配长度）
- `THRESHOLD = 3`
- 环形缓冲区前 `N-F` 字节初始化为空格 `0x20`。

### 10.1 编码格式

- 每 8 个单元有一个标志字节。
- 标志字节的**低位**对应第一个单元：
  - bit=1：下一个字节是字面字符（1 字节）。
  - bit=0：接下来的 2 字节是 `<position:12, length:4>`：
    ```
    pos = b0 | ((b1 & 0xf0) << 4)
    len = (b1 & 0x0f) + 3
    ```
- 每个标志字节用完后，下一个字节是新的标志字节。
- 位置是环形缓冲区索引（0..4095），长度 3..18。
- 解码时环形缓冲区初始为 `N-F` 个空格，`r = N-F`；每输出一个字节写入 `ring[r]` 并 `r = (r+1) & (N-1)`；复制时从 `ring[(pos+k) & (N-1)]` 取。
- 编码时环形缓冲区初始为空格，先读 F 字节到尾部，再建树匹配。解码器不需要建树。

### 10.2 已知测试向量

由上游 `LZSSCompress` 生成（十六进制）：

| 输入 | 长度 | 压缩输出 |
|---|---:|---|
| `""` | 0 | `(空)` |
| `"a"` | 1 | `0161` |
| `"abc"` | 3 | `07616263` |
| `"hello hello hello"` | 17 | `1f68656c6c6fedf9` |
| `"         "`（9 个空格） | 9 | `00edf6` |
| `"abcabcabcabcabcabcabcabcabcabc"` | 30 | `07616263eefffaf6` |

解码器必须能还原这些输入；编码器（若实现）必须产生可被上游解码的输出（不要求字节完全一致，但建议一致以便测试）。

### 10.3 其他压缩

- `ZIP`：zlib 的 `compress2` 输出（zlib 格式），用 `uncompress` 解码。注意不是 raw deflate，也不是 zip 归档。
- `BZIP2`：libbz2 的 `BZ2_bzBuffToBuffCompress/Decompress`。
- `XZ`：liblzma 的 `lzma_easy_buffer_encode` / `lzma_stream_buffer_decode`。
- 所有压缩数据都没有额外头部；类型来自 conf。

---

## 11. 归档 `mods.d.tar.gz`

- gzip 压缩的 tar 归档。
- 成员路径以 `mods.d/` 开头，例如：
  ```
  mods.d/
  mods.d/chiuns2.conf
  mods.d/kjv.conf
  ```
- 解包到 `<private>/<uid>/`，得到 `<private>/<uid>/mods.d/*.conf`。
- 只关心普通文件和目录；其他类型忽略或拒绝。
- 安全要求（本项目强制）：
  - 拒绝绝对路径、`..`、符号链接/硬链接/设备/FIFO；
  - 拒绝解包后路径不在 staging 目录内；
  - 限制单文件大小、总大小、成员数；
  - 解包后校验 `mods.d/` 存在且含 `.conf`。

---

## 12. 安装源配置

### 12.1 `InstallMgr.conf`

```ini
[General]
PassiveFTP=true
UnverifiedPeerAllowed=true
TimeoutMillis=10000
DefaultMod=KJV
DefaultMod=StrongsGreek

[Sources]
FTPSource=CrossWire|ftp.crosswire.org|/pub/sword/raw|||20081216195754
HTTPSource=MyRepo|example.org|/sword||||myrepo
HTTPSSource=SecureRepo|example.org|/sword|user|pass|secure-uid
```

`*Source` 值格式：

```
Caption|host|directory|user|password|uid
```

- `Caption`：用户可见名称，唯一。
- `host`：主机名（不要带 `http://`）。
- `directory`：远端根目录，例如 `/pub/sword/raw`。
- `user`/`password`：可空；空则使用默认 `ftp` / `installmgr@user.com`。
- `uid`：本地 shadow 目录名；通常时间戳。必须安全（`[A-Za-z0-9._-]`，不以 `.` 开头）。
- `directory` 末尾 `/` 会被去掉；URL 拼接时再加。

### 12.2 `masterRepoList.conf`

```ini
[Repos]
20081216195754=FTPSource=CrossWire|ftp.crosswire.org|/pub/sword/raw
20090514005700=FTPSource=Bible.org|ftp.bible.org|/sword
20151110161300=FTPSource=eBible.org|ftp.ebible.org|/sword
```

- key：UID（本地目录名）。
- value：`<Type>Source=<Caption>|<host>|<directory>[|<user>|<pass>]`。
- `value == "REMOVE"` 表示删除该 UID 的源。
- 本项目还支持 `HTTPSource=` / `HTTPSSource=`（上游 master list 主要是 FTP）。

### 12.3 远端仓库布局

```
<source>/<directory>/
├── mods.d.tar.gz
├── mods.d/
│   └── *.conf
└── modules/
    └── ... 模块数据
```

安装 URL：

- 配置：`<source>/<directory>/mods.d.tar.gz`，或 `<source>/<directory>/mods.d/<file>.conf`
- 数据：`<source>/<directory>/<relativeDir>/...`（`relativeDir` 为 `AbsoluteDataPath` 相对 `<prefix>` 的目录）
- `File=` 条目：`<source>/<directory>/<File>`，目标 `<prefix>/<File>`（上游语义）。

### 12.4 安装清单

- 若模块 section 有 `File=`：
  - 逐个下载 `File=` 指定的文件。
  - 远端 URL 为 `<source>/<directory>/<File>`。
  - 目标为 `<prefix>/<File>`（相对仓库根，与上游一致）。
- 否则：
  - 取 `AbsoluteDataPath` 相对 `<prefix>` 的部分作为相对目录（对字典/通用书是包含 `<base>.*` 的父目录）。
  - 递归下载该目录到目标 `<prefix>/<relativeDir>`。
  - 注意：上游的上游实现会先把整个目录下载到 shadow，再复制到目标；本项目可直接下载到目标临时目录。
- 最后把源 `mods.d/<xxx>.conf` 复制到 `<prefix>/mods.d/<xxx>.conf`。
- `InstallSize` 可用于进度总量；如果为 0，使用目录列表大小。

---

## 13. 版本比较

与 `SWVersion` 兼容：

- 以 `.` 分割，最多 4 段。
- 缺失段 = `-1`。
- 每段 `atoi`。
- 依次比较 major、minor、minor2、minor3。
- 例：`1.0 < 1.0.1`，`1.0.0.0 == 1.0`（按上游缺失为 -1，`1.0` 的 minor2/3 为 -1，`1.0.0.0` 为 0，因此 `1.0 < 1.0.0`；实现时与上游保持一致并在测试中固定）。

> 注意：上游 `SWVersion` 对 `1.0` 与 `1.0.0` 的比较结果可能反直觉；本项目应写测试与上游一致，避免更新判断偏差。

---

## 14. 兼容性矩阵

| 格式 | v1 读取 | v1 写入 | 备注 |
|---|---|---|---|
| RawText / RawText4 | 是 | 否 | 经文/注释 |
| RawCom / RawCom4 | 是 | 否 | 同 RawVerse |
| zText / zText4 | 是 | 否 | LZSS/ZIP/BZIP2/XZ |
| zCom / zCom4 | 是 | 否 | 同 zVerse |
| RawLD / RawLD4 | 是 | 否 | 字典 |
| zLD | 是 | 否 | EntriesBlock |
| RawGenBook | 是 | 否 | TreeKey |
| RawFiles | 否 | 否 | 后续 |
| HREFCom | 否 | 否 | 后续 |
| Sapphire 加密 | 是 | 否 | zText/zCom/zLD/RawText/RawCom/RawLD/RawGenBook |
| SCSU 编码 | 否（报错） | 否 | 后续 |

---

## 15. 参考实现位置

| 主题 | 上游文件 |
|---|---|
| 配置 | `src/mgr/swconfig.cpp` |
| 模块加载/驱动选择 | `src/mgr/swmgr.cpp` |
| 经文键/解析 | `src/keys/versekey.cpp` |
| 经文系统 | `src/mgr/versificationmgr.cpp`、`include/canon*.h` |
| RawVerse | `src/modules/common/rawverse.cpp`、`rawverse4.cpp` |
| zVerse | `src/modules/common/zverse.cpp`、`zverse4.cpp` |
| RawStr | `src/modules/common/rawstr.cpp`、`rawstr4.cpp` |
| zStr | `src/modules/common/zstr.cpp` |
| EntriesBlock | `src/modules/common/entriesblk.cpp` |
| LZSS | `src/modules/common/lzsscomprs.cpp` |
| ZIP/untar | `src/modules/common/zipcomprs.cpp` |
| BZIP2/XZ | `src/modules/common/bz2comprs.cpp`、`xzcomprs.cpp` |
| RawGenBook | `src/modules/genbook/rawgenbook/rawgenbook.cpp` |
| TreeKeyIdx | `src/keys/treekeyidx.cpp` |
| 安装 | `src/mgr/installmgr.cpp` |
| 传输 | `src/mgr/remotetrans.cpp`、`curlhttpt.cpp`、`curlftpt.cpp` |
| 压缩基类 | `src/modules/common/swcomprs.cpp` |

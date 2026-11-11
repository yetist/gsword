# 上游 SWORD 1.9 实现分析

- 状态：设计输入
- 日期：2026-10-28
- 分析对象：`sword-1.9.0/`（C++）
- 目的：理解 SWORD 的加载、安装、存储、键与过滤器机制，为 `libsword` 的 C/GObject 重写提供依据。

---

## 1. 源码地图

```
sword-1.9.0/
├── include/            公共头文件（类声明、canon 数据）
├── src/
│   ├── mgr/            管理器：SWMgr、SWConfig、InstallMgr、传输、versification
│   ├── keys/           SWKey、VerseKey、StrKey、TreeKey、ListKey
│   ├── modules/
│   │   ├── texts/      RawText、zText 等文本驱动
│   │   ├── comments/   RawCom、zCom、RawFiles、HREFCom
│   │   ├── lexdict/    RawLD、zLD、SWLD
│   │   ├── genbook/    RawGenBook、SWGenBook
│   │   ├── common/     RawVerse/zVerse/RawStr/zStr、压缩、EntriesBlock、加密
│   │   └── filters/    标记/编码/选项过滤器
│   ├── utilfuns/       SWBuf、FileMgr、URL、ftplib、ftpparse、zlib
│   └── frontend/       命令行前端
├── tests/              单元/集成测试（CppUnit）
├── examples/           示例
└── utilities/          imp2vs、diatheke 等
```

核心类关系：

```
SWObject
├── SWKey
│   ├── VerseKey
│   ├── StrKey
│   └── TreeKey / TreeKeyIdx
├── SWModule (also SWCacher, SWSearchable)
│   ├── SWText  -> RawText, zText
│   ├── SWCom   -> RawCom, zCom, RawFiles, HREFCom
│   ├── SWLD    -> RawLD, zLD
│   └── SWGenBook -> RawGenBook
├── SWConfig
├── SWMgr
├── InstallMgr / InstallSource
├── RemoteTransport (CURLHTTP/FTP, FTPLib)
└── SWCompress (LZSS, Zip, Bzip2, Xz)
```

---

## 2. 本地库发现与加载

入口：`SWMgr::load()`（`src/mgr/swmgr.cpp:837`）。

流程：

1. 若构造时未提供 `config`，调用 `findConfig()`（`swmgr.cpp:449`）。
2. `findConfig()` 依次查找：
   - 提供的 `sysConfig`；
   - 当前目录 `sword.conf`；
   - 当前目录 `mods.conf` / `mods.d/`；
   - `../library/mods.d`；
   - `SWORD_PATH` 环境变量（`mods.conf` 或 `mods.d`）；
   - 编译期 `globalConfPath`（如 `/etc/sword.conf`）；
   - `~/.sword/sword.conf`、`~/sword/sword.conf`；
   - `sword.conf` 的 `[Install] DataPath`；
   - `~/.sword/mods.d`、`~/sword/mods.d`。
3. `configType`：
   - `0` = 单文件 `mods.conf`；
   - `1` = 目录 `mods.d/`（系统级）；
   - `2` = 用户目录（`~/.sword`）。
4. `loadConfigDir()`（`swmgr.cpp:747`）读取目录下所有 `*.conf`，用 `SWConfig::augment` 合并；没有 conf 时创建 `globals.conf`。
5. `createAllModules()`（`swmgr.cpp:1456`）遍历 section，调用 `createModule(name, driver, section)`。
6. `augmentModules()`（`swmgr.cpp:776`）加载 `[Install] AugmentPath` 与 `~/.sword`，后加载覆盖先加载。
7. `AutoInstall` 机制：`Globals` section 的 `AutoInstall` 指向目录，`InstallScan()` 会把新 conf 复制到 `mods.d`。

关键点：

- `SWMgr` 用 `std::map<SWBuf, SWModule*>` 暴露模块。
- 过滤器管理器（`SWFilterMgr` / `MarkupFilterMgr`）在创建模块时按 `SourceType`、`Encoding`、`GlobalOptionFilter` 注入过滤器。
- `SWMgr` 构造时通常 `autoload=true`，立即创建所有模块并打开数据文件。

---

## 3. 配置：SWConfig

`SWConfig`（`src/mgr/swconfig.cpp`）：

- 数据结构：
  ```cpp
  typedef multimap<SWBuf, SWBuf> ConfigEntMap;
  typedef map<SWBuf, ConfigEntMap> SectionMap;
  ```
- `load()`：
  - 逐行读取；
  - 去掉 UTF-8 BOM；
  - `#` 开头忽略；
  - `[section]` 切换 section；
  - `key=value` 在第一个 `=` 分割；
  - 值 `strstrip`；
  - 重复 key 插入 multimap。
- `save()`：按 map 顺序输出 `[section]` 与 `key=value`。
- `augment()`：合并另一个配置，处理多值。
- 文件名保存在 `filename`，`save()` 写回。

问题：

- 没有错误返回；文件不存在/读取失败静默。
- 注释只认行首 `#`（前导空白后 `#` 会被当作 key）。
- 无转义、无续行。
- `Sections` 和 `filename` 是公开 deprecated 字段。

---

## 4. 模块创建与驱动映射

`SWMgr::createModule()`（`swmgr.cpp:909`）：

1. 从 section 读取 `Description`、`Lang`、`SourceType`、`Encoding`、`DataPath`、`Versification`。
2. 计算：
   - `PrefixPath` = `prefixPath`（末尾带 `/`）
   - `AbsoluteDataPath` = `PrefixPath + DataPath`（去掉 `./`）
3. 根据 `SourceType` 映射 `FMT_*`，根据 `Encoding` 映射 `ENC_*`，根据 `Direction` 映射 `DIRECTION_*`。
4. 按 `ModDrv` 创建具体类：
   - `zText` / `zText4` / `zCom` / `zCom4`：解析 `BlockType`、`CompressType`，new 对应压缩器。
   - `RawText` / `RawText4` / `RawCom` / `RawCom4`。
   - `RawLD` / `RawLD4` / `zLD`：解析 `CaseSensitiveKeys`、`StrongsPadding`、`BlockCount`、`CompressType`。
   - `RawGenBook`：解析 `KeyType`。
   - `HREFCom`：解析 `Prefix`。
   - `RawFiles`。
5. 对 `RawLD`/`zLD`/`RawGenBook`，把 `AbsoluteDataPath` 去掉最后一个 `/` 后的 basename，保留目录。
6. 若 section 有 `Type`，覆盖模块类型字符串。
7. `newmod->setConfig(&section)`，模块保存配置指针。

问题：

- 创建即打开数据文件；远端元数据场景依赖“打开失败但不报错”。
- `datapath` 的 basename 处理用 `pos=1` 标志，逻辑隐晦。
- 未知驱动静默返回 `NULL`。
- `CompressType` 不支持时静默创建失败。

---

## 5. 模块、键与过滤器

### 5.1 SWModule

`include/swmodule.h` / `src/modules/swmodule.cpp`：

- 成员：
  - `ownConfig` / `config`：配置 section。
  - `key`：当前键。
  - `entryBuf`：当前条目缓冲。
  - 过滤器列表：`stripFilters`、`rawFilters`、`renderFilters`、`optionFilters`、`encodingFilters`。
  - `entryAttributes`：条目属性（Strong's、lemma 等），由过滤器填充。
- 主要接口：
  - `setKey` / `getKey` / `getKeyText`
  - `getRawEntryBuf`（纯虚）
  - `renderText` / `stripText`
  - `increment` / `decrement` / `setPosition`
  - `search` / `createSearchFramework`
  - `getConfigEntry`
- 过滤器顺序（读取时）：
  1. 驱动读取原始字节；
  2. `rawFilter`（解密等）；
  3. `encodingFilter`（转 UTF-8）；
  4. `optionFilter`（Strong's、脚注等开关）；
  5. `renderFilter`（OSIS/ThML/GBF -> HTML/纯文本等）；
  6. `stripFilter`（搜索/纯文本）。

### 5.2 键

- `SWKey`：文本键基类，支持 `setText`、`getText`、`increment/decrement`、`compare`、`clone`、`setPosition`。
- `VerseKey`：
  - 字段 `testament/book/chapter/verse/suffix`；
  - `refSys` 指向 `VersificationMgr::System`；
  - `parse()` 调用 `parseVerseList()`，支持范围/列表；
  - `normalize()` 处理越界 snap；
  - `getIndex()` / `getTestamentIndex()` 做经文偏移；
  - `getBookFromAbbrev()` 用本地化缩写表 + OSIS 名映射；
  - `setIntros()` 控制是否包含 0 节/0 章标题。
- `StrKey`：字典字符串键。
- `TreeKey` / `TreeKeyIdx`：树键，`TreeKeyIdx` 用 `.idx`/`.dat` 持久化；支持 parent/firstChild/nextSibling、`getUserData`（RawGenBook 存 bdt 偏移/大小）。

### 5.3 VersificationMgr

`src/mgr/versificationmgr.cpp`：

- 内置多个系统：KJV、Leningrad、MT、KJVA、NRSV、NRSVA、Synodal、SynodalProt、Vulg、German、Luther、Catholic、Catholic2、LXX、Orthodox、Calvin、DarbyFr、Segond。
- 数据来自 `include/canon_*.h` 的 `sbook` 数组和 `vm[]` 数组。
- `loadFromSBook()` 构造：
  - `offset` 从 0 开始；
  - 模块标题占 0；
  - 约标题、书标题、章标题各占一个偏移；
  - 章内节从 1 开始；
  - 计算 `offsetPrecomputed`、`BMAX`、`ntStartOffset`。
- `getOffsetFromVerse()` / `getVerseFromOffset()` 做双向映射。
- `translateVerse()` 处理不同系统间的映射（NRSV、Vulg 等）。
- 单例 `getSystemVersificationMgr()`。

### 5.4 本地化

`LocaleMgr` / `SWLocale`：

- 读取 `locales.d/*.conf`；
- 书名翻译、缩写表（`abbr.conf`）；
- `VerseKey::getBookFromAbbrev` 依赖当前 locale。
- 本项目 v1 不实现完整 locale，内置英文/OSIS + 常用中文缩写，后续可扩展。

---

## 6. 安装流程

### 6.1 InstallMgr 构造

`src/mgr/installmgr.cpp:99`：

- `privatePath` 默认 `./`。
- `confPath = privatePath + "/InstallMgr.conf"`。
- `readInstallConf()`：
  - 读取 `[General]`：
    - `PassiveFTP`（默认 true）
    - `TimeoutMillis`（默认 10000）
    - `UnverifiedPeerAllowed`（默认 true）
  - 读取 `[Sources]`：
    - `FTPSource` / `SFTPSource` / `HTTPSource` / `HTTPSSource`
    - 每个值 `Caption|source|directory|u|p|uid`
    - `localShadow = privatePath + "/" + uid`
  - 读取 `[General] DefaultMod` 列表。
- `saveInstallConf()` 写回。

### 6.2 刷新远端源

`refreshRemoteSource()`（`installmgr.cpp:550`）：

```
root = privatePath + "/" + uid
target = root + "/mods.d"
removeDir(target)

# 优先
error = remoteCopy(is, "mods.d.tar.gz", root + "/mods.d.tar.gz")
if !error:
    ZipCompress::unTarGZ(fd, root)

# 失败且不是连接错误/取消
if error > -2:
    remoteCopy(is, "mods.d", target, true, ".conf")

is->flush()
```

`InstallSource::getMgr()`：

```cpp
mgr = new SWMgr(localShadow.c_str(), true, 0, false, false);
```

即用 shadow 目录构建一个只含 conf 的 `SWMgr`。因为驱动构造容忍数据文件缺失，所以能列出元数据。

### 6.3 安装模块

`installModule()`（`installmgr.cpp:394`）：

1. `sourceDir = privatePath + "/" + uid` 或 `fromLocation`。
2. `SWMgr mgr(sourceDir)`。
3. 在 `mgr.config` 中找模块 section。
4. 若有 `File=`：
   - 对每个 `File`：`remoteCopy(is, file, sourceDir + file)`；
   - 再复制到 `destMgr->prefixPath + "/" + file`；
   - 删除临时文件。
5. 否则：
   - `AbsoluteDataPath` 从 section 取；
   - `relativePath` = `AbsoluteDataPath` 去掉 `PrefixPath`（或 `mgr.prefixPath`）；
   - 若远端：`remoteCopy(is, relativePath, absolutePath, true)` 下载整个目录到 shadow；
   - 复制 shadow 目录到 `destMgr->prefixPath + relativePath`；
   - 删除 shadow 临时数据。
6. 在 `sourceDir/mods.d/` 中找包含该模块 section 的 `.conf`，复制到 `destMgr->configPath`。
7. 若模块有 `CipherKey`：调用 `getCipherCode` 让 UI 提供密钥；失败则删除已安装模块。

`remoteCopy()`（`installmgr.cpp:300`）：

- 校验 disclaimer。
- 按 `is->type` 创建 FTP/HTTP transport。
- 设置 passive、timeout、user/pass、unverifiedPeerAllowed。
- 构造 URL：`<scheme>://<source><directory>/<src>`。
- `dirTransfer` 时调用 `transport->copyDirectory(urlPrefix, dir, dest, suffix)`。
- 否则 `transport->getURL(dest, url)`。
- 返回：`0` 成功，`-1` 操作错误，`-2` 连接错误，`-3` 取消。

### 6.4 删除模块

`removeModule()`（`installmgr.cpp:241`）：

- 在 `manager->config` 找 section；
- `manager->deleteModule(name)` 关闭模块；
- 若有 `File=`：逐个删除 `<AbsoluteDataPath>/<File>`；
- 否则 `FileMgr::removeDir(AbsoluteDataPath)`，并扫描 `configPath` 下所有 `.conf`，删除包含该 section 的 conf；
- 返回 0/1。

### 6.5 模块状态

`getModuleStatus()`（`installmgr.cpp:592`）：

- 遍历远端模块；
- 读 `CipherKey`、`Version`、`MinimumVersion`；
- 在本地找同名模块比较版本；
- 返回 `MODSTAT_*` 位标志：
  - `OLDER=0x001`
  - `SAMEVERSION=0x002`
  - `UPDATED=0x004`
  - `NEW=0x008`
  - `CIPHERED=0x010`
  - `CIPHERKEYPRESENT=0x020`

---

## 7. 传输层

`RemoteTransport`（`include/remotetrans.h`、`src/mgr/remotetrans.cpp`）：

- 抽象方法：
  - `getURL(destPath, sourceURL, destBuf=0)`
  - `putURL(destURL, sourcePath, sourceBuf=0)`
  - `getDirList(dirURL)`
- `copyDirectory(urlPrefix, dir, dest, suffix)`：
  - 列目录；
  - 递归展开子目录；
  - 计算总大小；
  - 对每个匹配 `suffix` 的文件调用 `getURL`；
  - 发 `preStatus(total, completed, "Downloading (i of n): name")`。
- `CURLHTTPTransport`（`curlhttpt.cpp`）：
  - `curl_easy_init` + `CURLOPT_WRITEFUNCTION` + `CURLOPT_PROGRESSFUNCTION`；
  - `CURLOPT_FAILONERROR`、超时、TLS 校验可关；
  - FTP 相关选项；
  - `getDirList()` 解析 HTML `<a href>` 和 `<td>` 大小。
- `CURLFTPTransport`（`curlftpt.cpp`）：
  - 类似，用 FTP URL / CURL；
  - `getDirList()` 可能用 `ftpparse`。
- `FTPLibFTPTransport`（`ftplibftpt.cpp`）：
  - 基于内置 `ftplib.c`；
  - 无 HTTP。
- `StatusReporter`：
  - `preStatus(total, completed, message)`
  - `update(total, completed)` -> 已弃用的 `statusUpdate(double,double)`。

问题：

- HTTP 目录列表解析非常脆弱，依赖具体服务器 HTML。
- 进度只有当前文件，没有整体语义。
- 取消只能通过 `RemoteTransport::terminate()` 设置 `term`，但 curl 回调没有检查它（部分实现依赖写回调失败）。
- `unverifiedPeerAllowed` 默认 true，不安全。
- 每个请求新建 transport，连接不复用。

---

## 8. 存储驱动

详细字节格式见 `docs/formats.md`。这里只列结构：

- `RawVerse`：
  - `ot`/`nt` 数据 + `ot.vss`/`nt.vss` 6 字节索引；
  - `findOffset(testmt, idxoff, &start, &size)`；
  - `readText(testmt, start, size, buf)`；
  - 写入追加数据、更新索引；
  - `createModule()` 按 VerseKey 迭代写全 0 索引。
- `RawVerse4`：索引 8 字节。
- `zVerse`：
  - `ot.<x>zv` 逐经文索引（10 字节）；
  - `ot.<x>zs` 块索引（12 字节）；
  - `ot.<x>zz` 压缩数据；
  - `findOffset()` 读 `.zv` 得到 block/start/size；
  - `zReadText()` 读 `.zs`、解压、缓存；
  - `flushCache()` 写入压缩块。
- `zVerse4`：`.zv` 12 字节。
- `RawStr`：
  - `.idx` 6 字节（dat offset + size）；
  - `.dat` `key\r\nvalue`；
  - 二分查找；
  - `@LINK` 解析。
- `RawStr4`：`.idx` 8 字节。
- `zStr`：
  - `.idx` 8 字节；
  - `.dat` key + block/entry；
  - `.zdx` 块索引 8 字节；
  - `.zdt` 压缩 EntriesBlock；
  - `EntriesBlock` 布局。
- `RawGenBook`：
  - `.bdt` 数据；
  - TreeKeyIdx `.idx`/`.dat`；
  - 节点 userData 存 bdt offset/size。
- `RawFiles`：文件型，按 key 映射到文件。
- `HREFCom`：注释文本是链接/偏移。

共同问题：

- 文件句柄由全局 `FileMgr` 管理，构造即打开。
- 很多函数返回 `char` 错误码，0 成功、负数失败，语义不统一。
- 读取缓冲使用 `SWBuf`，长度/容量处理复杂。
- 热路径有大量 `new/delete`。

---

## 9. 压缩与加密

- `SWCompress`：基类，提供 `setUncompressedBuf`/`getCompressedBuf`/`getChars`/`sendChars`。
- `LZSSCompress`：SWORD 自有位流；N=4096、F=18、THRESHOLD=3；环形缓冲区初始空格；编码用二叉树匹配。
- `ZipCompress`：zlib `compress2`/`uncompress`；还包含 `unTarGZ()`（gzip + tar 解包）。
- `Bzip2Compress`：libbz2。
- `XzCompress`：liblzma。
- `SWCipher`：Sapphire 加密；模块 `CipherKey` 通过 `rawFilter` 在压缩数据上解密/加密。

问题：

- `unTarGZ` 没有路径安全检查。
- 解压大小信任输入，可能内存爆炸。
- LZSS 使用静态成员保存 ring buffer/tree，多个实例并发不安全（上游用 `Private` 静态数组，实际是全局状态）。
- 压缩错误只写日志，不返回错误。

---

## 10. 过滤器与渲染

- `SWFilter` 基类：`processText(SWBuf&, const SWKey*, const SWModule*)`。
- `SWOptionFilter`：有 option 名称和值。
- `MarkupFilterMgr`：按输出格式（HTML/HTMLHREF/RTF/OSIS/Plain 等）添加渲染过滤器。
- 编码过滤器：`Latin1UTF8`、`UTF16UTF8`、`UTF8SCSU`、`UTF8Latin1` 等。
- 选项过滤器：`OSISStrongs`、`OSISFootnotes`、`OSISHeadings`、`OSISRedLetterWords`、`GBFStrongs`、`ThMLStrongs` 等。
- 过滤器会填充 `entryAttributes`（Strong's、lemma、morph 等）。

本项目 v1 只做基础纯文本剥离，不实现完整过滤器图。设计上保留过滤器管线，后续按需添加。

---

## 11. 上游缺陷与本项目对策

| 上游问题 | 影响 | 本项目对策 |
|---|---|---|
| 创建模块时打开数据文件 | 远端元数据依赖“打开失败但不报错” | 元数据/后端分离，懒加载 |
| 全程裸指针/new/delete | 内存泄漏、绑定困难 | GObject + boxed + 明确所有权 |
| 错误用 `char` 返回码/全局 `error` | 调用者难以诊断 | `GError` + 错误码 |
| 路径/解包不校验 | 路径穿越、覆盖任意文件 | 路径工具 + 安全 tar + 测试/fuzz |
| 安装非原子 | 取消/失败留下半成品 | staging + rename + conf 最后写 |
| 下载先落 shadow 再复制 | 双倍 I/O | 直接下载到目标临时目录 |
| 进度只有当前文件 | UI 需自己累计 | 同时提供 current/overall 语义 |
| HTTP 目录解析脆弱 | 刷新失败 | 优先 tar.gz；MLSD/LIST/HTML 回退 |
| TLS 默认不校验 | 中间人风险 | 默认校验，显式关闭需选项 |
| LZSS 静态全局状态 | 线程不安全 | 每实例状态 |
| 过滤器图庞大 | 工期/复杂度 | v1 基础 strip；管线预留 |
| 单例 versification | 可接受，但初始化需线程安全 | 初始化后只读；`g_once_init` |
| 语言/本地化复杂 | 依赖 locales.d | v1 内置常用缩写，后续扩展 |
| 配置注释只认行首 | 兼容性小问题 | 允许前导空白后 `#` |
| `SWVersion` 版本比较反直觉 | 更新判断偏差 | 写测试锁定与上游一致 |

---

## 12. 上游类到本项目映射

| 上游 | 本项目 | 说明 |
|---|---|---|
| `SWMgr` | `GswManager` | 本地库、发现、元数据 |
| `SWConfig` | `GswConfig` | GObject |
| `SWModule` | `GswModule` | 公共模块对象 |
| `SWText` / `SWCom` / `SWLD` / `SWGenBook` | 后端 + 键类型 | 不公开继承层次 |
| `RawText` / `zText` / ... | `GswRawVerse` / `GswZVerse` / ... | 内部 |
| `VerseKey` | `GswVerseKey` | boxed |
| `StrKey` | `GswStrKey` | boxed |
| `TreeKeyIdx` | `GswTreeKey` | boxed |
| `VersificationMgr` | `GswVersificationMgr` | GObject |
| `InstallMgr` | `GswInstallManager` | GObject |
| `InstallSource` | `GswInstallSource` | GObject |
| `RemoteTransport` | `GswTransport` | derivable |
| `StatusReporter` | `GswStatusReporter` | 信号 |
| `SWCompress` | `GswCompressor` | 内部接口 |
| `SWFilter` | 内部 filter | v1 基础实现 |

---

## 13. 可借鉴的上游测试

`sword-1.9.0/tests/` 中有价值的测试：

- `configtest.cpp`：配置解析/保存。
- `keytest.cpp`、`parsekey.cpp`、`listtest.cpp`：键解析/列表。
- `refsystest.cpp`：versification 偏移/转换。
- `indextest.cpp`、`ldtest.cpp`、`modtest.cpp`：模块驱动读取。
- `complzss.cpp`、`compzip.cpp`、`compnone.cpp`：压缩 round-trip。
- `installmgrtest.cpp`：安装管理器。
- `httptest.cpp`：HTTP 传输。

本项目用 GLib `GTest` 重写对应用例，并增加安全/取消/进度测试。

---

## 14. 结论

上游 SWORD 的功能与格式已经稳定，适合作为兼容目标；但其 C++ 对象模型、错误处理、全局状态和安装流程不适合直接暴露给 GObject 前端。`libsword` 应采用：

- 元数据与后端分离；
- GObject 公共 API + boxed 键；
- `GError`；
- 安全、原子的安装；
- 可测试的传输抽象；
- 明确 v1 范围，避免过滤器/搜索/加密拖慢核心目标。

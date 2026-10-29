# libsword 实现里程碑

> 与 `docs/design.md` 第 13 节对应。每完成一个里程碑形成一次可构建、可测试的提交。

## M0 设计与工程骨架

- [x] 阅读上游 SWORD 1.9 关键源码
- [x] 完成 `docs/design.md`、`docs/formats.md`、`docs/upstream-analysis.md`
- [x] 初始化 Git 仓库与 `.gitignore`
- [x] Meson 工程可配置、可编译
- [x] 公共头文件骨架 + `pkg-config` 文件
- [x] 示例程序可链接

验收：`meson setup build && meson compile -C build` 成功，安装产物包含 `libgsword.so`、头文件、`gsword.pc`。

## M1 配置与元数据

- [x] `GswConfig`：读取、保存、重复键、augment
- [x] `GswModuleInfo`：从 conf section 构造不可变元数据
- [x] `GswManager`：本地库发现、`mods.d/*.conf` 合并、模块列表
- [x] `GswManager`：按路径/配置文件构造，空库可用
- [x] 版本比较、路径安全、通用工具
- [x] 单元测试：配置、版本、路径、模块元数据

验收：能列出 `~/.sword` 中的模块名称、类型、驱动、版本，不打开模块数据文件；空目录也可构造 manager。

## M2 网络与安装

- [x] `GswStatusReporter`：`pre-status` / `progress` 信号、主上下文派发
- [x] `GswTransport`：libcurl HTTP/HTTPS/FTP、超时、取消、TLS 校验
- [x] 目录列表：HTTP autoindex、FTP MLSD/LIST 回退
- [x] `mods.d.tar.gz` 安全解包
- [x] `GswInstallSource`：`InstallMgr.conf` 格式解析/序列化
- [x] `GswInstallManager`：加载/保存配置、刷新源、列远端模块
- [x] `GswInstallManager`：安装、卸载、版本状态
- [x] 进度与取消
- [x] 集成测试：本地 HTTP 仓库，安装后校验文件与元数据

验收：从本地 HTTP fixture 仓库刷新源、列出模块、安装到临时 prefix、卸载；取消后无残留（集成测试已覆盖取消）。

## M3 经文键与文本模块

- [x] `GswKey` boxed 基类
- [x] `GswVerseKey`：解析、规范化、边界、偏移
- [x] `GswVersificationMgr`：KJV + 上游 canon_* 生成的全部常用系统
- [x] `RawText` / `RawText4` / `RawCom` / `RawCom4`
- [x] LZSS / ZIP 解码（BZIP2/XZ 可选实现）
- [x] `zText` / `zText4` / `zCom` / `zCom4`
- [x] 编码转换（UTF-8/Latin-1/UTF-16；SCSU 报不支持）
- [x] 与真实 KJV、ChiUns（NRSV）模块的人工对比

验收：读取 KJV、ChiUns2 指定经文，键规范化与条目文本正确。

## M4 字典、通用书与文本处理

- [x] `RawLD` / `RawLD4`
- [x] `zLD`
- [x] `EntriesBlock`
- [x] BZIP2 / XZ（可选依赖）
- [x] `GswStrKey`
- [x] `RawGenBook` / TreeKeyIdx / `GswTreeKey`
- [x] 基础 OSIS/ThML/GBF 纯文本剥离
- [x] 字典、zLD、通用书合成测试；真实 RawLD4/RawGenBook 人工验证

验收：读取 Strong's 字典、GerAugustinus 通用书、中文模块。

## M5 打磨与扩展

- [x] GIR/typelib（`-Dintrospection=true`）
- [x] 异步 API（GTask：刷新源、安装、卸载）
- [x] 文档与示例完善（模块信息、安装、异步安装、字典、通用书、章节、键解析）
- [x] 全部上游 versification 系统（canon_* 生成）
- [x] 本地化书名/缩写（`GswLocaleMgr`，支持 SWORD `locales.d`）
- [x] Sapphire 解密（zText/zCom/zLD/RawText/RawCom/RawLD/RawGenBook）
- [x] 基础渲染选项（Strong's、脚注、标题、red letter、morphology；OSIS/ThML/GBF）
- [x] zVerse 4 块缓存 + zLD 单块缓存

验收：GI 绑定可用；示例覆盖读取、安装、进度、取消、异步、渲染选项、本地化；加密模块可读。

## M6 增强扩展

- [x] SCSU -> UTF-8 解码
- [x] `RawFiles` 驱动
- [x] `HREFCom` 驱动
- [x] 基础搜索 API（phrase/multiword/regex）
- [x] `gsword-search` 示例与搜索测试
- [x] 搜索结果恢复当前键、跳过标题条目、索引越界安全处理
- [x] gi-docgen API reference（`-Ddocs=true`，`docs` 目标，安装到 `share/doc/gsword`）
- [x] Vala API（`-Dintrospection=true` 生成 `gsword.vapi`，安装到 `share/vala/vapi`）

## 后续路线：面向 GTK/Xiphos 类前端

以下里程碑按“先完成只读 GTK 阅读器 MVP，再补齐 Xiphos 级能力”的顺序安排。
每个里程碑完成后必须保持可构建、可测试，并同步更新公共头文件文档和示例。

## M7 多实例与阅读导航补全

目标：让同一模块可以同时出现在多个视图/窗格中，并补齐前后端都依赖的基础导航能力。

- [x] 公开 `gsw_module_new_from_info()`，允许从模块元数据创建独立模块实例
- [x] 新增 `gsw_manager_create_module()`，不进入 manager 的模块缓存
- [x] 新增 `GswKeyKind` 和 `gsw_key_get_kind()`，避免前端依赖驱动名称进行 key 强转
- [x] 完成 `RawVerse` / `zVerse` / `RawFiles` / `HREFCom` 的 `GSW_POSITION_BOTTOM`
- [x] BOTTOM 定位时跳过尾部空条目，定位到最后一个非空条目
- [x] 新增 `GswTestamentFlags`、`gsw_module_get_testaments()`、
      `gsw_module_has_testament()`
- [x] 增加独立模块实例、key kind、BOTTOM、testament 的回归测试
- [x] 增加 `gsword-parallel` 示例
- [x] `gsw_module_set_position()` 清除旧条目的 raw 缓存

验收：

- 同一模块创建两个实例，分别定位到不同经文，互不干扰。
- `gsw_module_set_position(BOTTOM)` 在 RawVerse/zVerse/RawFiles/HREFCom 上可定位到最后一个非空条目。
- Bible/commentary 模块可识别 OT/NT；字典和通用书返回 `GSW_TESTAMENT_NONE`。
- 普通测试、ASan 测试、GI 测试通过。

## M8 渲染与 EntryAttributes

目标：支撑 Xiphos 风格的富文本显示。

- [x] 增加中性渲染 API：HTML 片段、纯文本和 strip 文本
      （`gsw_module_get_render_html()`、`gsw_module_render_html()`、
      `gsw_module_render_text()`、`gsw_module_strip_text()`）
- [x] 增加 EntryAttributes API，支持 Heading/Preverse、脚注、交叉引用、图片等
      （`gsw_module_get_entry_attribute()`、
      `gsw_module_get_entry_attribute_keys()`）
- [x] 扩展 render options：Strong's、Morphology、Footnotes、Cross-references、
      Headings、Red Letter、Greek Accents、Hebrew Vowel Points、Transliteration、
      Morpheme Segmentation、Textual Variants、Glosses、Images
- [x] 增加全局 render options 与模块级覆盖
      （`gsw_manager_*_option()` + `gsw_module_*_option()`）
- [x] 保持 gsword 不依赖 GTK/WebKit
- [x] 增加 OSIS/ThML/GBF 风格标记的渲染、属性和选项回归测试

验收：OSIS/ThML/GBF 风格模块可输出可供 WebKit 使用的 HTML 片段，并读取条目属性。

说明：M8 完成的是基础渲染和属性层，不追求一次覆盖上游 SWORD 的全部过滤器语义；
Greek/Hebrew 重音、Textual Variants、Morpheme Segmentation 等选项已进入统一选项表，
后续可在 filter 层逐步细化。

## M9 经文范围、OSIS 与 TreeKey 补全

目标：支撑书签、导出、搜索范围、并行同步和通用书导航。

- [x] 新增经文 list/range 类型与解析 API
      （`GswVerseList`、`gsw_verse_list_parse()`、`gsw_verse_list_expand()`）
- [x] 暴露 VerseKey 上下界读取接口
      （`gsw_verse_key_get_lower_bound()`、`gsw_verse_key_get_upper_bound()`）
- [x] 增加 `gsw_verse_key_get_osis_ref()` 和短文本接口
      （`gsw_verse_key_get_short_text()`）
- [x] 增加全局 book number 转换辅助
      （`gsw_verse_key_get_global_book_number()`）
- [x] TreeKey 增加 offset、local name、previous sibling API
- [x] 增加经文范围、OSIS 和通用书遍历测试

验收：可解析 `John 3:16-18; Rom 8:1`，OSIS ref 可往返，RawGenBook 可完整遍历。

说明：当前 verse list 支持单个经文、范围、多段列表，以及省略书卷/章节的相对范围
（例如 `John 3:16-18`、`Gen 1:1-2:3`）。更复杂的 SWORD 简写语法可在后续扩展。

## M10 模块管理与加密完整性

目标：补齐 Xiphos 模块管理器的后端能力。

- [x] `GswSourceType` 增加 `DIR`，支持本地目录安装源
- [x] 增加本地目录安装 API
      （`gsw_install_manager_install_module_from_directory()`）
- [x] 增加运行期 CipherKey 设置与保存 API
      （`gsw_manager_set_module_cipher_key()`）
- [x] 允许安装 locked 模块，读取时再提示密钥
- [x] 增加模块配置更新 API，并支持保存后刷新 manager
      （`gsw_manager_set_module_config_value()`）
- [x] 增加全局 render options 的持久化入口
      （`gsw_manager_load_render_options()`、
      `gsw_manager_save_render_options()`）
- [x] 增加 DIR 源、本地安装、加密模块和选项持久化测试

验收：可从本地目录安装模块，可安装并解锁加密模块，修改配置后可重新加载。

说明：本地目录安装已完成；本地压缩归档安装暂未加入，留待后续与 zip/tar 归档
处理一起完善。

## M11 搜索 2.0

目标：把当前扫描式基础搜索提升到 Xiphos 前端可用水平。

- [x] 增加 `GswSearchOptions`：大小写、搜索类型、范围、最大结果数
- [x] 增加 `GswSearchResult`：模块名、key、预览文本、匹配片段
- [x] 增加 `GCancellable` 与进度回调
- [x] 增加多模块联合搜索
- [x] 增加 Strong's、Morphology、Footnote 属性搜索
- [x] 搜索期间保护模块当前 key，并明确线程模型

验收：覆盖 Xiphos 搜索对话框主要场景，可取消搜索，可限定范围并生成结果预览。

说明：M11 使用扫描式搜索引擎，不依赖索引；索引搜索仍属于 M13。搜索对象不是线程安全
的，推荐每个搜索任务使用独立模块实例。

新增 API：

- `GswSearchOptions` / `GswSearchResult`
- `gsw_module_search_full()`
- `gsw_manager_search()`
- `gsw_search_options_set_scope()`
- `gsw_search_options_set_attribute()`
- `GswSearchProgressFunc`

## M12 写入与模块编辑

目标：支撑个人笔记、Personal Commentary、祷告列表、日记和书籍编辑。

### M12a 写入核心

- [x] `gsw_module_set_entry()`
- [x] `gsw_module_delete_entry()`
- [x] RawFiles / Personal Commentary 写入
- [x] `gsw_module_is_writable()` 返回真实值
- [x] 内容文件/树结构使用临时文件替换
- [ ] 完整事务回滚和跨进程文件锁

### M12b RawGenBook 编辑

- [x] 创建 RawGenBook 模块（`gsw_manager_create_genbook()`）
- [x] `append_child()` / `append_sibling()`
- [x] `set_local_name()`
- [x] `remove_node()`（同时移除子树）
- [x] 创建模块配置文件和初始根节点
- [x] 树编辑即时写入，无需额外 `save()`

当前状态：M12a 内容写入和 M12b 基础树结构编辑已完成；完整事务回滚和文件锁
仍需在后续加固。树编辑当前会重写 `.idx`/`.dat` 并保留旧文件直到成功替换。

验收：可创建并编辑个人笔记和祷告列表，编辑结果可被上游 SWORD 读取，写入失败不破坏原模块。

## M13 索引、性能与 Xiphos 迁移验证

目标：达到 Xiphos 级体验并验证 API 可用性。

- [ ] 可选搜索索引框架（内部倒排索引或可选 CLucene）
- [ ] 创建/删除索引、索引进度
- [ ] 大模块搜索性能测试
- [ ] 块缓存、LRU、并发读取优化
- [ ] 用 gsword 编写最小 GTK 前端原型
- [ ] 或实现 Xiphos backend shim，验证功能覆盖

验收：大型模块搜索可接受；最小 GTK 原型覆盖模块列表、阅读、并行、搜索、安装和笔记。

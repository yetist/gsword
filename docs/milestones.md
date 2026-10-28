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

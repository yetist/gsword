# GSword 个人中心与祷告事项设计

- 状态：规划中，尚未实现
- 日期：2026-11-05
- 目标：在不增加主导航项的前提下，为 GSword 增加祷告事项管理能力
- 范围：个人内容架构、祷告事项数据模型、页面结构、实施步骤和验收条件

---

## 1. 设计目标

GSword 当前主导航为：

```text
阅读 | 对照 | 笔记 | 计划
```

祷告事项属于个人内容，但不适合继续增加一个主导航项，也不适合直接塞进现有笔记页：

- 主导航继续增加项会让窄窗口和底部导航变得拥挤；
- 现有笔记页右侧固定是笔记编辑器，无法仅通过增加左侧标签承载独立的祷告编辑器；
- 祷告事项有自己的状态、应允时间和生命周期，不是普通笔记或经文列表的变体。

本方案将“笔记”升级为“个人”中心：

```text
阅读 | 对照 | 个人 | 计划
```

“个人”页面内部切换：

```text
笔记 | 祷告
```

以后还可以继续容纳收藏、书签、灵修等内容，而不增加主导航项。

## 2. 概念边界

祷告事项与现有功能保持明确分工：

| 功能 | 语义 | 主要用途 |
|---|---|---|
| 笔记 | 绑定经文的个人注释 | 记录对某节经文的理解 |
| 经文列表 | 用户自定义的经文集合 | 主题查经、讲道素材、背诵清单 |
| 计划 | 阅读计划和进度 | 按天推进读经 |
| 祷告事项 | 有状态和时间的个人祷告记录 | 记录祷告、标记应允、归档 |
| 高亮 | 经文视觉标记 | 阅读时快速辨认 |
| 书签 | 阅读位置标记 | 回到上次或指定阅读位置 |

当前不实现独立的“收藏”和“书签”功能；祷告事项也不复用经文列表或笔记表。

## 3. 信息架构

### 3.1 主导航

主导航保持 4 项：

```text
阅读 | 对照 | 个人 | 计划
```

原 `Notes` 页面改名为 `Personal`：

- 中文：个人
- 英文：Personal
- 页面内部保留现有笔记功能和新增祷告功能

### 3.2 个人页

个人页顶部使用轻量模式切换：

```text
[笔记] [祷告]
```

两种模式各自使用完整内容区域：

- 笔记模式：保持现有笔记、标签、经文列表布局；
- 祷告模式：显示祷告列表和祷告详情编辑器。

建议使用 `GtkStack` + `GtkStackSwitcher` 或 `AdwViewSwitcher`，不把祷告作为笔记页左侧的第四个标签。

## 4. 祷告页界面

### 4.1 整体布局

```text
┌──────────────────────────────────────────────────────────────┐
│ 个人                                                         │
│ [笔记] [祷告]                                                │
├──────────────────────┬───────────────────────────────────────┤
│ 搜索祷告...           │ 为家人祷告                            │
│ 状态：[进行中 ▾]      │ 状态：[进行中 ▾]                      │
│                      │ 关联经文：腓 4:6-7                    │
│ 为家人祷告            │                                       │
│ 进行中 · 11-05        │ 标题：____________________________    │
│ 为工作祷告            │                                       │
│ 已应允 · 10-28        │ 正文编辑器                            │
│                      │ ...                                   │
│ [+ 新建祷告]          │                                       │
│                      │ 应允备注（状态为已应允时显示）        │
│                      │ ...                                   │
│                      │ [保存] [标记已应允] [归档] [删除]      │
└──────────────────────┴───────────────────────────────────────┘
```

### 4.2 左侧列表

支持：

- 搜索标题和正文；
- 状态筛选：
  - 进行中
  - 已应允
  - 已归档
  - 全部
- 排序：
  - 最近更新
  - 创建时间
  - 应允时间
- 新建祷告事项；
- 列表行显示标题、状态、更新时间和关联经文摘要。

### 4.3 右侧编辑器

字段：

- 标题；
- 正文；
- 状态；
- 应允日期；
- 应允备注；
- 关联经文；
- 创建时间和更新时间只读信息。

操作：

- 保存；
- 标记已应允；
- 归档；
- 删除；
- 打开关联经文；
- 移除关联经文。

### 4.4 空状态

- 没有祷告事项：显示“还没有祷告事项”和新建入口；
- 未选择事项：显示“选择一条祷告事项”；
- 搜索无结果：显示“没有匹配的祷告事项”。

## 5. 与阅读器和搜索的联动

### 5.1 阅读器

在阅读器“更多”菜单增加：

```text
添加到祷告
```

行为：

1. 打开“个人”页；
2. 切换到“祷告”模式；
3. 创建一条未保存草稿；
4. 自动关联当前经文；
5. 用户填写标题和正文后保存。

### 5.2 搜索结果

搜索结果右键菜单增加：

```text
添加到祷告
```

行为与阅读器一致，使用搜索结果所属模块和 key 作为关联经文。

### 5.3 经文关联

- 一个祷告事项可以关联零节、一节或多节经文；
- 关联经文不是必填项；
- 点击关联经文打开阅读器；
- 移除关联不影响祷告事项本身。

## 6. 数据模型

祷告数据存储在现有 `~/.local/share/gsword/app.db`，使用独立表。

```sql
CREATE TABLE IF NOT EXISTS prayers (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  title TEXT NOT NULL,
  body TEXT NOT NULL DEFAULT '',
  status TEXT NOT NULL DEFAULT 'active',
  created INTEGER NOT NULL,
  updated INTEGER NOT NULL,
  answered_at INTEGER,
  answered_note TEXT
);

CREATE TABLE IF NOT EXISTS prayer_entries (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  prayer_id INTEGER NOT NULL REFERENCES prayers(id) ON DELETE CASCADE,
  body TEXT NOT NULL,
  created INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS prayer_verses (
  prayer_id INTEGER NOT NULL REFERENCES prayers(id) ON DELETE CASCADE,
  module TEXT NOT NULL,
  key TEXT NOT NULL,
  display_key TEXT NOT NULL,
  PRIMARY KEY (prayer_id, module, key)
);

CREATE INDEX IF NOT EXISTS prayers_status_updated
  ON prayers(status, updated);
```

### 6.1 状态定义

- `active`：进行中；
- `answered`：已应允；
- `archived`：已归档。

首版不自动删除已应允事项，用户可手动归档或删除。

### 6.2 祷告日志

`prayer_entries` 首版可以只建表、不暴露 UI，用于后续增加：

- 每次祷告记录；
- 祷告时间线；
- 应允前后的过程记录。

## 7. 代码结构

建议新增：

```text
src/prayer-store.c
src/prayer-store.h
src/prayer-page.c
src/prayer-page.h
src/personal-page.c
src/personal-page.h
tests/test-prayer-store.c
```

### 7.1 数据对象

```c
typedef struct {
    gint64 id;
    gchar *title;
    gchar *body;
    gchar *status;
    gint64 created;
    gint64 updated;
    gint64 answered_at;
    gchar *answered_note;
} GswordPrayer;

typedef struct {
    gchar *module;
    gchar *key;
    gchar *display_key;
} GswordPrayerVerse;
```

### 7.2 Store 接口

```c
GswordPrayerStore *gsword_prayer_store_new(GError **error);

GPtrArray *gsword_prayer_store_list(GswordPrayerStore *self,
                                    const gchar *status,
                                    const gchar *search);

GswordPrayer *gsword_prayer_store_get(GswordPrayerStore *self, gint64 id);

gboolean gsword_prayer_store_save(GswordPrayerStore *self,
                                  GswordPrayer *prayer,
                                  GError **error);

gboolean gsword_prayer_store_delete(GswordPrayerStore *self,
                                    gint64 id,
                                    GError **error);

gboolean gsword_prayer_store_set_status(GswordPrayerStore *self,
                                        gint64 id,
                                        const gchar *status,
                                        const gchar *answered_note,
                                        GError **error);

GPtrArray *gsword_prayer_store_get_verses(GswordPrayerStore *self,
                                          gint64 prayer_id);

gboolean gsword_prayer_store_add_verse(GswordPrayerStore *self,
                                       gint64 prayer_id,
                                       const gchar *module,
                                       const gchar *key,
                                       const gchar *display_key,
                                       GError **error);
```

### 7.3 页面对象

```c
GswordPrayerPage *gsword_prayer_page_new(GswordWindow *window);
GtkWidget *gsword_prayer_page_widget(GswordPrayerPage *page);
void gsword_prayer_page_refresh(GswordPrayerPage *page);

GswordPersonalPage *gsword_personal_page_new(GswordWindow *window);
GtkWidget *gsword_personal_page_widget(GswordPersonalPage *page);
void gsword_personal_page_show_notes(GswordPersonalPage *page);
void gsword_personal_page_show_prayer(GswordPersonalPage *page);
```

## 8. 窗口整合

当前 `window.c` 直接创建 `GswordNotesPage` 并加入内容栈。新结构改为：

```text
window.c
├── reader_page
├── workspace_page
├── search_page
├── personal_page
│   ├── notes_page
│   └── prayer_page
└── plan_page
```

需要调整：

- 内容栈页面 ID 从 `notes` 改为 `personal`；
- 页面标题改为“个人 / Personal”；
- 兼容旧 `active-page=notes` 设置，将其映射到 `personal`；
- 保留 `notes_page` 指针，现有“打开笔记”等入口继续工作；
- 新增 `prayer_page` 指针和 `personal_page` 指针；
- 阅读历史、搜索、计划等跨页面调用改为通过 `personal_page` 切换到对应子页。

## 9. 实施步骤

### 第 1 步：数据层

- 新增 `prayer-store.c/.h`；
- 创建表；
- 实现 CRUD、状态切换、搜索、筛选、经文关联；
- 新增 `tests/test-prayer-store.c`；
- 加入 Meson。

验收：

- 新建、读取、更新、删除；
- 标记已应允和归档；
- 搜索标题和正文；
- 删除事项级联删除经文关联；
- ASan 下无泄漏。

### 第 2 步：祷告页

- 新增 `prayer-page.c/.h`；
- 左侧列表、搜索、状态筛选、新建按钮；
- 右侧标题、正文、状态、应允备注；
- 保存、归档、删除；
- 关联经文显示、打开、移除；
- 空状态和未保存状态提示。

### 第 3 步：个人页容器

- 新增 `personal-page.c/.h`；
- 顶部“笔记 / 祷告”切换；
- 内嵌现有笔记页和新的祷告页；
- 记忆上次选择的模式。

### 第 4 步：窗口整合

- 将内容栈中的 `notes` 替换为 `personal`；
- 更新页面标题、图标和页面 ID；
- 兼容旧 `active-page=notes`；
- 调整 `window-private.h` 和 `window-services.c`。

### 第 5 步：阅读器和搜索联动

- 阅读器更多菜单增加“添加到祷告”；
- 搜索结果右键菜单增加“添加到祷告”；
- 新增 `gsword_window_open_prayer` 或类似服务；
- 支持预填当前经文和模块 key。

### 第 6 步：测试、文档和翻译

- 单元测试：prayer store；
- 手工或自动页面 smoke test；
- 更新 `docs/app-design.md`；
- 更新 README；
- 更新 `po/POTFILES.in` 和 `po/zh_CN.po`；
- 运行普通测试和 ASan。

## 10. 首版范围

首版实现：

- [x] 祷告事项 CRUD
- [x] 进行中 / 已应允 / 已归档
- [x] 应允日期和应允备注
- [x] 搜索标题和正文
- [x] 状态筛选
- [x] 关联经文的添加、显示、打开和移除
- [x] 阅读器和搜索结果“添加到祷告”
- [x] 个人页内部“笔记 / 祷告”切换

首版暂不实现：

- 提醒和系统通知
- 重复祷告周期
- 祷告日志时间线 UI
- 标签系统
- 云端同步
- 多人共享
- 导出和打印

## 11. 风险与约束

- SQLite 仍由多个 store 独立打开同一 `app.db`，需要设置 `busy_timeout`，避免并发写入冲突；
- 祷告页和笔记页共用窗口服务，应避免直接访问窗口私有结构；
- 新增页面会扩大顶层 UI 结构，需要测试窄窗口和底部导航；
- 提醒功能涉及系统通知和后台生命周期，暂不纳入首版；
- 祷告内容属于隐私数据，默认只保存在本地数据库，不做隐式同步。

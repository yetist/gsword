# `src/window.c` 重构计划

## 文档状态

- 状态：阶段 10 完成（全部页面已拆分，窗口壳体已清理到目标规模）
- 验收基线：`docs/window-refactor-acceptance.md`
- 基线提交：`46b4e81 Compact chapter and verse path segments`
- 当前规模：`src/window.c` 约 747 行
- 目标规模：`src/window.c` 约 500–800 行
- 实施原则：行为不变、分阶段提交、每阶段可独立编译/测试/回滚

## 一、背景

`src/window.c` 已经同时承担窗口框架、阅读器、模块侧边栏、研究面板、多窗格工作区、搜索、笔记、计划、灵修、背经、教学、资源阅读、模块安装和首选项等职责。

主要维护问题：

1. 单文件超过 7000 行，定位和审查成本高。
2. 大量页面共享同一个 `GswordWindowPrivate`，字段边界不清晰。
3. 页面回调、异步安装回调、引用同步逻辑互相交织。
4. 修改一个页面时容易影响其他页面。
5. 翻译字符串、页面构建和业务逻辑混在一起。

主内容区当前使用 `GtkStack` + `GtkStackSwitcher`，不是 `GtkNotebook`。内部使用 `GtkNotebook` 的部分是：

- 计划页：今日 / 计划 / 灵修 / 背经
- 研究面板：注释 / Strong's / 交叉引用 / 词典

因此本次重构按“页面功能”拆分，同时覆盖这些内部 notebook 子页。

## 二、已确认决策

以下四项全部确认执行：

1. 采用推荐方案：直接按页面对象拆分，窗口只保留共享服务和页面装配。
2. 计划页拆分为：
   - `plan-page.c`
   - `devotion-page.c`
   - `memory-page.c`
3. 工作区拆分为：
   - `workspace-page.c`
   - `workspace-pane.c`
4. 每个阶段单独提交、单独可回滚，并独立完成编译、测试和手工验收。

## 三、重构目标

### 3.1 最终结构

```text
src/
├── window.c                     # 窗口壳体、页面装配、跨页面协调
├── window.h
├── window-private.h             # 只暴露共享服务，不暴露 GswordWindowPrivate
├── window-services.c            # 状态、错误、页面切换、模块/引用访问
├── module-sidebar.c             # 模块侧边栏
├── reader-page.c                # 阅读器、活动节、范围、高亮
├── reader-toolbar.c             # 复制、笔记、高亮、教学、研究切换、引用选择器
├── study-panel.c                # 注释、Strong's、交叉引用、词典
├── workspace-page.c             # 多窗格容器、布局、列数、同步滚动、持久化
├── workspace-pane.c             # 单个窗格、模块选择、引用选择器、拖拽
├── search-page.c                # 搜索和索引
├── notes-page.c                 # 笔记、标签、经文列表、导出
├── plan-page.c                  # 计划页容器和计划管理
├── devotion-page.c              # 每日灵修
├── memory-page.c                # 背经复习
├── library-page.c               # 模块库、远程/本地安装
├── teaching-page.c              # 教学布局
├── resource-page.c              # 通用书/地图等资源阅读
└── preferences-dialog.c         # 首选项
```

如果某些页面过小，可以在实施时合并，但不得重新形成 2000 行以上的单文件。

### 3.2 职责边界

`GswordWindow` 最终只保留：

- HeaderBar、侧边栏、`GtkStack`、状态栏；
- 页面创建和页面切换；
- 全局菜单和快捷键；
- 持有 `GswManager`、`GSettings`、当前模块；
- 协调页面之间的引用跳转和模块切换；
- 页面对象生命周期。

各页面拥有：

- 自己的控件指针；
- 自己的回调；
- 自己的局部状态；
- 自己的构建、刷新和销毁逻辑。

页面不得直接访问 `GswordWindowPrivate`，统一通过 `window-private.h` 中的共享服务访问窗口能力。

### 3.3 页面对象接口约定

推荐每个页面使用独立状态结构，并由窗口保存不透明指针：

```c
typedef struct _GswordSearchPage GswordSearchPage;

GswordSearchPage *gsword_search_page_new(GswordWindow *window);
GtkWidget *gsword_search_page_widget(GswordSearchPage *page);
void gsword_search_page_refresh(GswordSearchPage *page);
```

窗口私有结构保存：

```c
GswordSearchPage *search_page;
GswordNotesPage *notes_page;
GswordPlanPage *plan_page;
GswordWorkspacePage *workspace_page;
GswordReaderPage *reader_page;
```

页面根控件由窗口装配到 `GtkStack`，页面状态由页面自身管理并在销毁时释放。

### 3.4 共享服务接口

`window-private.h` 只暴露窄接口，例如：

```c
GswModule *gsword_window_get_module(GswordWindow *self);
GSettings *gsword_window_get_settings(GswordWindow *self);
GswordNotesStore *gsword_window_get_notes_store(GswordWindow *self);
GswordPlanStore *gsword_window_get_plan_store(GswordWindow *self);

void gsword_window_show_page(GswordWindow *self, const gchar *page);
void gsword_window_set_status_message(GswordWindow *self, const gchar *message);
void gsword_window_show_error_message(GswordWindow *self,
                                      const gchar *prefix,
                                      GError *error);
gboolean gsword_window_open_reference(GswordWindow *self,
                                      const gchar *reference);
```

约束：

- 不把完整 `GswordWindowPrivate` 暴露到页面头文件；
- 页面跳转统一走 `gsword_window_open_reference()` / `gsword_window_show_page()`；
- 页面刷新侧边栏或模块列表统一走窗口服务；
- 笔记和计划数据库仍由窗口统一持有，页面只使用指针。

## 四、阶段计划

每个阶段必须单独提交，提交信息建议使用：

```text
refactor(window): extract search page
```

每个阶段完成后执行：

```sh
meson compile -C build
meson test -C build
meson compile -C build-asan
ASAN_OPTIONS=detect_leaks=0 meson test -C build-asan --timeout-multiplier 2
```

并完成对应页面的手工验收。

---

### 阶段 0：建立基线和验收矩阵

- [x] 记录当前测试基线。
- [x] 列出每个页面的手工验收项。
- [x] 确认笔记库、计划库、安装异步回调、工作区持久化等关键路径。
- [x] 冻结该阶段之外的新功能改动。

产出：`docs/window-refactor-acceptance.md`；未改变代码结构。

基线：

```text
普通测试：25/25
ASan 测试：24/24
UI 启动烟雾测试：通过
src/window.c：7140 行
```

风险：低。

---

### 阶段 1：建立共享服务层

- [x] 新增 `window-services.c`。
- [x] 新增 `window-private.h`，仅放共享服务声明。
- [x] 迁移状态栏、错误显示、页面切换、通用视图/格式化辅助函数。
- [x] 暴露当前模块、设置、笔记库、计划库和当前引用访问接口。
- [x] 保留原有行为不变。

说明：当前引用相关函数仍暂时实现在 `window.c`，但已经通过
`window-private.h` 暴露；具体实现随阅读器页面在阶段 9 迁移。

产出：

```text
src/window-services.c
src/window-private.h
```

结果：

```text
src/window.c：7140 -> 7021 行
普通测试：25/25 通过
ASan 测试：24/24 通过
UI 烟雾测试：通过
```

风险：低。

---

### 阶段 2：拆分搜索页

- [x] 新增 `search-page.c` / `search-page.h`。
- [x] 迁移搜索输入、类型、大小写选项。
- [x] 迁移搜索结果列表和结果跳转。
- [x] 迁移索引构建/删除和索引状态。
- [x] 搜索页构建改为页面对象工厂。

产出：

```text
src/search-page.c
src/search-page.h
```

结果：

```text
src/window.c：7021 -> 6806 行
普通测试：25/25 通过
ASan 测试：24/24 通过
```

验收：

- 搜索普通文本；
- 搜索短语/多词/正则；
- 构建和删除索引；
- 点击结果返回阅读器；
- 索引状态显示正确。

风险：低。

---

### 阶段 3：拆分资源页和教学页

#### 资源页

- [x] 新增 `resource-page.c` / `resource-page.h`。
- [x] 迁移资源树、路径显示、文本/图片内容。
- [x] 迁移资源返回逻辑。

#### 教学页

- [x] 新增 `teaching-page.c` / `teaching-page.h`。
- [x] 迁移教学经文、教学笔记、全屏、加载、保存、返回。
- [x] 保持教学笔记与计划库的关联。

产出：

```text
src/resource-page.c
src/resource-page.h
src/teaching-page.c
src/teaching-page.h
```

结果：

```text
src/window.c：6806 -> 6268 行
普通测试：25/25 通过
ASan 测试：24/24 通过
```

验收：

- 通用书、地图资源打开和返回；
- 教学布局加载当前经文；
- 教学笔记保存/恢复；
- 全屏和返回阅读器正常。

风险：低到中。

---

### 阶段 4：拆分笔记页

- [x] 新增 `notes-page.c` / `notes-page.h`。
- [x] 迁移笔记搜索、笔记列表、标签列表、经文列表。
- [x] 迁移笔记保存、删除、列表创建、加入列表。
- [x] 迁移 Markdown 导出。
- [x] 页面通过共享服务获取当前模块/引用并打开阅读器。

产出：

```text
src/notes-page.c
src/notes-page.h
```

结果：

```text
src/window.c：6268 -> 5734 行
普通测试：25/25 通过
ASan 测试：24/24 通过
```

验收：

- 新建/编辑/删除笔记；
- 标签筛选；
- 经文列表创建和添加；
- 导出 Markdown；
- 当前节笔记入口正常。

风险：中。

---

### 阶段 5：拆分计划页、灵修页、背经页

#### 计划页

- [x] 新增 `plan-page.c` / `plan-page.h`。
- [x] 迁移计划选择、摘要、今日计划、完成状态、创建、删除。
- [x] 保留计划页的 `GtkNotebook` 容器。
- [x] 页面只通过共享服务打开引用。

#### 灵修页

- [x] 新增 `devotion-page.c` / `devotion-page.h`。
- [x] 迁移灵修模块选择、日期切换、灵修正文。

#### 背经页

- [x] 新增 `memory-page.c` / `memory-page.h`。
- [x] 迁移背经列表、遮罩、输入检查、复习、删除。

产出：

```text
src/plan-page.c
src/plan-page.h
src/devotion-page.c
src/devotion-page.h
src/memory-page.c
src/memory-page.h
```

结果：

```text
src/window.c：5734 -> 4508 行
普通测试：25/25 通过
ASan 测试：24/24 通过
```

验收：

- 创建计划并完成今日计划；
- 打开计划经文；
- 灵修切换模块/日期；
- 背经添加、复习、检查、删除。

风险：中。

---

### 阶段 6：拆分模块库和模块侧边栏

- [x] 新增 `library-page.c` / `library-page.h`。
- [x] 新增 `module-sidebar.c` / `module-sidebar.h`。
- [x] 将安装源、远程模块、本地安装、安装进度、取消操作迁入库页面。
- [x] 将模块分组、模块列表、模块选择迁入侧边栏模块。
- [x] 安装完成统一通过窗口服务刷新模块和侧边栏。

产出：

```text
src/library-page.c
src/library-page.h
src/module-sidebar.c
src/module-sidebar.h
```

结果：

```text
src/window.c：4508 -> 3490 行
普通测试：25/25 通过
ASan 测试：24/24 通过
```

验收：

- 刷新安装源；
- 刷新远程模块列表；
- 安装/更新/删除模块；
- 本地目录安装；
- 取消安装；
- 安装后侧边栏和当前模块状态正确。

风险：中到高，重点检查异步回调生命周期。

---

### 阶段 7：拆分研究面板

- [x] 新增 `study-panel.c` / `study-panel.h`。
- [x] 迁移注释、Strong's、交叉引用、词典。
- [x] 迁移跟随/固定状态和模块选择。
- [x] 研究面板通过共享引用上下文同步当前节。

产出：

```text
src/study-panel.c
src/study-panel.h
```

结果：

```text
src/window.c：3490 -> 2954 行
普通测试：25/25 通过
ASan 测试：24/24 通过
```

验收：

- 点击经文，注释跟随；
- 固定注释后不被翻章打断；
- Strong's 编号跳转；
- 交叉引用点击返回阅读器；
- 词典查询正常。

风险：中。

---

### 阶段 8：拆分多窗格工作区

- [x] 新增 `workspace-page.c` / `workspace-page.h`。
- [x] 新增 `workspace-pane.c` / `workspace-pane.h`。
- [x] `workspace-page` 负责容器、列数、同步滚动、保存恢复、并行模板。
- [x] `workspace-pane` 负责单窗格模块选择、引用选择器、拖拽、渲染、FOLLOW。

产出：

```text
src/workspace-page.c
src/workspace-page.h
src/workspace-pane.c
src/workspace-pane.h
```

结果：

```text
src/window.c：2954 -> 1636 行
普通测试：25/25 通过
ASan 测试：24/24 通过
```

验收：

- 添加/删除/清空窗格；
- 1–4 列切换；
- 同步滚动；
- FOLLOW 与非 FOLLOW；
- 拖拽排序；
- 工作区重启恢复；
- 并行模板创建两个窗格。

风险：高。

---

### 阶段 9：拆分阅读器

- [x] 新增 `reader-page.c` / `reader-page.h`。
- [x] 新增 `reader-toolbar.c` / `reader-toolbar.h`。
- [x] 迁移阅读文本、活动节、范围选择、高亮、阅读器点击事件。
- [x] 迁移引用选择器、复制、笔记、高亮、教学、研究面板切换。
- [x] 阅读器通过共享上下文发布当前引用变化。

产出：

```text
src/reader-page.c
src/reader-page.h
src/reader-toolbar.c
src/reader-toolbar.h
```

结果：

```text
src/window.c：1636 -> 905 行
普通测试：25/25 通过
ASan 测试：24/24 通过
```

验收：

- 引用选择器跳转；
- 点击节设置活动节；
- `Shift` 范围选择；
- 高亮切换；
- 复制范围文本；
- 研究面板同步；
- 工作区 FOLLOW 同步。

风险：高。

---

### 阶段 10：清理窗口壳体

- [x] `window.c` 只保留构建、页面注册、菜单/快捷键、首选项入口和跨页面协调。
- [x] 删除已迁走的静态函数。
- [x] 确保页面控件字段不再出现在 `GswordWindowPrivate`。
- [x] 更新 `src/README.md`、`docs/app-design.md` 和 `POTFILES.in`。
- [x] 确认 `window.c` 规模达到目标。

产出：

```text
src/preferences-dialog.c
src/preferences-dialog.h
src/README.md
docs/app-design.md
```

结果：

```text
src/window.c：905 -> 747 行
普通测试：25/25 通过
ASan 测试：24/24 通过
```

最终目标：

```text
window.c <= 800 行
单个页面文件尽量 <= 900 行
```

风险：中。

## 五、构建与翻译

每个新增源文件必须同步更新：

- `src/meson.build`
- `po/POTFILES.in`

每次迁移后：

```sh
meson compile -C build gsword-pot
msgmerge --update po/zh_CN.po po/gsword.pot
msgfmt -c -o /dev/null po/zh_CN.po
```

注意：

- 页面中的用户可见字符串继续使用 `_()`；
- 不改变已有 gettext msgid；
- 不删除仍然使用的翻译；
- 新增字符串补充中文翻译。

## 六、测试策略

### 自动化

每个阶段必须通过：

- 普通构建；
- 普通测试；
- ASan 构建；
- ASan 测试；
- `git diff --check`。

现有基线：

```text
普通测试：25/25
ASan 测试：24/24
```

如果阶段中新增页面级测试，应单独加入 Meson test。

### 手工

每个阶段至少验证：

- 页面能打开和切换；
- 页面刷新不崩溃；
- 状态栏更新正常；
- 当前模块/当前引用同步正常；
- 设置持久化正常；
- 对应异步操作正常结束。

建议使用：

```sh
xvfb-run -s '-screen 0 1500x1000x24' ...
```

配合 `xdotool` 做基本窗口和点击检查。

## 七、风险与缓解

| 风险 | 缓解措施 |
|---|---|
| 页面对象生命周期错误 | 页面根控件由窗口持有；页面状态随控件销毁释放 |
| 异步安装回调悬空 | 库页面持有安装管理器；卸载时取消 cancellable |
| 跨页面跳转耦合 | 所有跳转统一走 `gsword_window_open_reference()` |
| 阅读器/研究面板引用不同步 | 先建立单一下文接口，再拆阅读器 |
| 工作区持久化回归 | 保留现有 GSettings key，阶段 8 单独重点验收 |
| 翻译丢失 | 每个阶段更新 `POTFILES.in` 并重新生成 pot |
| 单阶段改动过大 | 每阶段一个提交，只做该阶段迁移，不顺带改行为 |
| 调试困难 | 保留阶段前后手工验收清单和截图 |

## 八、完成标准

重构完成时满足：

- [x] `window.c` 不超过约 800 行。
- [x] 不存在页面直接访问 `GswordWindowPrivate`。
- [x] 页面通过独立对象/状态和共享服务协作。
- [x] 计划页、研究面板子页已拆分。
- [x] 工作区拆分为 page 和 pane。
- [x] 每个阶段均有独立提交。
- [x] 普通测试和 ASan 测试通过。
- [x] 中文翻译和 `POTFILES.in` 完整。
- [x] 阅读、搜索、笔记、计划、灵修、背经、教学、资源、模块库、多窗格功能无回归。

## 九、实施进度

> 实施时在此记录每个阶段的状态和提交。

| 阶段 | 状态 | 提交 | 备注 |
|---|---|---|---|
| 0 基线 | 完成 | `5760c3b` | `docs/window-refactor-acceptance.md` |
| 1 共享服务 | 完成 | 本阶段提交 | `window-services.c` + `window-private.h` |
| 2 搜索页 | 完成 | 本阶段提交 | 新增 `search-page.c` / `search-page.h` |
| 3 资源页/教学页 | 完成 | 本阶段提交 | 新增 `resource-page.*` / `teaching-page.*` |
| 4 笔记页 | 完成 | 本阶段提交 | 新增 `notes-page.c` / `notes-page.h` |
| 5 计划/灵修/背经 | 完成 | 本阶段提交 | 新增 `plan-page.*` / `devotion-page.*` / `memory-page.*` |
| 6 模块库/侧边栏 | 完成 | 本阶段提交 | 新增 `library-page.*` / `module-sidebar.*` |
| 7 研究面板 | 完成 | 本阶段提交 | 新增 `study-panel.c` / `study-panel.h` |
| 8 工作区 | 完成 | 本阶段提交 | 新增 `workspace-page.*` / `workspace-pane.*` |
| 9 阅读器 | 完成 | 本阶段提交 | 新增 `reader-page.*` / `reader-toolbar.*` |
| 10 壳体清理 | 完成 | 本阶段提交 | `window.c` 747 行；新增 `preferences-dialog.*` |

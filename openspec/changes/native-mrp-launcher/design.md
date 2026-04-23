## 上下文

- 当前应用是单页面结构：Index.ets 内嵌 XComponent 运行 Unicorn 模拟器，启动时硬编码加载 `dsm_gm.mrp`（冒泡启动器）。
- MRP 文件头部（240 字节）包含 `appname`（GBK 24字节）、`appid`（uint32）、`version`（uint32）、`vendor`（GBK 40字节）、`description`（GBK 64字节）等结构化元数据。
- `napi.start(filename, ext)` 已参数化，可直接传入任意 MRP 文件名启动。
- `router.pushUrl()` 已在项目中使用（OpenSourceNotices 页面），传参模式成熟。
- 文件系统：MRP 文件存放在 `<sandbox>/mythroad/` 目录，由 EntryAbility 启动时从 rawfile 拷贝或用户通过 DocumentPicker 导入。

## 目标 / 非目标

**目标：**

- 用户打开应用后首先看到原生鸿蒙风格的 MRP 应用列表，展示每个 MRP 的应用名、版本、开发商等信息。
- 用户点击列表项后，跳转到模拟器页面并直接启动该 MRP（入口文件名默认 `"start.mr"`）。
- 支持从模拟器页返回列表页，再选择其他 MRP 启动。
- 支持在列表页导入新的 .mrp 文件。

**非目标：**

- 本变更不实现 MRP 图标展示（MRP 格式无图标字段）。
- 不实现 MRP 分类、搜索、排序功能（可后续迭代）。
- 不修改 Unicorn/Native 层代码——`napi.start()` 已满足需求。
- 不处理从列表页重新启动时 Unicorn 的生命周期重建（首次启动后的热切换行为不在本范围内，首次启动即销毁重建即可）。

## 决策

**决策 1：MRP 头部解析在 ArkTS 层完成**

- 直接用 `fileIo` 读取文件前 240 字节，按偏移解析为 `MrpInfo` 结构体。
- GBK 解码使用 `util.TextDecoder('gbk')`。
- 不增加 NAPI 接口。240 字节的解析在 ArkTS 层足够高效，且避免增加 Native 代码复杂度。

**决策 2：列表页为应用入口页，模拟器页改为子页面**

- 当前 `pages/Index` 是入口且包含模拟器 XComponent。改为：新增列表页作为入口，Index 重命名或保持但改为由列表页导航进入。
- 实际做法：新增 `pages/MrpList` 作为首页（在 `main_pages.json` 中排第一），`pages/Index` 作为模拟器页，通过 `router.pushUrl({ url: 'pages/Index', params: { mrp: 'xxx.mrp' } })` 传递选中文件名。
- 也可以反过来：保持 Index 为列表页（首次展示列表），点击后 XComponent 区域切换为模拟器。但这会让页面状态管理复杂化，不采用。

**决策 3：路由传参传递 MRP 文件名**

- 使用 `router.pushUrl` 的 `params` 传递 `{ mrp: string }`。
- Index 页的 `initEmulator` 读取参数：有 `mrp` 则启动指定文件，否则默认 `dsm_gm.mrp`。
- ext（入口扩展名）固定为 `"start.mr"`，不传参。大多数 MRP 的入口都是 `start.mr`。

**决策 4：页面切换时 Unicorn 生命周期**

- 从列表页进入模拟器页时，XComponent 创建 → `onLoad` → `initEmulator` → 全新初始化。
- 从模拟器页返回时，XComponent 销毁 → `OnSurfaceDestroyed`。下次进入重新初始化。
- 不做热切换复用，避免状态残留问题。

**决策 5：文件导入功能移至列表页**

- 当前 Index 页有 `pickMrpIntoMythroad` 方法（DocumentPicker 导入）。
- 将此功能移到列表页的 FAB / 按钮，导入后刷新列表。

## 风险 / 权衡

- **[风险] 部分 MRP 入口不是 `start.mr`** → **缓解**：固定传 `"start.mr"`，`_mr_intra_start` 内部会回退尝试 `logo.ext`；失败时模拟器页显示错误信息，用户可返回列表。后续可扩展为可配置入口名。
- **[风险] GBK 解码在某些 HarmonyOS 版本上可能不可用** → **缓解**：`util.TextDecoder` 在 API 9+ 支持 GBK；若不支持则 fallback 为 raw bytes 展示（显示文件名即可）。
- **[风险] `dsm_gm.mrp` 作为启动器可能提供公共库（commonv2.mr 等）给其他 MRP** → **缓解**：大多数独立游戏 MRP 不依赖启动器的公共库；若遇到问题可在后续迭代中将公共库提取为独立资源。
- **[权衡] 每次切换 MRP 都重新初始化 Unicorn → 启动慢** → 可接受，因为 XComponent 必须重建；后续可优化为在 Index 页内热切换。

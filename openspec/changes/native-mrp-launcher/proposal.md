## 为什么

当前应用启动时硬编码加载 `dsm_gm.mrp`（冒泡应用列表），用户必须在这个 MRP 内的 ARM 脚本菜单中导航才能选择游戏。这导致：启动慢（需要先加载启动器再加载游戏）；无法展示中文应用名等元数据（启动器 UI 限于 240×320 像素）；用户体验与原生鸿蒙应用割裂。MRP 文件的头部已包含 appname、version、vendor、description 等结构化信息，完全可以在原生 ArkTS 层直接解析并展示。

## 变更内容

- 新增 **MRP 列表页**（ArkTS）：扫描 `<sandbox>/mythroad/` 目录下所有 `.mrp` 文件，解析 MRPG 头部获取 appname / appid / version / vendor / description，以原生鸿蒙列表形式展示。
- 修改 **Index 页启动逻辑**：从硬编码 `dsm_gm.mrp` 改为接收路由参数 `mrp`（文件名），直接启动指定 MRP；无参数时保持原行为（启动 `dsm_gm.mrp`）。
- 支持从列表页导航到模拟器页并传递选中的 MRP 文件名；模拟器页返回时回到列表页。
- 保留现有的文件导入功能（DocumentPicker 导入 .mrp 到 mythroad/）。

## 功能 (Capabilities)

### 新增功能

- `mrp-native-list`: 原生 MRP 应用列表——扫描 mythroad 目录、解析 MRP 头部、展示应用信息、点击启动

### 修改功能

（无——本变更不修改现有规格的需求层面，仅新增页面和修改 Index 页的启动参数来源）

## 影响

- `entry/src/main/ets/pages/`：新增列表页文件；修改 Index.ets 的 `initEmulator` 方法读取路由参数
- `entry/src/main/resources/base/profile/main_pages.json`：注册新页面路由
- MRP 头部解析为纯 ArkTS 逻辑（读取 240 字节二进制），不涉及 Native/NAPI 变更
- `napi.start(filename, ext)` 已支持任意文件名，无需修改 Native 层

## 1. MRP 头部解析工具

- [x] 1.1 在 ArkTS 中实现 `parseMrpHeader(filePath: string): MrpInfo | null` 函数，读取文件前 240 字节，验证 Magic 为 `"MRPG"`，按偏移提取 filename（16, 12B）、appname（28, 24B, GBK）、appid（68, uint32 LE）、version（72, uint32 LE）、vendor（88, 40B, GBK）、description（128, 64B, GBK）；GBK 解码失败时 fallback 为 filename 字段
- [x] 1.2 实现 `scanMythroadDir(sandboxPath: string): MrpInfo[]` 函数，扫描 `<sandbox>/mythroad/` 下所有 `.mrp` 文件，对每个调用 `parseMrpHeader`，过滤无效文件并返回排序列表

## 2. 列表页 UI

- [x] 2.1 创建 `pages/MrpList.ets`，实现页面结构：顶部标题栏 + 可滚动列表 + 空状态提示 + FAB/按钮导入 .mrp；列表项展示 appname、version、vendor、description，无有效信息时展示文件名
- [x] 2.2 在列表项点击回调中调用 `router.pushUrl`，将选中 MRP 文件名传递给模拟器页
- [x] 2.3 在列表页实现导入功能：从 Index.ets 迁移 `pickMrpIntoMythroad` 和 `copyPickerUriToMythroad`，导入完成后自动刷新列表并显示 toast 提示

## 3. 路由与入口配置

- [x] 3.1 在 `main_pages.json` 中注册 `pages/MrpList` 并将其排在首位（作为应用入口页），确保 `pages/Index` 仍注册
- [x] 3.2 修改 Index.ets 的 `initEmulator` 方法：从 `router.getParams()` 读取 `params.mrp`，有值时调用 `napi.start(params.mrp, 'start.mr')`，无值时保持原行为 `napi.start('dsm_gm.mrp', 'start.mr')`

## 4. 页面生命周期处理

- [x] 4.1 确保从模拟器页（Index）按返回键时回到列表页（MrpList），而非退出应用；必要时覆写 Index 页的 `onBackPress` 或使用 `router.back()`
- [x] 4.2 确保 Index 页的 XComponent 在每次进入时重新初始化（`initialized` 标志在页面销毁时重置），避免从列表页再次进入时复用旧的 Unicorn 实例

## 5. 验证

- [ ] 5.1 启动应用验证列表页显示：能看到预装的 dsm_gm.mrp、ydqtwo.mrp、mpc.mrp 的应用名和版本信息（手测）
- [ ] 5.2 点击 ydqtwo.mrp 验证直接启动：跳转到模拟器页面并运行游戏，无需经过 dsm_gm.mrp 启动器（手测）
- [ ] 5.3 验证返回：在模拟器页按返回键回到列表页，再次选择另一个 MRP 可正常启动（手测）
- [ ] 5.4 验证导入：在列表页导入一个新的 .mrp 文件，确认列表自动刷新并显示新条目（手测）

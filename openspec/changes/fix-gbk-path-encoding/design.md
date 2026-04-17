## 上下文

vmrp 使用 Unicorn 引擎模拟 ARM 指令。MRP 应用的 ARM 代码通过 `DSM_REQUIRE_FUNCS` 结构中的函数指针调用宿主平台功能。在 Unicorn 模式下，这些函数指针指向 `bridge.c` 中的 `br_*` 回调函数（如 `br_mr_open`），而非 `dsm.c` 中的高层函数。

`dsm.c` 的 `get_filename()` 和 `mr_findGetNext()` 虽有 GBK↔UTF-8 转换逻辑，但在 Unicorn 模式下**不在调用链路中** — MRP ARM 代码直接通过 hook 调用 `br_mr_open` → `my_open`，完全绕过 `dsm.c`。

## 目标 / 非目标

**目标：**
- 使包含中文字符的文件路径在 HarmonyOS（UTF-8 文件系统）上正常工作
- 在 bridge 层实现 GBK↔UTF-8 双向转换

**非目标：**
- 不修改 `fileLib.c` 或 `encode.c` 的转换逻辑
- 不处理非 GBK 编码的 MRP 应用

## 决策

**决策：在 bridge.c 中添加编码转换辅助函数，在每个接受文件路径的 br_* 函数中调用**

在 bridge.c 中新增两个辅助函数：
- `gbkToUtf8(gbStr)` — GBK→UCS2BE→UTF-8，用于 MRP→文件系统方向
- `utf8ToGbk(utf8Str)` — UTF-8→GBK，用于 文件系统→MRP 方向（readdir）

复用 `encode.c` 中已有的转换函数：`GBStrToUCS2BEStr`、`UCS2BEStrToUTF8Str`、`UTF8StrToGBStr`。

需要转换的 bridge 函数：
- **GBK→UTF-8**：`br_mr_open`、`br_info`、`br_mr_remove`、`br_mr_rename`（两个参数）、`br_mr_mkDir`、`br_mr_rmDir`、`br_opendir`、`br_mr_getLen`
- **UTF-8→GBK**：`br_readdir`（返回的 d_name）

同时保留 `bridge_init()` 中设置的 `FLAG_USE_UTF8_FS | FLAG_USE_UTF8_EDIT`，虽然 Unicorn 模式下 dsm.c 不在调用链中，但保持一致性。

## 风险 / 权衡

- **内存泄漏** → 辅助函数返回 malloc 分配的字符串，调用者必须 free → 使用 static buffer 或确保每个路径都 free
- **纯 ASCII 路径** → GBK 和 UTF-8 对 ASCII 子集一致，转换是恒等操作，开销可忽略
- **非 bridge 路径** → `cfunction.ext` 加载和 rawfile 复制不经过 bridge，不受影响
- **buffer 长度** → 使用 static buffer（2048 字节），与 fileLib.c 的 mapPath 一致

## 为什么

MRP 应用的文件路径字符串使用 GBK 编码（功能手机时代标准），而 HarmonyOS 文件系统使用 UTF-8 编码。当前 `bridge_init()` 将 flags 设为 `FLAG_USE_UTF8_EDIT`（仅处理编辑框编码），未设置 `FLAG_USE_UTF8_FS`（文件系统编码）。纯 ASCII 路径可以正常工作，但任何包含中文的文件路径（存档名、MRP 文件名、目录名）都会导致文件操作失败。

## 变更内容

- 在 `bridge_init()` 中将 `flags` 从 `FLAG_USE_UTF8_EDIT` 改为 `FLAG_USE_UTF8_FS | FLAG_USE_UTF8_EDIT`
- vmrp 的 DSM 层已内置完整的 GBK↔UTF-8 双向转换逻辑（`get_filename()` 和 `mr_findGetNext()`），设置 flag 即可启用

## 功能 (Capabilities)

### 新增功能

无

### 修改功能

无

## 影响

- `entry/src/main/cpp/vmrp/bridge.c` — `bridge_init()` 函数，修改 flags 赋值（1 行）
- 所有经过 DSM 层的文件操作（`mr_open`、`mr_findStart`、`mr_rename`、`mr_mkDir` 等）将自动获得编码转换

# path-encoding

## Requirements

### 需求:文件路径编码必须兼容 UTF-8 文件系统
MRP 应用通过 DSM 层访问文件时，GBK 编码的文件路径必须自动转换为 UTF-8 后再传递给 POSIX 文件系统 API。目录枚举返回的 UTF-8 文件名必须转换回 GBK 后再传递给 MRP 应用。

#### 场景:MRP 应用打开含中文路径的文件
- **当** MRP 应用调用 `mr_open("存档/第一章.sav")`，其中路径为 GBK 编码
- **那么** DSM 层的 `get_filename()` 将 GBK 路径转换为 UTF-8，`my_open()` 收到 UTF-8 编码的路径，文件操作成功

#### 场景:MRP 应用枚举含中文文件名的目录
- **当** MRP 应用调用 `mr_findStart()` 枚举目录，目录中包含 UTF-8 编码的中文文件名
- **那么** `mr_findGetNext()` 将 `readdir()` 返回的 UTF-8 文件名转换为 GBK，MRP 应用正确显示中文文件名

#### 场景:纯 ASCII 路径不受影响
- **当** MRP 应用打开纯 ASCII 路径的文件（如 `cfunction.ext`）
- **那么** 编码转换为恒等操作，行为与未启用转换时完全一致

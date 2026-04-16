## 新增需求

### 需求:NAPI 初始化接口
系统必须暴露 NAPI init 函数，接收 ArkTS 层传入的沙箱路径，完成模拟器初始化。

#### 场景:正常初始化
- **当** ArkTS 层调用 init(sandboxPath) 并传入有效的沙箱路径
- **那么** 系统必须创建 Unicorn 引擎实例、初始化内存管理器、加载 cfunction.ext 到模拟内存、初始化 bridge 函数表

#### 场景:初始化失败
- **当** cfunction.ext 文件不存在或 Unicorn 初始化失败
- **那么** 必须返回错误码，ArkTS 层能获取到错误信息

### 需求:文件系统路径映射
系统必须将 MRP 应用的文件路径映射到 HarmonyOS 沙箱目录。

#### 场景:相对路径访问
- **当** ARM 代码请求打开相对路径文件（如 "dsm_gm.mrp"）
- **那么** 系统必须将其解析为 `{sandboxPath}/dsm_gm.mrp` 并执行文件操作

#### 场景:绝对路径访问
- **当** ARM 代码请求打开绝对路径文件（如 "/mythroad/system/gb12.uc2"）
- **那么** 系统必须将前导 "/" 替换为沙箱路径，解析为 `{sandboxPath}/mythroad/system/gb12.uc2`

#### 场景:目录操作
- **当** ARM 代码请求创建目录或列举目录内容
- **那么** 系统必须在沙箱路径下正确执行 mkdir、opendir、readdir 等操作

### 需求:Bridge 函数桥接
系统必须拦截 ARM 代码的宿主函数调用，将其转发到 HarmonyOS 原生实现。

#### 场景:内存操作拦截
- **当** ARM 代码调用 mr_malloc、mr_free、memcpy、memset
- **那么** 系统必须在模拟内存空间中正确分配/释放/操作内存

#### 场景:文件操作拦截
- **当** ARM 代码调用 mr_open、mr_close、mr_read、mr_write、mr_seek、mr_getLen
- **那么** 系统必须通过路径映射后执行对应的 POSIX 文件操作

#### 场景:未实现函数拦截
- **当** ARM 代码调用 mr_table_funcMap 中标记为 NULL 的函数
- **那么** 系统必须记录日志但不崩溃，返回 MR_FAILED 或 MR_IGNORE

### 需求:MRP 加载启动
系统必须能加载指定 MRP 文件并启动执行。

#### 场景:正常启动
- **当** ArkTS 层调用 start("dsm_gm.mrp", "start.mr")
- **那么** 系统必须通过 bridge 调用 ARM 层的 dsm_init 和 mr_start_dsm，开始模拟执行

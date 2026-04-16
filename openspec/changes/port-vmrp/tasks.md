## 1. 环境准备与资源文件

- [x] 1.1 将 Unicorn Engine 源码下载并集成到项目中（entry/src/main/cpp/unicorn/）
- [x] 1.2 配置 CMakeLists.txt 编译 Unicorn Engine 为 HarmonyOS 原生库
- [x] 1.3 将 vmrp 核心 C 源码复制到项目中（entry/src/main/cpp/vmrp/）
- [x] 1.4 准备资源文件到 rawfile 目录（cfunction.ext、字体、mrp 文件）

## 2. C 层核心移植

- [x] 2.1 修改 CMakeLists.txt，将 vmrp 核心源文件和 Unicorn 加入编译，链接 libz.so
- [x] 2.2 适配文件系统：修改 fileLib.c 添加沙箱路径映射（处理绝对路径前导 `/` 和相对路径）
- [x] 2.3 移除 SDL2 依赖：替换 main.c 中的 SDL 初始化、事件循环、guiDrawBitmap 等
- [x] 2.4 移除 pthread 依赖中的平台特定代码（如有），确认 pthread_mutex 在 HarmonyOS NAPI 可用
- [x] 2.5 处理编译错误：修复头文件路径、平台宏、缺少的依赖等，确保整体编译通过

## 3. NAPI 接口层

- [x] 3.1 重写 napi_init.cpp：注册 init(sandboxPath)、start(filename, extName)、event(code, p0, p1)、timer() 四个 NAPI 函数
- [x] 3.2 实现 init()：接收沙箱路径，初始化 Unicorn 引擎、bridge、加载 cfunction.ext
- [x] 3.3 实现 start()：调用 bridge_dsm_init 和 bridge_dsm_mr_start_dsm 启动 MRP 执行
- [x] 3.4 实现 event() 和 timer()：将 ArkTS 层的事件和定时器回调传递到 C 层的 event()/timer()

## 4. 画面渲染（C → ArkTS 回调）

- [x] 4.1 实现 napi_threadsafe_function 回调机制：C 层在 mr_drawBitmap 被调用时通知 ArkTS 层
- [x] 4.2 定义回调数据结构：传递 RGB565 像素缓冲区、坐标(x,y)、尺寸(w,h)
- [x] 4.3 替换 bridge.c 中 guiDrawBitmap 为 NAPI 回调实现

## 5. 定时器（C → ArkTS 回调）

- [x] 5.1 实现 timerStart/timerStop 的 NAPI 回调：C 层通知 ArkTS 层启动/停止 setTimeout
- [x] 5.2 替换 bridge.c 中 timerStart/timerStop 为 NAPI 回调实现

## 6. ArkTS UI 层

- [x] 6.1 重写 Index.ets：使用 XComponent 作为渲染区域，添加虚拟按键布局
- [x] 6.2 实现 XComponent 渲染：接收 C 层回调的 RGB565 数据，转换为 RGBA 写入 Canvas
- [x] 6.3 实现触屏事件：XComponent 的 onTouch 回调中计算坐标缩放，调用 NAPI event()
- [x] 6.4 实现按键事件：虚拟按键的 onClick 回调中映射为 MR_KEY_* 枚举，调用 NAPI event()
- [x] 6.5 实现定时器：收到 C 层 timerStart 回调后用 setTimeout/setInterval 启动，触发时调用 NAPI timer()

## 7. 资源文件管理

- [x] 7.1 实现 rawfile 资源释放：EntryAbility.onCreate 中将 rawfile 资源复制到沙箱目录
- [x] 7.2 传递沙箱路径给 C 层：初始化时将 context.filesDir 传入 NAPI init()

## 8. 集成验证

- [x] 8.1 编译整个项目，修复所有编译错误
- [ ] 8.2 部署到模拟器/设备，验证启动流程（init → loadCode → bridge_ext_init → bridge_dsm_init）
- [ ] 8.3 验证画面渲染：确认 MRP 应用的画面正确显示在 XComponent 上
- [ ] 8.4 验证触屏/按键输入：确认触摸和按键事件正确传递到模拟器

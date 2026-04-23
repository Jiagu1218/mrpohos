## 为什么

当前 MRP 模拟器使用 GLES 3.0 渲染 RGB565 帧到屏幕，功能完整但存在局限性：
- HarmonyOS 官方明确支持 Vulkan 1.3（`libvulkan.so` + `VK_OHOS_surface` 扩展），且提供了 `VK_OHOS_external_memory` 可与 `OH_NativeBuffer` 零拷贝共享
- Vulkan 后端可为未来多实例渲染、计算着色器加速（如 RGB565→RGBA8888 转换）提供基础
- 渲染管线极简（1 纹理 + 1 四边形 + 1 draw call），Vulkan 迁移成本低、风险可控
- 不同设备/模拟器 GPU 能力差异大（部分模拟器无 Vulkan，部分设备 GLES 驱动有问题），需要运行时自动选择最优后端
- 用户可能需要手动选择渲染模式（测试/调试/性能对比），所选模式不支持时自动降级并提示

## 变更内容

- 新增 Vulkan 渲染后端 `mrp_vulkan_renderer.h` / `mrp_vulkan_renderer.cpp`
- 新增渲染调度层 `mrp_renderer.h` / `mrp_renderer.cpp`，统一管理 Vulkan → GLES → CPU 三级降级
- 新增 NAPI 接口：`setRendererMode(mode)` / `getRendererMode()` / `getActiveRendererMode()`
- `CMakeLists.txt` 同时编译三个模块，链接 `libvulkan.so` + `libGLESv3.so`
- `napi_init.cpp` 改为调用 `mrp_renderer_*` 调度层接口，导出渲染模式相关 NAPI 函数
- Vulkan 后端使用 `VK_OHOS_surface` 从 `OHNativeWindow` 创建 `VkSurfaceKHR`
- 纹理上传使用 `VK_FORMAT_R5G6B5_UNORM_PACK16` 直接上传 RGB565 数据
- UI 侧：在手机外壳右侧添加"渲染"侧键，点击弹出渲染模式选择面板，显示当前活跃模式和可选模式

## 功能 (Capabilities)

### 新增功能
- `vulkan-renderer`: Vulkan 渲染后端 + 运行时三级降级调度（Vulkan → GLES → CPU）+ 用户可选手动模式 + 当前模式显示

### 修改功能

（无已有功能需求变更，GLES 和 CPU 路径代码不变）

## 影响

- `entry/src/main/cpp/CMakeLists.txt` — 同时编译三模块，链接 `libvulkan.so` + `libGLESv3.so`
- `entry/src/main/cpp/mrp_renderer.h` — 新文件（调度层接口）
- `entry/src/main/cpp/mrp_renderer.cpp` — 新文件（降级调度逻辑）
- `entry/src/main/cpp/mrp_vulkan_renderer.h` — 新文件
- `entry/src/main/cpp/mrp_vulkan_renderer.cpp` — 新文件
- `entry/src/main/cpp/napi_init.cpp` — 调用方从 `mrp_gles_renderer_*` 改为 `mrp_renderer_*`，新增 `setRendererMode` / `getRendererMode` / `getActiveRendererMode` NAPI 导出
- `entry/src/main/cpp/mrp_gles_renderer.h` / `.cpp` — 不修改（作为降级后端原样保留）
- `entry/src/main/ets/pages/Index.ets` — 新增"渲染"侧键 + 渲染模式选择面板 UI
- 依赖系统库 `libvulkan.so`（HarmonyOS API 10+ 内置，无需额外打包）

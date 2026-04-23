## 上下文

MRP 模拟器当前使用 GLES 3.0 后端渲染 240×320 RGB565 帧到屏幕（`mrp_gles_renderer.cpp`，约 640 行）。渲染管线极简：1 个 RGB565 纹理 → 1 个全屏四边形 → `eglSwapBuffers`。

HarmonyOS 从 API 10 起内置 `libvulkan.so`，支持 Vulkan 1.3，并提供 `VK_OHOS_surface` 和 `VK_OHOS_external_memory` 平台扩展。现有 `OHNativeWindow`（来自 XComponent `OnSurfaceCreated` 回调）可直接传给 `vkCreateSurfaceOHOS` 创建 Vulkan surface。

当前渲染器接口（`mrp_gles_renderer.h`）仅暴露 4 个函数：
- `mrp_gles_renderer_init(OHNativeWindow*, int32_t w, int32_t h)` — 登记 surface
- `mrp_gles_renderer_shutdown()` — 释放资源
- `mrp_gles_renderer_is_ready()` — 查询状态
- `mrp_gles_renderer_present_rgb565(const uint16_t*, int32_t w, int32_t h)` — 上传并呈现一帧

## 目标 / 非目标

**目标：**
- 实现 Vulkan 渲染后端，与 GLES 后端接口一致
- 运行时三级降级链：**Vulkan → GLES → CPU NativeWindow**
- 两个 GPU 后端同时编译，运行时自动选择最优可用后端
- 直接上传 RGB565（`VK_FORMAT_R5G6B5_UNORM_PACK16`）到纹理，与 GLES 路径一致
- `napi_init.cpp` 的 `onDrawCallback` 调用统一的渲染调度层，调度层负责降级逻辑

**非目标：**
- 不替换 GLES 后端，两者并行存在
- 不实现多实例渲染或计算着色器（留作未来扩展）
- 不使用 `VK_OHOS_external_memory` 零拷贝（当前帧数据在用户空间，无 `OH_NativeBuffer` 来源）
- 不实现 Vulkan 验证层或调试工具集成

## 决策

### D1: 直接链接 libvulkan.so

**选择**: CMake `target_link_libraries(... libvulkan.so)` + `#include <vulkan/vulkan.h>` 直接调用

**理由**: 官方文档示例即为此方式。直接链接代码量最少，编译期类型安全。`dlopen` 仅在"手动动态加载"场景下使用——官方提示"若用 dlopen 则不要再在 CMake 中加依赖以避免符号冲突"，这是说两种方式选其一，并非要求必须 dlopen。

**替代方案**: `dlopen` + `dlsym` 逐函数加载 — 代码量巨大（80+ 函数指针），无编译期类型检查，无必要。

### D2: 用户可选渲染模式 + 自动降级

**选择**: 用户可在 UI 中选择渲染模式（自动/Vulkan/GLES/CPU），所选模式不可用时自动降级到下一级

**渲染模式枚举**（Native 侧 `int`，ArkTS 侧对应）:
| 值 | 模式 | 行为 |
|----|------|------|
| 0 | `AUTO` | 按优先级 Vulkan → GLES → CPU 依次尝试，默认值 |
| 1 | `VULKAN` | 仅尝试 Vulkan，失败则降级 GLES → CPU |
| 2 | `GLES` | 仅尝试 GLES，失败则降级 CPU |
| 3 | `CPU` | 强制 CPU NativeWindow 路径 |

**降级规则**: 用户选择某模式后，该模式作为起始点，仍按 Vulkan→GLES→CPU 降级链向下寻找可用后端。例如选择 `GLES` 但 GLES 不可用时降级到 CPU，不会向上尝试 Vulkan。

**理由**: 自动模式适合普通用户（无需了解后端差异），手动模式适合开发者/高级用户（测试特定后端、性能对比、调试渲染问题）。

**模式持久化**: 通过 `@ohos.data.preferences` 保存用户选择，应用重启后恢复。`AUTO` 为首次安装默认值。

**替代方案**: 仅自动降级无手动选项 — 无法满足调试/测试需求。

### D3: 后端抽象层

**选择**: 新增 `mrp_renderer.h` 统一调度层，内部持有 Vulkan/GLES 两套后端实例，按优先级选择

**理由**: 现有 `mrp_gles_renderer.h` 的 4 函数接口由调度层对外暴露，Vulkan 和 GLES 后端各自实现独立的初始化/渲染/关闭函数。调度层按 Vulkan→GLES→CPU 顺序尝试，首次成功即锁定。`napi_init.cpp` 调用调度层接口，不直接接触任何后端。

**替代方案**: 在 `napi_init.cpp` 中写降级逻辑 — 污染调用方代码，不符合关注点分离。

### D4: Swapchain 管理

**选择**: 标准 `vkCreateSwapchainKHR`，双缓冲（`minImageCount=2`），`VK_FORMAT_B8G8R8A8_UNORM`

**理由**: BGRA8888 是 HarmonyOS 原生窗口默认格式，移动端事实标准。双缓冲足够（240×320，内存无压力）。

### D5: 纹理格式

**选择**: `VK_FORMAT_R5G6B5_UNORM_PACK16`，`VK_IMAGE_TILING_LINEAR` + `VK_IMAGE_USAGE_SAMPLED_BIT`

**理由**: 与 GLES 路径一致，直接上传 RGB565 无需 CPU 转换。`LINEAR` tiling 允许 `memcpy` 式上传。

### D6: 帧同步

**选择**: `VkFence` 帧间同步，`vkAcquireNextImageKHR` → `vkQueueSubmit` → `vkQueuePresentKHR` 标准三步

**理由**: 最简单的同步模型，1 fence 足够（双缓冲）。

### D7: 渲染模式 UI 与 NAPI 接口

**选择**: 在手机外壳右侧"关于"侧键下方新增"渲染"侧键，点击弹出面板显示可选模式和当前活跃模式

**NAPI 接口**（新增 3 个导出）:
- `setRendererMode(mode: number): void` — 设置首选渲染模式（下次 surface 创建时生效）
- `getRendererMode(): number` — 获取用户设置的首选模式
- `getActiveRendererMode(): number` — 获取当前实际使用的活跃后端（0=未初始化, 1=Vulkan, 2=GLES, 3=CPU）

**UI 面板**: 使用 `CustomDialogController` 弹出面板（与现有 edit/message dialog 风格一致），4 个 Radio 选项（自动/Vulkan/GLES/CPU），底部显示当前活跃后端文字（如"当前: GLES (Vulkan 不可用)"）。

**模式切换时机**: `setRendererMode` 保存偏好但不立即重建管线，下次 `OnSurfaceChanged` 或 `OnSurfaceCreated` 回调时重新初始化。无需手动重启应用。

**替代方案**: 设置页面（独立 page）— 过重，渲染模式是一个低频操作，弹窗即可。

## 风险 / 权衡

**[R1] 模拟器 Vulkan 支持不完整** → 降级到 GLES 或 CPU。缓解：Vulkan 初始化时全面检查（instance、device、format），任何一步失败即跳过。

**[R2] RGB565 纹理格式支持** → `VK_FORMAT_R5G6B5_UNORM_PACK16` 不保证所有设备支持 `SAMPLED`。缓解：`vkGetPhysicalDeviceFormatProperties` 检查，不支持则降级 GLES。

**[R3] 同时链接 libvulkan.so 和 libGLESv3.so** → 两个库无符号冲突（不同命名空间），可安全共存。权衡：包体积略增（两个 so 都加载），但 HarmonyOS 上两者均为系统库，不占额外空间。

**[R4] 线程安全** → 与现有机制一致，`present` 必须在同一线程调用。保留 `pthread_self()` 检查。

**[R5] 代码量** → Vulkan 后端约 800-1000 行 + 调度层约 100 行。权衡：一次性成本，后续维护简单。

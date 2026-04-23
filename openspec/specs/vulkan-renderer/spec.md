## 新增需求

### 需求:渲染后端运行时降级
系统必须在运行时根据用户选择的渲染模式，从该模式起始按 Vulkan → GLES → CPU 顺序尝试初始化，首个成功者被锁定。AUTO 模式从 Vulkan 开始尝试。

#### 场景:AUTO 模式 Vulkan 成功
- **当** 用户选择 AUTO（默认），设备支持 Vulkan
- **那么** 锁定 Vulkan 后端

#### 场景:AUTO 模式 Vulkan 失败降级 GLES
- **当** 用户选择 AUTO，Vulkan 不可用
- **那么** 记录日志，尝试 GLES，若成功则锁定 GLES

#### 场景:所有 GPU 后端均失败
- **当** Vulkan 和 GLES 初始化均失败
- **那么** 使用 CPU NativeWindow 路径，应用正常运行不崩溃

#### 场景:用户选择 VULKAN 但不可用
- **当** 用户选择 VULKAN 模式但设备不支持 Vulkan
- **那么** 降级到 GLES，若 GLES 也失败则降级到 CPU

#### 场景:用户选择 GLES 但不可用
- **当** 用户选择 GLES 模式但 GLES 初始化失败
- **那么** 降级到 CPU

#### 场景:用户选择 CPU
- **当** 用户选择 CPU 模式
- **那么** 直接使用 CPU NativeWindow 路径，不尝试任何 GPU 后端

### 需求:用户可选择渲染模式
系统必须允许用户通过 UI 选择首选渲染模式（AUTO/VULKAN/GLES/CPU），选择通过 Preferences 持久化，应用重启后恢复。默认为 AUTO。

#### 场景:用户选择模式并持久化
- **当** 用户在渲染模式面板选择某个模式
- **那么** 调用 `setRendererMode(mode)`，值保存到 Preferences，下次 Surface 创建时按新模式初始化

#### 场景:应用重启恢复模式
- **当** 应用冷启动
- **那么** 从 Preferences 读取上次选择的模式，若未保存过则默认 AUTO

### 需求:显示当前活跃渲染模式
系统必须通过 `getActiveRendererMode()` 返回当前实际使用的后端（0=未初始化, 1=Vulkan, 2=GLES, 3=CPU），UI 必须在渲染模式面板中显示此信息。

#### 场景:显示活跃后端
- **当** 渲染模式面板打开
- **那么** 面板底部显示当前实际使用的后端文字，如"当前: Vulkan"或"当前: GLES (Vulkan 不可用)"

#### 场景:降级时显示原因
- **当** 用户选择的模式降级到更低级后端
- **那么** 显示降级提示，如"当前: CPU (Vulkan 和 GLES 均不可用)"

### 需求:Vulkan 渲染器初始化
系统必须通过 CMake 链接 `libvulkan.so`，使用 `<vulkan/vulkan.h>` 调用 Vulkan API，创建完整的 Vulkan 渲染管线。

#### 场景:Vulkan 完整初始化
- **当** 设备支持 Vulkan 1.x、RGB565 纹理格式可用、OHNativeWindow 有效
- **那么** 创建 Instance → Surface → PhysicalDevice → Device → Swapchain → RenderPass → Pipeline → Framebuffers → CommandBuffers → Fence，返回成功

#### 场景:RGB565 纹理格式不支持
- **当** `vkGetPhysicalDeviceFormatProperties` 报告 `VK_FORMAT_R5G6B5_UNORM_PACK16` 不支持 `VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT`
- **那么** 记录警告日志，Vulkan 初始化失败，降级到下一级后端

### 需求:Vulkan 帧渲染
系统必须接受 RGB565 `uint16_t` 缓冲区，通过 Vulkan 纹理上传 + 全屏四边形绘制 + swapchain present 完成帧渲染。

#### 场景:正常帧渲染
- **当** Vulkan 后端已初始化且调用 `present_rgb565` 传入有效 RGB565 缓冲区
- **那么** 上传纹理 → 录制命令 → 提交队列 → present，返回 0

#### 场景:帧渲染失败
- **当** `vkQueueSubmit` 或 `vkQueuePresentKHR` 返回错误
- **那么** 记录错误日志，返回 -1

### 需求:渲染调度层
系统必须提供统一的渲染调度层（`mrp_renderer.h`），对 `napi_init.cpp` 暴露渲染接口和模式控制接口，内部管理 Vulkan/GLES/CPU 降级。

#### 场景:调度层转发调用
- **当** `napi_init.cpp` 调用调度层的 init/present/shutdown/is_ready
- **那么** 调度层转发到当前活跃后端，调用方不感知后端类型

### 需求:OHOS 平台 Vulkan Surface 创建
系统必须使用 `VK_OHOS_surface` 扩展从 `OHNativeWindow` 创建 `VkSurfaceKHR`。

#### 场景:从 OHNativeWindow 创建 Surface
- **当** Vulkan 后端初始化且持有有效的 `OHNativeWindow`
- **那么** 通过 `vkCreateSurfaceOHOS` 创建 VkSurfaceKHR

### 需求:Vulkan 资源释放
系统必须在关闭时释放所有 Vulkan 资源，禁止泄漏。

#### 场景:正常关闭
- **当** 调用关闭函数
- **那么** `vkDeviceWaitIdle` → 按依赖逆序销毁所有 Vulkan 对象

### 需求:渲染模式 NAPI 接口
系统必须通过 NAPI 导出 `setRendererMode`、`getRendererMode`、`getActiveRendererMode` 三个接口供 ArkTS 调用。

#### 场景:ArkTS 调用 setRendererMode
- **当** ArkTS 调用 `setRendererMode(1)` (VULKAN)
- **那么** Native 侧保存首选模式为 VULKAN，返回成功

#### 场景:ArkTS 调用 getActiveRendererMode
- **当** ArkTS 调用 `getActiveRendererMode()`
- **那么** 返回当前实际活跃后端的枚举值（0=未初始化, 1=Vulkan, 2=GLES, 3=CPU）

### 需求:渲染模式 UI
系统必须在手机外壳右侧提供"渲染"侧键，点击弹出渲染模式选择面板。

#### 场景:打开渲染模式面板
- **当** 用户点击"渲染"侧键
- **那么** 弹出面板，显示 4 个选项（自动/Vulkan/GLES/CPU）和当前活跃后端

#### 场景:选择新模式
- **当** 用户在面板中选择一个模式
- **那么** 保存偏好，提示用户模式将在下次画面刷新时生效，关闭面板

## 修改需求

（无已有功能需求变更）

## 移除需求

（无移除）

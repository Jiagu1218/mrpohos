## 1. 渲染调度层

- [x] 1.1 创建 `mrp_renderer.h`，暴露统一接口：`mrp_renderer_init` / `mrp_renderer_shutdown` / `mrp_renderer_is_ready` / `mrp_renderer_present_rgb565`，以及 `mrp_renderer_set_mode` / `mrp_renderer_get_mode` / `mrp_renderer_get_active_mode`
- [x] 1.2 创建 `mrp_renderer.cpp`，实现降级调度：根据用户首选模式（默认 AUTO=从 Vulkan 开始）依次尝试 init，首次成功即锁定后端，记录日志
- [x] 1.3 修改 `napi_init.cpp`，`#include "mrp_renderer.h"` 替换 `mrp_gles_renderer.h`，所有 `mrp_gles_renderer_*` 替换为 `mrp_renderer_*`
- [x] 1.4 在 `napi_init.cpp` 新增 3 个 NAPI 导出：`setRendererMode(mode)` / `getRendererMode()` / `getActiveRendererMode()`

## 2. Vulkan 初始化管线

- [x] 2.1 创建 `mrp_vulkan_renderer.h` / `mrp_vulkan_renderer.cpp`，实现 `mrp_vulkan_init` / `mrp_vulkan_shutdown` / `mrp_vulkan_present_rgb565` / `mrp_vulkan_is_ready` 四函数
- [x] 2.2 实现 `vkCreateInstance`（启用 `VK_KHR_SURFACE_EXTENSION_NAME` + `VK_OHOS_SURFACE_EXTENSION_NAME`）
- [x] 2.3 枚举 PhysicalDevice，选择图形队列 queueFamilyIndex，创建 Device（启用 `VK_KHR_SWAPCHAIN_EXTENSION_NAME`）
- [x] 2.4 实现 `vkCreateSurfaceOHOS` 从 `OHNativeWindow` 创建 `VkSurfaceKHR`
- [x] 2.5 查询 Surface capabilities/formats/presentModes，创建 Swapchain（`VK_FORMAT_B8G8R8A8_UNORM`，双缓冲），获取 swapchain images 并创建 ImageView
- [x] 2.6 检查 `VK_FORMAT_R5G6B5_UNORM_PACK16` formatProperties，不支持则返回失败

## 3. Vulkan 渲染资源

- [x] 3.1 创建 RGB565 纹理 Image（LINEAR tiling）+ DeviceMemory + ImageView + Sampler
- [x] 3.2 创建 DescriptorSetLayout + DescriptorPool + DescriptorSet（combined image sampler）
- [x] 3.3 创建 RenderPass + 内嵌 SPIR-V shader（vertex 全屏四边形 + fragment 采样 Y 翻转）+ GraphicsPipeline
- [x] 3.4 为每个 swapchain image 创建 Framebuffer + CommandPool + CommandBuffer + Fence

## 4. Vulkan 帧渲染

- [x] 4.1 实现纹理上传：`vkMapMemory` → `memcpy` RGB565 → `vkUnmapMemory` → pipeline barrier（→ SHADER_READ_ONLY_OPTIMAL）
- [x] 4.2 实现帧循环：`vkAcquireNextImageKHR` → 录制命令（renderpass + bind pipeline + bind descriptor + draw quad）→ `vkQueueSubmit`（fence）→ `vkQueuePresentKHR` → fence 同步

## 5. CMake 集成

- [x] 5.1 `CMakeLists.txt` 始终编译 `mrp_renderer.cpp` + `mrp_gles_renderer.cpp` + `mrp_vulkan_renderer.cpp`，添加 `VK_USE_PLATFORM_OHOS=1`，链接 `libvulkan.so` + `libGLESv3.so`

## 6. 渲染模式 UI

- [x] 6.1 在 `Index.ets` 右侧侧键区域"关于"下方新增"渲染"侧键（复用 `MrpSideKey` 风格）
- [x] 6.2 新增 `@CustomDialog` 渲染模式选择面板：4 个 Radio 选项（自动/Vulkan/GLES/CPU），底部显示当前活跃后端（调用 `getActiveRendererMode()`）
- [x] 6.3 面板中选择模式后调用 `setRendererMode(mode)`，通过 Preferences 持久化选择
- [x] 6.4 `aboutToAppear` 中从 Preferences 读取保存的模式，调用 `setRendererMode` 恢复

## 7. 验证

- [ ] 7.1 编译通过
- [ ] 7.2 默认 AUTO 模式：真机/模拟器正常渲染，hilog 显示选中后端
- [ ] 7.3 手动选择 Vulkan/GLES/CPU：切换后渲染正常
- [ ] 7.4 选择不支持的模式时自动降级：UI 显示降级提示（如"当前: GLES (Vulkan 不可用)"）
- [ ] 7.5 模式选择重启后保持
- [ ] 7.6 Surface 销毁后重建（前后台切换）正常工作

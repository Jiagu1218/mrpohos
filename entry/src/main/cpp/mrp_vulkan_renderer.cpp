#include "mrp_vulkan_renderer.h"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_ohos.h>
#include <hilog/log.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x0001
#define LOG_TAG "vmrp_vulkan"

static OHNativeWindow *g_window = nullptr;
static int32_t g_width = 0;
static int32_t g_height = 0;
static int g_disabled = 0;
static int g_inited = 0;

static VkInstance g_instance = VK_NULL_HANDLE;
static VkPhysicalDevice g_physDev = VK_NULL_HANDLE;
static VkDevice g_device = VK_NULL_HANDLE;
static uint32_t g_queueFamily = 0;
static VkQueue g_queue = VK_NULL_HANDLE;
static VkSurfaceKHR g_surface = VK_NULL_HANDLE;
static VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;
static VkFormat g_swapchainFormat = VK_FORMAT_UNDEFINED;
static VkExtent2D g_swapchainExtent = {0, 0};

static uint32_t g_imageCount = 0;
static VkImage *g_swapchainImages = nullptr;
static VkImageView *g_swapchainViews = nullptr;
static VkFramebuffer *g_framebuffers = nullptr;
static VkCommandBuffer *g_cmdBuffers = nullptr;
static VkFence *g_fences = nullptr;
static VkFence g_uploadFence = VK_NULL_HANDLE;

static VkRenderPass g_renderPass = VK_NULL_HANDLE;
static VkPipelineLayout g_pipelineLayout = VK_NULL_HANDLE;
static VkPipeline g_pipeline = VK_NULL_HANDLE;
static VkDescriptorSetLayout g_descLayout = VK_NULL_HANDLE;
static VkDescriptorPool g_descPool = VK_NULL_HANDLE;
static VkDescriptorSet g_descSet = VK_NULL_HANDLE;
static VkSampler g_sampler = VK_NULL_HANDLE;
static VkCommandPool g_cmdPool = VK_NULL_HANDLE;

static VkImage g_texImage = VK_NULL_HANDLE;
static VkDeviceMemory g_texMem = VK_NULL_HANDLE;
static VkImageView g_texView = VK_NULL_HANDLE;
static VkDeviceSize g_texMemSize = 0;
static int32_t g_texW = 0;
static int32_t g_texH = 0;


static VkBuffer g_vertexBuf = VK_NULL_HANDLE;
static VkDeviceMemory g_vertexMem = VK_NULL_HANDLE;

static uint32_t g_frameIndex = 0;

static const uint32_t kVertSpirv[] = {
    0x07230203, 0x00010000, 0x00000000, 0x00000017,
    0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
    0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x0009000f, 0x00000000, 0x00000010, 0x6e69616d,
    0x00000000, 0x0000000a, 0x0000000b, 0x0000000c,
    0x0000000d, 0x00040005, 0x0000000a, 0x6f705f61,
    0x00000073, 0x00040005, 0x0000000b, 0x76755f61,
    0x00000000, 0x00040005, 0x0000000c, 0x76755f76,
    0x00000000, 0x00050005, 0x0000000d, 0x505f6c67,
    0x7469736f, 0x006e6f69, 0x00040047, 0x0000000a,
    0x0000001e, 0x00000000, 0x00040047, 0x0000000b,
    0x0000001e, 0x00000001, 0x00040047, 0x0000000c,
    0x0000001e, 0x00000000, 0x00040047, 0x0000000d,
    0x0000000b, 0x00000000, 0x00020013, 0x00000002,
    0x00030016, 0x00000003, 0x00000020, 0x00040017,
    0x00000004, 0x00000003, 0x00000002, 0x00040017,
    0x00000005, 0x00000003, 0x00000004, 0x00030021,
    0x00000006, 0x00000002, 0x00040020, 0x00000007,
    0x00000001, 0x00000004, 0x00040020, 0x00000008,
    0x00000003, 0x00000004, 0x00040020, 0x00000009,
    0x00000003, 0x00000005, 0x0004003b, 0x00000007,
    0x0000000a, 0x00000001, 0x0004003b, 0x00000007,
    0x0000000b, 0x00000001, 0x0004003b, 0x00000008,
    0x0000000c, 0x00000003, 0x0004003b, 0x00000009,
    0x0000000d, 0x00000003, 0x0004002b, 0x00000003,
    0x0000000e, 0x00000000, 0x0004002b, 0x00000003,
    0x0000000f, 0x3f800000, 0x00050036, 0x00000002,
    0x00000010, 0x00000000, 0x00000006, 0x000200f8,
    0x00000011, 0x0004003d, 0x00000004, 0x00000012,
    0x0000000a, 0x0004003d, 0x00000004, 0x00000013,
    0x0000000b, 0x0003003e, 0x0000000c, 0x00000013,
    0x00050051, 0x00000003, 0x00000014, 0x00000012,
    0x00000000, 0x00050051, 0x00000003, 0x00000015,
    0x00000012, 0x00000001, 0x00070050, 0x00000005,
    0x00000016, 0x00000014, 0x00000015, 0x0000000e,
    0x0000000f, 0x0003003e, 0x0000000d, 0x00000016,
    0x000100fd, 0x00010038,
};

static const uint32_t kFragSpirv[] = {
    0x07230203, 0x00010000, 0x00000000, 0x00000016,
    0x00000000, 0x00020011, 0x00000001, 0x0006000b,
    0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e,
    0x00000000, 0x0003000e, 0x00000000, 0x00000001,
    0x0007000f, 0x00000004, 0x00000011, 0x6e69616d,
    0x00000000, 0x0000000e, 0x00000010, 0x00030010,
    0x00000011, 0x00000007, 0x00040005, 0x0000000e,
    0x76755f76, 0x00000000, 0x00040005, 0x0000000f,
    0x65745f75, 0x00000078, 0x00050005, 0x00000010,
    0x67617266, 0x6f6c6f43, 0x00000072, 0x00040047,
    0x0000000e, 0x0000001e, 0x00000000, 0x00040047,
    0x00000010, 0x0000001e, 0x00000000, 0x00040047,
    0x0000000f, 0x00000021, 0x00000000, 0x00040047,
    0x0000000f, 0x00000022, 0x00000000, 0x00030047,
    0x00000015, 0x00000003, 0x00020013,
    0x00000002, 0x00030016, 0x00000003, 0x00000020,
    0x00040017, 0x00000004, 0x00000003, 0x00000002,
    0x00040017, 0x00000005, 0x00000003, 0x00000004,
    0x00030021, 0x00000007, 0x00000002, 0x00040020,
    0x00000008, 0x00000001, 0x00000004, 0x00040020,
    0x00000009, 0x00000003, 0x00000005, 0x00090019,
    0x0000000a, 0x00000003, 0x00000001, 0x00000000,
    0x00000000, 0x00000000, 0x00000001, 0x00000000,
    0x0002001a, 0x0000000b, 0x0003001b, 0x0000000c,
    0x0000000a, 0x00040020, 0x0000000d, 0x00000000,
    0x0000000c, 0x0004003b, 0x00000008, 0x0000000e,
    0x00000001, 0x0004003b, 0x0000000d, 0x0000000f,
    0x00000000, 0x0004003b, 0x00000009, 0x00000010,
    0x00000003, 0x00050036, 0x00000002, 0x00000011,
    0x00000000, 0x00000007, 0x000200f8, 0x00000012,
    0x0004003d, 0x00000004, 0x00000013, 0x0000000e,
    0x0004003d, 0x0000000c, 0x00000014, 0x0000000f,
    0x00050057, 0x00000005, 0x00000015, 0x00000014,
    0x00000013, 0x0003003e, 0x00000010, 0x00000015,
    0x000100fd, 0x00010038,
};

static uint32_t find_memory_type(uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(g_physDev, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return UINT32_MAX;
}

static void destroy_vulkan_resources(void) {
    if (g_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(g_device);
    }
    if (g_cmdPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(g_device, g_cmdPool, nullptr);
        g_cmdPool = VK_NULL_HANDLE;
    }
    for (uint32_t i = 0; i < g_imageCount; i++) {
        if (g_fences && g_fences[i] != VK_NULL_HANDLE) {
            vkDestroyFence(g_device, g_fences[i], nullptr);
        }
    }
    free(g_fences); g_fences = nullptr;
    if (g_uploadFence != VK_NULL_HANDLE) {
        vkDestroyFence(g_device, g_uploadFence, nullptr);
        g_uploadFence = VK_NULL_HANDLE;
    }
    free(g_cmdBuffers); g_cmdBuffers = nullptr;
    free(g_framebuffers); g_framebuffers = nullptr;
    for (uint32_t i = 0; i < g_imageCount; i++) {
        if (g_swapchainViews && g_swapchainViews[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(g_device, g_swapchainViews[i], nullptr);
        }
    }
    free(g_swapchainViews); g_swapchainViews = nullptr;
    free(g_swapchainImages); g_swapchainImages = nullptr;
    if (g_pipeline != VK_NULL_HANDLE) { vkDestroyPipeline(g_device, g_pipeline, nullptr); g_pipeline = VK_NULL_HANDLE; }
    if (g_pipelineLayout != VK_NULL_HANDLE) { vkDestroyPipelineLayout(g_device, g_pipelineLayout, nullptr); g_pipelineLayout = VK_NULL_HANDLE; }
    if (g_renderPass != VK_NULL_HANDLE) { vkDestroyRenderPass(g_device, g_renderPass, nullptr); g_renderPass = VK_NULL_HANDLE; }
    if (g_descPool != VK_NULL_HANDLE) { vkDestroyDescriptorPool(g_device, g_descPool, nullptr); g_descPool = VK_NULL_HANDLE; }
    if (g_descLayout != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(g_device, g_descLayout, nullptr); g_descLayout = VK_NULL_HANDLE; }
    if (g_sampler != VK_NULL_HANDLE) { vkDestroySampler(g_device, g_sampler, nullptr); g_sampler = VK_NULL_HANDLE; }
    if (g_texView != VK_NULL_HANDLE) { vkDestroyImageView(g_device, g_texView, nullptr); g_texView = VK_NULL_HANDLE; }
    if (g_texImage != VK_NULL_HANDLE) { vkDestroyImage(g_device, g_texImage, nullptr); g_texImage = VK_NULL_HANDLE; }
    if (g_texMem != VK_NULL_HANDLE) { vkFreeMemory(g_device, g_texMem, nullptr); g_texMem = VK_NULL_HANDLE; }
    if (g_vertexBuf != VK_NULL_HANDLE) { vkDestroyBuffer(g_device, g_vertexBuf, nullptr); g_vertexBuf = VK_NULL_HANDLE; }
    if (g_vertexMem != VK_NULL_HANDLE) { vkFreeMemory(g_device, g_vertexMem, nullptr); g_vertexMem = VK_NULL_HANDLE; }
    if (g_swapchain != VK_NULL_HANDLE) { vkDestroySwapchainKHR(g_device, g_swapchain, nullptr); g_swapchain = VK_NULL_HANDLE; }
    if (g_device != VK_NULL_HANDLE) { vkDestroyDevice(g_device, nullptr); g_device = VK_NULL_HANDLE; }
    if (g_surface != VK_NULL_HANDLE) { vkDestroySurfaceKHR(g_instance, g_surface, nullptr); g_surface = VK_NULL_HANDLE; }
    if (g_instance != VK_NULL_HANDLE) { vkDestroyInstance(g_instance, nullptr); g_instance = VK_NULL_HANDLE; }
    g_physDev = VK_NULL_HANDLE;
    g_queue = VK_NULL_HANDLE;
    g_imageCount = 0;
    g_texW = 0;
    g_texH = 0;
    g_inited = 0;
}

static int check_rgb565_format_support(void) {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(g_physDev, VK_FORMAT_R5G6B5_UNORM_PACK16, &props);
    if (props.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
        return 1;
    }
    OH_LOG_WARN(LOG_APP, "vulkan: VK_FORMAT_R5G6B5_UNORM_PACK16 not supported");
    return 0;
}

static int create_instance(void) {
    uint32_t availVer = 0;
    if (vkEnumerateInstanceVersion(&availVer) != VK_SUCCESS) {
        availVer = VK_API_VERSION_1_0;
    }
    uint32_t major = VK_VERSION_MAJOR(availVer);
    uint32_t minor = VK_VERSION_MINOR(availVer);
    uint32_t useVer = availVer;
    if (major < 1 || (major == 1 && minor < 1)) {
        useVer = VK_API_VERSION_1_0;
    }
    OH_LOG_INFO(LOG_APP, "vulkan: driver supports %{public}u.%{public}u, using %{public}u.%{public}u",
        major, minor, VK_VERSION_MAJOR(useVer), VK_VERSION_MINOR(useVer));

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "vmrp";
    appInfo.pEngineName = "vmrp";
    appInfo.apiVersion = useVer;

    const char *extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_OHOS_SURFACE_EXTENSION_NAME,
    };

    VkInstanceCreateInfo ci = {};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &appInfo;
    ci.enabledExtensionCount = 2;
    ci.ppEnabledExtensionNames = extensions;

    VkResult res = vkCreateInstance(&ci, nullptr, &g_instance);
    if (res != VK_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "vulkan: vkCreateInstance failed %{public}d", (int)res);
        return 0;
    }
    return 1;
}

static int pick_physical_device(void) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(g_instance, &count, nullptr);
    if (count == 0) {
        OH_LOG_ERROR(LOG_APP, "vulkan: no physical devices");
        return 0;
    }
    VkPhysicalDevice *devs = (VkPhysicalDevice *)malloc(sizeof(VkPhysicalDevice) * count);
    vkEnumeratePhysicalDevices(g_instance, &count, devs);

    g_physDev = VK_NULL_HANDLE;
    g_queueFamily = UINT32_MAX;
    for (uint32_t d = 0; d < count; d++) {
        uint32_t qfCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devs[d], &qfCount, nullptr);
        VkQueueFamilyProperties *qfProps = (VkQueueFamilyProperties *)malloc(sizeof(VkQueueFamilyProperties) * qfCount);
        vkGetPhysicalDeviceQueueFamilyProperties(devs[d], &qfCount, qfProps);
        for (uint32_t i = 0; i < qfCount; i++) {
            if (qfProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                VkBool32 present = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(devs[d], i, g_surface, &present);
                if (present) {
                    g_physDev = devs[d];
                    g_queueFamily = i;
                    break;
                }
            }
        }
        free(qfProps);
        if (g_physDev != VK_NULL_HANDLE) break;
    }
    free(devs);
    if (g_physDev == VK_NULL_HANDLE) {
        OH_LOG_ERROR(LOG_APP, "vulkan: no suitable physical device");
        return 0;
    }
    return 1;
}

static int create_device(void) {
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci = {};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = g_queueFamily;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;

    const char *devExts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo dci = {};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = devExts;

    VkResult res = vkCreateDevice(g_physDev, &dci, nullptr, &g_device);
    if (res != VK_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "vulkan: vkCreateDevice failed %{public}d", (int)res);
        return 0;
    }
    vkGetDeviceQueue(g_device, g_queueFamily, 0, &g_queue);
    return 1;
}

static int create_surface(void) {
    VkSurfaceCreateInfoOHOS sci = {};
    sci.sType = VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS;
    sci.window = g_window;
    VkResult res = vkCreateSurfaceOHOS(g_instance, &sci, nullptr, &g_surface);
    if (res != VK_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "vulkan: vkCreateSurfaceOHOS failed %{public}d", (int)res);
        return 0;
    }
    return 1;
}

static int create_swapchain(void) {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physDev, g_surface, &caps);

    uint32_t fmtCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physDev, g_surface, &fmtCount, nullptr);
    VkSurfaceFormatKHR *fmts = (VkSurfaceFormatKHR *)malloc(sizeof(VkSurfaceFormatKHR) * (fmtCount > 0 ? fmtCount : 1));
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_physDev, g_surface, &fmtCount, fmts);

    VkSurfaceFormatKHR chosen = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    int found = 0;
    for (uint32_t i = 0; i < fmtCount; i++) {
        if (fmts[i].format == VK_FORMAT_B8G8R8A8_UNORM && fmts[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen = fmts[i];
            found = 1;
            break;
        }
    }
    if (!found && fmtCount > 0) chosen = fmts[0];
    free(fmts);

    g_swapchainFormat = chosen.format;

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = (uint32_t)g_width;
        extent.height = (uint32_t)g_height;
        if (extent.width < caps.minImageExtent.width) extent.width = caps.minImageExtent.width;
        if (extent.height < caps.minImageExtent.height) extent.height = caps.minImageExtent.height;
        if (extent.width > caps.maxImageExtent.width) extent.width = caps.maxImageExtent.width;
        if (extent.height > caps.maxImageExtent.height) extent.height = caps.maxImageExtent.height;
    }
    g_swapchainExtent = extent;

    uint32_t imgCount = caps.minImageCount;
    if (caps.minImageCount < 2) imgCount = 2;
    if (caps.maxImageCount > 0 && imgCount > caps.maxImageCount) imgCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR sci = {};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = g_surface;
    sci.minImageCount = imgCount;
    sci.imageFormat = chosen.format;
    sci.imageColorSpace = chosen.colorSpace;
    sci.imageExtent = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;

    VkResult res = vkCreateSwapchainKHR(g_device, &sci, nullptr, &g_swapchain);
    if (res != VK_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "vulkan: vkCreateSwapchainKHR failed %{public}d", (int)res);
        return 0;
    }

    vkGetSwapchainImagesKHR(g_device, g_swapchain, &g_imageCount, nullptr);
    g_swapchainImages = (VkImage *)malloc(sizeof(VkImage) * g_imageCount);
    g_swapchainViews = (VkImageView *)malloc(sizeof(VkImageView) * g_imageCount);
    g_framebuffers = (VkFramebuffer *)malloc(sizeof(VkFramebuffer) * g_imageCount);
    g_cmdBuffers = (VkCommandBuffer *)malloc(sizeof(VkCommandBuffer) * g_imageCount);
    g_fences = (VkFence *)calloc(g_imageCount, sizeof(VkFence));
    memset(g_swapchainViews, 0, sizeof(VkImageView) * g_imageCount);
    memset(g_framebuffers, 0, sizeof(VkFramebuffer) * g_imageCount);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &g_imageCount, g_swapchainImages);

    return 1;
}

static int create_image_views_and_framebuffers(void) {
    for (uint32_t i = 0; i < g_imageCount; i++) {
        VkImageViewCreateInfo ivci = {};
        ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivci.image = g_swapchainImages[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = g_swapchainFormat;
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.layerCount = 1;
        VkResult res = vkCreateImageView(g_device, &ivci, nullptr, &g_swapchainViews[i]);
        if (res != VK_SUCCESS) {
            OH_LOG_ERROR(LOG_APP, "vulkan: vkCreateImageView failed %{public}d", (int)res);
            return 0;
        }
    }
    return 1;
}

static int create_render_pass(void) {
    VkAttachmentDescription colorAtt = {};
    colorAtt.format = g_swapchainFormat;
    colorAtt.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAtt.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef = {};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkRenderPassCreateInfo rpci = {};
    rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpci.attachmentCount = 1;
    rpci.pAttachments = &colorAtt;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;

    VkResult res = vkCreateRenderPass(g_device, &rpci, nullptr, &g_renderPass);
    if (res != VK_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "vulkan: vkCreateRenderPass failed %{public}d", (int)res);
        return 0;
    }

    for (uint32_t i = 0; i < g_imageCount; i++) {
        VkFramebufferCreateInfo fbci = {};
        fbci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbci.renderPass = g_renderPass;
        fbci.attachmentCount = 1;
        fbci.pAttachments = &g_swapchainViews[i];
        fbci.width = g_swapchainExtent.width;
        fbci.height = g_swapchainExtent.height;
        fbci.layers = 1;
        VkResult r = vkCreateFramebuffer(g_device, &fbci, nullptr, &g_framebuffers[i]);
        if (r != VK_SUCCESS) {
            OH_LOG_ERROR(LOG_APP, "vulkan: vkCreateFramebuffer failed %{public}d", (int)r);
            return 0;
        }
    }
    return 1;
}

static int create_texture(int32_t w, int32_t h) {
    g_texW = w;
    g_texH = h;

    VkFormat texFmt = VK_FORMAT_R5G6B5_UNORM_PACK16;

    VkImageCreateInfo ici = {};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = texFmt;
    ici.extent.width = (uint32_t)w;
    ici.extent.height = (uint32_t)h;
    ici.extent.depth = 1;
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_LINEAR;
    ici.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;

    VkResult res = vkCreateImage(g_device, &ici, nullptr, &g_texImage);
    if (res != VK_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "vulkan: vkCreateImage (texture) failed %{public}d", (int)res);
        return 0;
    }

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(g_device, g_texImage, &memReq);
    g_texMemSize = memReq.size;

    VkMemoryAllocateInfo mai = {};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = memReq.size;
    mai.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (mai.memoryTypeIndex == UINT32_MAX) {
        OH_LOG_ERROR(LOG_APP, "vulkan: no host-visible memory for texture");
        return 0;
    }

    res = vkAllocateMemory(g_device, &mai, nullptr, &g_texMem);
    if (res != VK_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "vulkan: vkAllocateMemory (texture) failed %{public}d", (int)res);
        return 0;
    }
    vkBindImageMemory(g_device, g_texImage, g_texMem, 0);

    VkImageViewCreateInfo ivci = {};
    ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivci.image = g_texImage;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = VK_FORMAT_R5G6B5_UNORM_PACK16;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.layerCount = 1;
    res = vkCreateImageView(g_device, &ivci, nullptr, &g_texView);
    if (res != VK_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "vulkan: vkCreateImageView (texture) failed %{public}d", (int)res);
        return 0;
    }

    VkSamplerCreateInfo sci = {};
    sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter = VK_FILTER_LINEAR;
    sci.minFilter = VK_FILTER_LINEAR;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sci.maxLod = VK_LOD_CLAMP_NONE;
    res = vkCreateSampler(g_device, &sci, nullptr, &g_sampler);
    if (res != VK_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "vulkan: vkCreateSampler failed %{public}d", (int)res);
        return 0;
    }

    return 1;
}

static int create_descriptor(void) {
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo dlci = {};
    dlci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dlci.bindingCount = 1;
    dlci.pBindings = &binding;
    VkResult res = vkCreateDescriptorSetLayout(g_device, &dlci, nullptr, &g_descLayout);
    if (res != VK_SUCCESS) return 0;

    VkDescriptorPoolSize poolSize = {};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo dpci = {};
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.maxSets = 1;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &poolSize;
    res = vkCreateDescriptorPool(g_device, &dpci, nullptr, &g_descPool);
    if (res != VK_SUCCESS) return 0;

    VkDescriptorSetAllocateInfo dsai = {};
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool = g_descPool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &g_descLayout;
    res = vkAllocateDescriptorSets(g_device, &dsai, &g_descSet);
    if (res != VK_SUCCESS) return 0;

    VkDescriptorImageInfo imgInfo = {};
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imgInfo.imageView = g_texView;
    imgInfo.sampler = g_sampler;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = g_descSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &imgInfo;
    vkUpdateDescriptorSets(g_device, 1, &write, 0, nullptr);

    return 1;
}

static int create_pipeline(void) {
    VkPipelineLayoutCreateInfo plci = {};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &g_descLayout;
    VkResult res = vkCreatePipelineLayout(g_device, &plci, nullptr, &g_pipelineLayout);
    if (res != VK_SUCCESS) return 0;

    VkShaderModuleCreateInfo vmci = {};
    vmci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vmci.codeSize = sizeof(kVertSpirv);
    vmci.pCode = kVertSpirv;
    VkShaderModule vertMod;
    res = vkCreateShaderModule(g_device, &vmci, nullptr, &vertMod);
    if (res != VK_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "vulkan: vertex shader create failed %{public}d", (int)res);
        return 0;
    }

    VkShaderModuleCreateInfo fmci = {};
    fmci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fmci.codeSize = sizeof(kFragSpirv);
    fmci.pCode = kFragSpirv;
    VkShaderModule fragMod;
    res = vkCreateShaderModule(g_device, &fmci, nullptr, &fragMod);
    if (res != VK_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "vulkan: fragment shader create failed %{public}d", (int)res);
        vkDestroyShaderModule(g_device, vertMod, nullptr);
        return 0;
    }

    VkPipelineShaderStageCreateInfo stages[2] = {};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertMod;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragMod;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding = {};
    binding.stride = 4 * sizeof(float);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrs[2] = {};
    attrs[0].binding = 0;
    attrs[0].location = 0;
    attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[0].offset = 0;
    attrs[1].binding = 0;
    attrs[1].location = 1;
    attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[1].offset = 2 * sizeof(float);

    VkPipelineVertexInputStateCreateInfo visci = {};
    visci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    visci.vertexBindingDescriptionCount = 1;
    visci.pVertexBindingDescriptions = &binding;
    visci.vertexAttributeDescriptionCount = 2;
    visci.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo iasci = {};
    iasci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iasci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    iasci.primitiveRestartEnable = VK_FALSE;

    VkViewport vp = {0.0f, 0.0f, (float)g_swapchainExtent.width, (float)g_swapchainExtent.height, 0.0f, 1.0f};
    VkRect2D sc = {{0, 0}, g_swapchainExtent};

    VkPipelineViewportStateCreateInfo vpci = {};
    vpci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpci.viewportCount = 1;
    vpci.pViewports = &vp;
    vpci.scissorCount = 1;
    vpci.pScissors = &sc;

    VkPipelineRasterizationStateCreateInfo rsci = {};
    rsci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rsci.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo msci = {};
    msci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cba = {};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cbsci = {};
    cbsci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cbsci.attachmentCount = 1;
    cbsci.pAttachments = &cba;

    VkGraphicsPipelineCreateInfo gpci = {};
    gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gpci.stageCount = 2;
    gpci.pStages = stages;
    gpci.pVertexInputState = &visci;
    gpci.pInputAssemblyState = &iasci;
    gpci.pViewportState = &vpci;
    gpci.pRasterizationState = &rsci;
    gpci.pMultisampleState = &msci;
    gpci.pColorBlendState = &cbsci;
    gpci.renderPass = g_renderPass;
    gpci.subpass = 0;
    gpci.layout = g_pipelineLayout;

    res = vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &gpci, nullptr, &g_pipeline);
    vkDestroyShaderModule(g_device, vertMod, nullptr);
    vkDestroyShaderModule(g_device, fragMod, nullptr);
    if (res != VK_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "vulkan: vkCreateGraphicsPipelines failed %{public}d", (int)res);
        return 0;
    }
    return 1;
}

static int create_vertex_buffer(void) {
    float verts[] = {
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };

    VkBufferCreateInfo bci = {};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = sizeof(verts);
    bci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult res = vkCreateBuffer(g_device, &bci, nullptr, &g_vertexBuf);
    if (res != VK_SUCCESS) return 0;

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(g_device, g_vertexBuf, &memReq);

    VkMemoryAllocateInfo mai = {};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = memReq.size;
    mai.memoryTypeIndex = find_memory_type(memReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (mai.memoryTypeIndex == UINT32_MAX) return 0;

    res = vkAllocateMemory(g_device, &mai, nullptr, &g_vertexMem);
    if (res != VK_SUCCESS) return 0;
    vkBindBufferMemory(g_device, g_vertexBuf, g_vertexMem, 0);

    void *data = nullptr;
    vkMapMemory(g_device, g_vertexMem, 0, sizeof(verts), 0, &data);
    memcpy(data, verts, sizeof(verts));
    vkUnmapMemory(g_device, g_vertexMem);

    return 1;
}

static int create_command_resources(void) {
    VkCommandPoolCreateInfo cpci = {};
    cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpci.queueFamilyIndex = g_queueFamily;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkResult res = vkCreateCommandPool(g_device, &cpci, nullptr, &g_cmdPool);
    if (res != VK_SUCCESS) return 0;

    VkCommandBufferAllocateInfo cbai = {};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = g_cmdPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = g_imageCount;
    res = vkAllocateCommandBuffers(g_device, &cbai, g_cmdBuffers);
    if (res != VK_SUCCESS) return 0;

    for (uint32_t i = 0; i < g_imageCount; i++) {
        VkFenceCreateInfo fci = {};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        res = vkCreateFence(g_device, &fci, nullptr, &g_fences[i]);
        if (res != VK_SUCCESS) return 0;
    }

    VkFenceCreateInfo ufci = {};
    ufci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(g_device, &ufci, nullptr, &g_uploadFence);

    return 1;
}

static int init_vulkan_pipeline(int32_t w, int32_t h) {
    OH_LOG_INFO(LOG_APP, "vulkan: init_vulkan_pipeline step 1/11 create_instance");
    if (!create_instance()) return 0;
    OH_LOG_INFO(LOG_APP, "vulkan: init_vulkan_pipeline step 2/11 create_surface");
    if (!create_surface()) { destroy_vulkan_resources(); return 0; }
    OH_LOG_INFO(LOG_APP, "vulkan: init_vulkan_pipeline step 3/11 pick_physical_device");
    if (!pick_physical_device()) { destroy_vulkan_resources(); return 0; }
    OH_LOG_INFO(LOG_APP, "vulkan: init_vulkan_pipeline step 4/11 check_rgb565_format_support");
    if (!check_rgb565_format_support()) { destroy_vulkan_resources(); return 0; }
    OH_LOG_INFO(LOG_APP, "vulkan: init_vulkan_pipeline step 5/11 create_device");
    if (!create_device()) { destroy_vulkan_resources(); return 0; }
    OH_LOG_INFO(LOG_APP, "vulkan: init_vulkan_pipeline step 6/11 create_swapchain");
    if (!create_swapchain()) { destroy_vulkan_resources(); return 0; }
    OH_LOG_INFO(LOG_APP, "vulkan: init_vulkan_pipeline step 7/11 create_image_views_and_framebuffers");
    if (!create_image_views_and_framebuffers()) { destroy_vulkan_resources(); return 0; }
    OH_LOG_INFO(LOG_APP, "vulkan: init_vulkan_pipeline step 8/11 create_render_pass");
    if (!create_render_pass()) { destroy_vulkan_resources(); return 0; }
    OH_LOG_INFO(LOG_APP, "vulkan: init_vulkan_pipeline step 9/11 create_texture");
    if (!create_texture(w, h)) { destroy_vulkan_resources(); return 0; }
    OH_LOG_INFO(LOG_APP, "vulkan: init_vulkan_pipeline step 10/11 create_descriptor");
    if (!create_descriptor()) { destroy_vulkan_resources(); return 0; }
    OH_LOG_INFO(LOG_APP, "vulkan: init_vulkan_pipeline step 11a/11 create_pipeline");
    if (!create_pipeline()) { destroy_vulkan_resources(); return 0; }
    OH_LOG_INFO(LOG_APP, "vulkan: init_vulkan_pipeline step 11b/11 create_vertex_buffer");
    if (!create_vertex_buffer()) { destroy_vulkan_resources(); return 0; }
    OH_LOG_INFO(LOG_APP, "vulkan: init_vulkan_pipeline step 11c/11 create_command_resources");
    if (!create_command_resources()) { destroy_vulkan_resources(); return 0; }

    OH_LOG_INFO(LOG_APP, "vulkan: init OK %{public}dx%{public}d swapchain %{public}ux%{public}u %{public}u images texFmt=%{public}s",
        (int)w, (int)h, (unsigned)g_swapchainExtent.width, (unsigned)g_swapchainExtent.height, (unsigned)g_imageCount,
        "RGB565");
    return 1;
}

int mrp_vulkan_init(OHNativeWindow *window, int32_t width, int32_t height) {
    if (!window || width <= 0 || height <= 0) return 0;
    if (g_inited) mrp_vulkan_shutdown();
    g_window = window;
    g_width = width;
    g_height = height;
    g_disabled = 0;
    if (!init_vulkan_pipeline(width, height)) {
        OH_LOG_WARN(LOG_APP, "vulkan: init failed, disabled for this surface");
        g_disabled = 1;
        g_inited = 0;
        return 0;
    }
    g_inited = 1;
    return 1;
}

void mrp_vulkan_shutdown(void) {
    if (g_device != VK_NULL_HANDLE || g_instance != VK_NULL_HANDLE) {
        destroy_vulkan_resources();
        OH_LOG_INFO(LOG_APP, "vulkan: shutdown done");
    }
    g_window = nullptr;
    g_width = 0;
    g_height = 0;
    g_disabled = 0;
    g_inited = 0;
}

int mrp_vulkan_is_ready(void) {
    return (g_inited && !g_disabled) ? 1 : 0;
}

int mrp_vulkan_present_rgb565(const uint16_t *rgb565, int32_t width, int32_t height) {
    if (!rgb565 || !g_inited || g_disabled) return -1;
    if (width != g_texW || height != g_texH) return -1;

    uint32_t imgIdx = 0;
    VkResult res = vkAcquireNextImageKHR(g_device, g_swapchain, UINT64_MAX, VK_NULL_HANDLE, VK_NULL_HANDLE, &imgIdx);
    if (res != VK_SUCCESS) {
        static int s_acquireErr = 0;
        if (s_acquireErr++ < 3) {
            OH_LOG_WARN(LOG_APP, "vulkan: vkAcquireNextImageKHR failed %{public}d", (int)res);
        }
        return -1;
    }

    vkWaitForFences(g_device, 1, &g_fences[imgIdx], VK_TRUE, UINT64_MAX);
    vkResetFences(g_device, 1, &g_fences[imgIdx]);

    void *texData = nullptr;
    vkMapMemory(g_device, g_texMem, 0, g_texMemSize, 0, &texData);
    VkImageSubresource subres = {};
    subres.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subres.mipLevel = 0;
    subres.arrayLayer = 0;
    VkSubresourceLayout layout;
    vkGetImageSubresourceLayout(g_device, g_texImage, &subres, &layout);

    if (layout.rowPitch == (VkDeviceSize)(width * 2)) {
        memcpy(texData, rgb565, (size_t)width * (size_t)height * 2);
    } else {
        const uint8_t *src = (const uint8_t *)rgb565;
        uint8_t *dst = (uint8_t *)texData;
        for (int32_t y = 0; y < height; y++) {
            memcpy(dst + y * layout.rowPitch, src + y * width * 2, (size_t)width * 2);
        }
    }
    vkUnmapMemory(g_device, g_texMem);

    VkCommandBufferBeginInfo cbbi = {};
    cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkResetCommandBuffer(g_cmdBuffers[imgIdx], 0);
    vkBeginCommandBuffer(g_cmdBuffers[imgIdx], &cbbi);

    VkImageMemoryBarrier texBarrier = {};
    texBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    texBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
    texBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    texBarrier.oldLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    texBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    texBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    texBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    texBarrier.image = g_texImage;
    texBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    texBarrier.subresourceRange.levelCount = 1;
    texBarrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(g_cmdBuffers[imgIdx],
        VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &texBarrier);

    VkClearValue clearVal = {};
    VkRenderPassBeginInfo rpbi = {};
    rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpbi.renderPass = g_renderPass;
    rpbi.framebuffer = g_framebuffers[imgIdx];
    rpbi.renderArea.extent = g_swapchainExtent;
    rpbi.clearValueCount = 1;
    rpbi.pClearValues = &clearVal;
    vkCmdBeginRenderPass(g_cmdBuffers[imgIdx], &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(g_cmdBuffers[imgIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipeline);
    vkCmdBindDescriptorSets(g_cmdBuffers[imgIdx], VK_PIPELINE_BIND_POINT_GRAPHICS,
        g_pipelineLayout, 0, 1, &g_descSet, 0, nullptr);

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(g_cmdBuffers[imgIdx], 0, 1, &g_vertexBuf, offsets);
    vkCmdDraw(g_cmdBuffers[imgIdx], 4, 1, 0, 0);

    vkCmdEndRenderPass(g_cmdBuffers[imgIdx]);
    vkEndCommandBuffer(g_cmdBuffers[imgIdx]);

    VkSubmitInfo si = {};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &g_cmdBuffers[imgIdx];
    res = vkQueueSubmit(g_queue, 1, &si, g_fences[imgIdx]);
    if (res != VK_SUCCESS) {
        static int s_submitErr = 0;
        if (s_submitErr++ < 3) {
            OH_LOG_WARN(LOG_APP, "vulkan: vkQueueSubmit failed %{public}d", (int)res);
        }
        return -1;
    }

    VkPresentInfoKHR pi = {};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.swapchainCount = 1;
    pi.pSwapchains = &g_swapchain;
    pi.pImageIndices = &imgIdx;
    res = vkQueuePresentKHR(g_queue, &pi);
    if (res != VK_SUCCESS) {
        static int s_presentErr = 0;
        if (s_presentErr++ < 3) {
            OH_LOG_WARN(LOG_APP, "vulkan: vkQueuePresentKHR failed %{public}d", (int)res);
        }
        return -1;
    }

    return 0;
}

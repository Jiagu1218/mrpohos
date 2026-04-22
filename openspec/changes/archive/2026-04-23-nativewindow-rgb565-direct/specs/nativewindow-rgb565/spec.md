## 新增需求

### 需求:CPU 渲染路径应支持 RGB565 格式直连 NativeWindow
当 NativeWindow 支持 RGB565 像素格式时，CPU 渲染路径应当以 RGB565 格式直接写入缓冲区，无需进行 RGB565→RGBA 转换。

#### 场景:Surface 创建时尝试设置 RGB565
- **当** 应用 Surface 创建（OnSurfaceCreated）
- **那么** 系统尝试设置 NativeWindow 格式为 RGB565；如果失败则回退 RGBA

#### 场景:RGB565 格式回退
- **当** 设置 RGB565 格式失败（设备不支持）
- **那么** 系统自动回退到 RGBA8888 格式继续渲染

#### 场景:直接写入 RGB565 数据
- **当** 使用 CPU 路径渲染到 NativeWindow 且格式为 RGB565
- **那么** 系统直接将 RGB565 数据写入缓冲区，每像素 2 字节，跳过 RGBA 转换

#### 场景:色彩精度损失可接受
- **当** RGB565 格式渲染
- **那么** 红色 5-bit、蓝色 5-bit、绿色 6-bit 精度满足复古像素游戏需求
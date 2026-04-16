# vmrp-rendering 规范

## 目的
待定 - 由归档变更 port-vmrp 创建。归档后请更新目的。
## 需求
### 需求:画面渲染回调
系统必须在 ARM 代码调用 mr_drawBitmap 时，将 RGB565 像素数据通过 NAPI 回调传递给 ArkTS 层。

#### 场景:正常渲染
- **当** ARM 代码调用 mr_drawBitmap(buf, x, y, w, h)
- **那么** 系统必须从模拟内存读取 RGB565 像素数据，通过 NAPI 回调通知 ArkTS 层，传递像素缓冲区、坐标和尺寸信息

#### 场景:全屏刷新
- **当** ARM 代码完成一帧完整画面绘制（通常 240x320）
- **那么** ArkTS 层必须接收到完整的像素数据并显示到 XComponent

### 需求:RGB565 格式转换
系统必须将 RGB565 格式像素数据转换为 HarmonyOS 可渲染的格式。

#### 场景:格式转换
- **当** 接收到 RGB565 像素数据
- **那么** 必须正确转换为 RGBA8888 格式供 XComponent Canvas 使用，颜色必须准确还原

### 需求:XComponent 集成
ArkTS 层必须使用 XComponent 作为渲染目标。

#### 场景:XComponent 初始化
- **当** 页面加载完成
- **那么** XComponent 必须完成初始化并准备好接收像素数据

#### 场景:画面更新
- **当** 收到渲染回调的像素数据
- **那么** XComponent 必须立即更新显示画面


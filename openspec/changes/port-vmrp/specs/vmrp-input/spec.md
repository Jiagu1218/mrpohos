## 新增需求

### 需求:触屏事件传递
系统必须将 HarmonyOS 的触屏事件转换为 MRP 模拟器的鼠标事件并传递给 C 层。

#### 场景:触摸按下
- **当** 用户在 XComponent 区域按下屏幕
- **那么** ArkTS 层必须调用 NAPI event(MR_MOUSE_DOWN, x, y)，坐标必须按 240x320 屏幕比例缩放

#### 场景:触摸移动
- **当** 用户在屏幕上移动手指
- **那么** ArkTS 层必须调用 NAPI event(MR_MOUSE_MOVE, x, y)

#### 场景:触摸抬起
- **当** 用户从屏幕上抬起手指
- **那么** ArkTS 层必须调用 NAPI event(MR_MOUSE_UP, x, y)

### 需求:按键事件传递
系统必须将 HarmonyOS 的按键事件转换为 MRP 模拟器的按键事件并传递给 C 层。

#### 场景:按键按下
- **当** 用户按下物理或虚拟按键
- **那么** ArkTS 层必须调用 NAPI event(MR_KEY_PRESS, keyCode, 0)，keyCode 必须映射到 MR_KEY_* 枚举值

#### 场景:按键释放
- **当** 用户释放按键
- **那么** ArkTS 层必须调用 NAPI event(MR_KEY_RELEASE, keyCode, 0)

### 需求:坐标缩放
系统必须将 HarmonyOS 屏幕坐标正确映射到 240x320 的 MRP 屏幕坐标。

#### 场景:坐标映射
- **当** 用户触摸 XComponent 上的任意位置
- **那么** 触摸坐标必须按 XComponent 实际尺寸与 240x320 的比例进行缩放映射

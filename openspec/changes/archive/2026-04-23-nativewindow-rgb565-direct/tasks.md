## 1. 初始化与格式设置

- [x] 1.1 在 onSurfaceCreated/onSurfaceChanged 中尝试设置 RGB565 格式
- [x] 1.2 添加 GET_FORMAT 探测回退逻辑（格式不支持时回退 RGBA）

## 2. RGB565 flush 实现

- [x] 2.1 修改 flushCpuNativeWindowLocked 根据当前格式选择写入路径
- [x] 2.2 实现 RGB565 直接写入（2 字节/像素，无转换）

## 3. 验证

- [x] 3.1 模拟器测试 RGB565 渲染
- [x] 3.2 真机测试格式回退兼容性
# 开源声明页面集成说明

## ✅ 已完成的工作

### 1. 创建了开源声明页面
**文件**: `entry/src/main/ets/pages/OpenSourceNotices.ets`

**功能特性**:
- 📱 美观的深色主题界面，与应用整体风格一致
- 📋 展示所有使用的开源组件列表
- 🔍 可展开/收起查看详细信息
- 🎨 根据许可证类型显示不同颜色标签
- 🔗 提供组件主页和源码链接
- 📄 包含许可证摘要文本
- ℹ️ 底部说明如何获取源码和重新链接

### 2. 在主页面添加入口按钮
**修改文件**: `entry/src/main/ets/pages/Index.ets`

**改动内容**:
- 在机身右上角添加了"关于"按钮
- 点击后跳转到开源声明页面
- 使用 HarmonyOS router API 进行页面导航

### 3. 注册新页面路由
**修改文件**: `entry/src/main/resources/base/profile/main_pages.json`

**改动内容**:
- 添加 `pages/OpenSourceNotices` 到页面列表

---

## 🎨 页面设计特点

### 视觉设计
- **配色方案**: 与应用保持一致的深色系 (#1a1e26, #23272e, #2a2f38)
- **许可证颜色编码**:
  - 🔴 GPL-3.0: 红色系 (#ff6b6b)
  - 🟠 GPL-2.0: 橙色系 (#ffa94d)
  - 🟢 LGPL: 绿色系 (#51cf66)
  - 🔵 BSD: 蓝色系 (#4dabf7)

### 交互设计
- 点击组件卡片展开/收起详细信息
- 平滑的动画过渡效果
- 支持滚动浏览所有组件
- 返回按钮回到主页面

### 信息架构
```
开源声明页面
├── 标题栏（返回按钮 + 标题）
├── 说明文本
├── 组件列表（可展开）
│   ├── VMRP (GPL-3.0)
│   ├── Unicorn Engine (GPL-2.0/LGPL-2.0)
│   ├── FluidSynth (LGPL-2.1+)
│   ├── libsndfile (LGPL-2.1)
│   ├── libogg (BSD 3-Clause)
│   └── zlib (zlib/libpng)
└── 底部说明
    ├── 如何获取源码
    ├── 重新链接说明
    └── 版本信息
```

---

## 📱 使用方法

### 用户视角
1. 打开应用
2. 点击右上角的"关于"按钮
3. 查看开源组件列表
4. 点击任意组件查看详细信息
5. 点击左上角返回按钮回到主页面

### 开发者视角
如需添加新的开源组件，编辑 `OpenSourceNotices.ets` 中的 `OPEN_SOURCE_COMPONENTS` 数组：

```typescript
const OPEN_SOURCE_COMPONENTS: OpenSourceComponent[] = [
  {
    name: '组件名称',
    version: '版本号',
    license: '许可证类型',
    homepage: 'https://homepage.url',
    sourceUrl: 'https://source.url',
    description: '组件描述',
    licenseText: '许可证摘要文本'
  },
  // ... 更多组件
];
```

---

## 🔧 技术实现

### 关键技术点

1. **路由导航**
   ```typescript
   import { router } from '@kit.ArkUI';
   
   router.pushUrl({
     url: 'pages/OpenSourceNotices'
   });
   ```

2. **可展开列表**
   - 使用 `@State expandedIndex` 跟踪当前展开的项
   - 点击时切换展开状态
   - 条件渲染详细内容

3. **动态样式**
   - 根据许可证类型返回不同颜色
   - `getLicenseColor()` 和 `getLicenseBgColor()` 方法

4. **响应式布局**
   - 使用 `List` 和 `ForEach` 渲染组件列表
   - 自适应滚动条
   - 弹性边缘效果

---

## ✅ 合规性检查清单

- [x] 列出所有使用的开源组件
- [x] 标明每个组件的许可证类型
- [x] 提供组件主页链接
- [x] 提供源码获取方式
- [x] 包含许可证文本或摘要
- [x] 说明如何获取完整源码
- [x] 提供 LGPL 组件的重新链接说明
- [x] 标注最后更新日期和应用版本

---

## 🎯 下一步建议

### 可选增强功能

1. **添加复制链接功能**
   ```typescript
   // 长按链接时复制到剪贴板
   .onLongPress(() => {
     pasteboard.setData(pasteboard.createData('text/plain', url));
     promptAction.showToast({ message: '已复制链接' });
   })
   ```

2. **添加搜索功能**
   - 在顶部添加搜索框
   - 实时过滤组件列表

3. **添加许可证全文查看**
   - 点击许可证标签查看完整文本
   - 创建单独的许可证详情页面

4. **国际化支持**
   - 提取字符串到资源文件
   - 支持中英文切换

5. **添加二维码**
   - 为每个组件生成二维码
   - 方便手机扫描访问

---

## 📝 注意事项

1. **更新组件信息**
   - 当添加新的第三方库时，记得更新此页面
   - 保持许可证信息的准确性

2. **许可证变更**
   - 如果某个组件更新了许可证，及时同步
   - 定期检查依赖的许可证状态

3. **源码获取**
   - 确保提供的链接仍然有效
   - 定期测试链接可访问性

4. **法律审查**
   - 建议在正式发布前由法律专业人士审查
   - 确保符合所有许可证要求

---

## 🐛 故障排除

### 问题：点击"关于"按钮无反应

**解决方案**:
1. 检查 `main_pages.json` 是否正确注册了页面
2. 确认 `OpenSourceNotices.ets` 文件路径正确
3. 查看 DevEco Studio 控制台是否有错误日志

### 问题：页面样式异常

**解决方案**:
1. 确认使用了正确的系统图标资源
2. 检查颜色值是否符合 HarmonyOS 规范
3. 验证布局参数是否合理

### 问题：返回列表为空

**解决方案**:
1. 检查 `OPEN_SOURCE_COMPONENTS` 数组是否为空
2. 确认 `ForEach` 的 key 生成函数正确
3. 查看控制台是否有渲染错误

---

**最后更新**: 2026-04-18  
**适用版本**: mrpohos 1.0.0

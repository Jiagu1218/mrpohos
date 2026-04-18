# 开源许可证合规性分析报告

**项目名称**: mrpohos (HarmonyOS MRP 应用运行时)  
**分析日期**: 2026-04-18  
**分析目的**: 评估项目依赖的开源组件是否可以用于商业上架（华为应用市场）

---

## 📊 总体结论

| 依赖项 | 许可证类型 | 商业可用性 | 风险等级 | 关键要求 |
|--------|-----------|-----------|---------|---------|
| **VMRP** | GPL-3.0 | ⚠️ **高风险** | 🔴 高 | 必须开源整个项目 |
| **Unicorn Engine** | GPL-2.0 + LGPL-2.0 | ⚠️ **高风险** | 🔴 高 | 动态链接可规避，但需仔细审查 |
| **FluidSynth** | LGPL-2.1+ | ✅ **可用** | 🟡 中 | 动态链接 + 提供源码/修改 |
| **HarmonyOS SDK** | 专有许可 | ✅ **可用** | 🟢 低 | 遵循华为开发者协议 |

---

## 🔍 详细分析

### 1. VMRP (Virtual MythRoad Platform)

**许可证**: GNU General Public License v3.0 (GPL-3.0)  
**文件位置**: `entry/src/main/cpp/vmrp/LICENSE`

#### ⚠️ 核心问题

**GPL-3.0 是强传染性许可证**，主要条款包括：

1. **衍生作品必须开源**（第5条）
   - 如果您的代码与 GPL 代码链接（静态或动态），整个作品被视为衍生作品
   - 必须将整个项目的源代码公开

2. **分发时必须提供完整源码**（第6条）
   - 包括所有修改和依赖的 GPL 组件
   - 必须以相同许可证（GPL-3.0）发布

3. **不能附加额外限制**（第10条）
   - 不能禁止反向工程
   - 不能限制用户修改和重新分发的权利

#### 🚫 商业上架影响

**如果您计划闭源商业化：**
- ❌ **不能直接上架** - GPL 要求您开源整个项目
- ❌ 违反 GPL 可能导致法律诉讼和赔偿
- ❌ 华为应用市场可能拒绝包含 GPL 代码的闭源应用

**如果您愿意开源：**
- ✅ 可以开源整个项目后上架
- ✅ 必须在应用中明确标注使用 GPL 组件
- ✅ 必须提供获取源码的方式（如 GitHub 链接）

#### 💡 建议方案

1. **替换 VMRP** - 寻找 MIT/BSD/Apache 许可的替代方案
2. **完全开源** - 将 mrpohos 以 GPL-3.0 许可证开源
3. **隔离架构** - 将 VMRP 作为独立进程运行（技术上复杂，法律风险仍存在）

---

### 2. Unicorn Engine

**许可证**: 
- 主许可证: GNU General Public License v2.0 (GPL-2.0)
- 部分组件: GNU Library General Public License v2.0 (LGPL-2.0)

**文件位置**: 
- `entry/src/main/cpp/unicorn/COPYING` (GPL-2.0)
- `entry/src/main/cpp/unicorn/COPYING.LGPL2` (LGPL-2.0)

#### ⚠️ 核心问题

Unicorn Engine 采用**双重许可**策略：

**GPL-2.0 部分**（主要代码）：
- 与 GPL-3.0 类似，具有强传染性
- 如果静态链接，您的代码也必须开源
- 版本2不允许"或更高版本"选项，与 GPL-3.0 不兼容

**LGPL-2.0 部分**（库文件）：
- 允许动态链接而不传染主程序
- 用户可以替换库文件（必须提供目标文件以便重新链接）
- 对库本身的修改必须开源

#### 🔍 您的使用情况

从 CMakeLists.txt 分析：
```cmake
add_subdirectory(unicorn)  # 直接编译进 libentry.so
target_link_libraries(entry PUBLIC unicorn)  # 静态链接
```

**当前是静态链接**，这意味着：
- ❌ GPL-2.0 的传染性生效
- ❌ 整个 libentry.so 被视为衍生作品
- ❌ 您的应用需要遵守 GPL-2.0

#### 💡 建议方案

1. **改为动态链接**（推荐）
   ```cmake
   # 将 Unicorn 编译为独立 .so
   add_library(unicorn SHARED ...)
   # 动态链接
   target_link_libraries(entry PRIVATE unicorn)
   ```
   - ✅ LGPL 部分允许动态链接
   - ⚠️ 仍需确认哪些代码是 LGPL vs GPL
   - ✅ 提供 Unicorn 的目标文件以便用户重新链接

2. **购买商业许可证**
   - Unicorn 官方可能提供商业授权
   - 联系作者获取商业使用许可

3. **替换为其他仿真引擎**
   - 寻找 MIT/BSD 许可的 CPU 仿真器
   - 例如：QEMU 的部分组件（LGPL）

---

### 3. FluidSynth

**许可证**: GNU Lesser General Public License v2.1 or later (LGPL-2.1+)  
**来源**: https://github.com/FluidSynth/fluidsynth

#### ✅ 核心优势

**LGPL 比 GPL 宽松得多**：

1. **动态链接不传染**（第6条）
   - 如果您的应用仅通过动态库调用 FluidSynth
   - 您的主程序不需要开源
   - 可以保持闭源商业化

2. **允许商业使用**
   - 可以在商业产品中免费使用
   - 无需支付版权费

3. **修改库本身才需要开源**
   - 如果您修改了 FluidSynth 源码
   - 只需开源对 FluidSynth 的修改部分
   - 不需要开源您的主程序

#### 📋 合规要求

**您必须做到：**

1. ✅ **动态链接**（已满足）
   ```cmake
   target_link_libraries(entry PRIVATE libfluidsynth.so)
   ```

2. ✅ **声明使用 LGPL 组件**
   - 在应用的"关于"页面或设置中添加声明
   - 示例文本：
     ```
     本产品使用了 FluidSynth (LGPL-2.1+)
     源码地址: https://github.com/FluidSynth/fluidsynth
     ```

3. ✅ **提供获取 FluidSynth 源码的方式**
   - 在应用中提供链接
   - 或在下载页面提供源码包

4. ⚠️ **如果修改了 FluidSynth**
   - 必须开源您的修改
   - 以 LGPL-2.1+ 许可证发布修改部分

#### 🎯 HarmonyOS 适配说明

根据搜索结果，OpenHarmony/HarmonyOS 官方已在系统中使用 FluidSynth：
- OpenHarmony 采用**动态链接**方式集成
- 符合 LGPL 许可证要求
- 您的使用方式与官方一致，风险较低

---

### 4. 其他依赖

#### libsndfile (FluidSynth 依赖)
- **许可证**: LGPL-2.1+
- **状态**: ✅ 安全（动态链接）
- **参考**: OpenHarmony 已使用此库

#### libogg (FluidSynth 依赖)
- **许可证**: BSD 3-Clause
- **状态**: ✅ 非常安全（无传染性）

#### zlib
- **许可证**: zlib/libpng License
- **状态**: ✅ 非常安全

---

## 🚨 风险评估总结

### 当前状态（静态链接 Unicorn + GPL VMRP）

| 风险项 | 严重程度 | 可能性 | 综合风险 |
|--------|---------|-------|---------|
| GPL-3.0 违规（VMRP） | 🔴 高 | 🔴 高 | 🔴 **极高** |
| GPL-2.0 违规（Unicorn） | 🔴 高 | 🟡 中 | 🔴 **高** |
| LGPL 合规（FluidSynth） | 🟢 低 | 🟢 低 | 🟢 **低** |

**整体评估**: 🔴 **不建议当前状态下架闭源应用**

---

## 💡 合规路径建议

### 方案 A: 完全开源（推荐用于学习/社区项目）

**步骤：**
1. 将整个 mrpohos 项目以 **GPL-3.0** 许可证开源
2. 在 GitHub/Gitee 上公开完整源码
3. 在应用中添加 GPL 声明
4. 可以免费上架（但不能禁止他人免费分发）

**优点：**
- ✅ 完全合法合规
- ✅ 获得社区贡献
- ✅ 建立技术声誉

**缺点：**
- ❌ 无法闭源商业化
- ❌ 竞争对手可以免费使用您的代码

---

### 方案 B: 替换 GPL 组件（推荐用于商业项目）

**步骤：**

1. **替换 VMRP (GPL-3.0)**
   - 选项1: 自行开发 MRP 运行时（MIT 许可证）
   - 选项2: 寻找其他开源 MRP 实现（检查许可证）
   - 选项3: 联系 VMRP 作者获取商业授权

2. **处理 Unicorn (GPL-2.0/LGPL-2.0)**
   - 改为动态链接 `.so` 文件
   - 确保仅使用 LGPL 部分的 API
   - 提供 Unicorn 源码和重新链接说明

3. **保留 FluidSynth (LGPL-2.1+)**
   - 继续保持动态链接
   - 添加合规声明

**预期结果：**
- ✅ 可以闭源商业化
- ✅ 符合华为应用市场上架要求
- ⚠️ 需要投入开发成本替换组件

---

### 方案 C: 混合架构（技术复杂）

**思路：**
- 将 GPL 组件隔离到独立进程
- 通过 IPC（进程间通信）交互
- 主应用保持闭源

**技术实现：**
```
┌─────────────────────┐
│  主应用 (闭源)       │
│  - ArkTS UI         │
│  - 业务逻辑          │
└──────────┬──────────┘
           │ IPC (Socket/Shared Memory)
┌──────────▼──────────┐
│  MRP 引擎 (GPL)      │
│  - VMRP             │
│  - Unicorn          │
│  - 独立进程          │
└─────────────────────┘
```

**法律争议：**
- ⚠️ 某些律师认为这仍构成"衍生作品"
- ⚠️ FSF（自由软件基金会）持严格解释
- ⚠️ 存在法律灰色地带

**不建议采用此方案**，除非咨询专业知识产权律师。

---

## 📝 立即行动清单

### 如果选择开源（方案 A）

- [ ] 创建 GitHub/Gitee 仓库
- [ ] 添加 LICENSE 文件（GPL-3.0）
- [ ] 在 README 中声明使用的开源组件
- [ ] 在应用"关于"页面添加开源声明
- [ ] 准备源码包供用户下载

### 如果选择商业化（方案 B）

- [ ] **紧急**: 评估替换 VMRP 的工作量
- [ ] 调研其他 MRP 运行时实现
- [ ] 将 Unicorn 改为动态链接
- [ ] 审查所有第三方库的许可证
- [ ] 咨询知识产权律师
- [ ] 准备开源组件声明文档
- [ ] 测试动态链接后的功能完整性

---

## 📞 资源链接

### 许可证文本
- [GPL-3.0 中文版](https://www.gnu.org/licenses/gpl-3.0.zh-cn.html)
- [GPL-2.0 英文版](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html)
- [LGPL-2.1 英文版](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html)

### 官方指南
- [GNU GPL 常见问题](https://www.gnu.org/licenses/gpl-faq.html)
- [FSF 许可证合规指南](https://www.fsf.org/licensing/)
- [OpenHarmony 第三方软件许可证说明](https://gitee.com/openharmony/docs/blob/master/zh-cn/contribute/第三方开源软件及许可证说明.md)

### 华为应用市场
- [华为应用市场上架规范](https://developer.huawei.com/consumer/cn/doc/distribution/app/agc-help-reviewstandard-0000001053921759)
- [开源组件声明要求](https://developer.huawei.com/consumer/cn/doc/distribution/app/agc-help-prepareappinfo-0000001053731758)

---

## ⚖️ 免责声明

**本报告仅供参考，不构成法律建议。**

- 开源许可证的解释可能存在争议
- 不同司法管辖区的法律适用可能不同
- 建议在商业化前咨询专业知识产权律师
- 本文作者不对因使用本报告导致的任何损失负责

---

**最后更新**: 2026-04-18  
**分析师**: AI Assistant  
**审核状态**: 待人工审核

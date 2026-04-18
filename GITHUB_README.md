# mrpohos - HarmonyOS MRP 应用运行时

[![License](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-HarmonyOS-green.svg)](https://developer.harmonyos.com/)
[![Version](https://img.shields.io/badge/version-1.0.0-orange.svg)](AppScope/app.json5)

## 📱 项目简介

**mrpohos** 是一个运行在 HarmonyOS（鸿蒙系统）上的 MRP（MythRoad Platform）应用虚拟机/模拟器。它基于 **VMRP** 和 **Unicorn** CPU 仿真引擎，能够在现代 HarmonyOS 设备上运行传统的功能机 MRP 格式应用程序和游戏。

🔗 **项目地址**: https://github.com/Jiagu1218/mrpohos

## ✨ 核心特性

- ✅ 完整 MRP 运行时支持
- ✅ ARM CPU 仿真（Unicorn Engine）
- ✅ 实时 MIDI 音频（FluidSynth + OHAudio）
- ✅ OpenGL ES 渲染（EGL/GLES2）
- ✅ GBK 编码支持
- ✅ 震动反馈
- ✅ 文件导入功能
- ✅ 经典功能机 UI

## 🚀 快速开始

### 前置要求

- DevEco Studio 6.0.2+
- HarmonyOS SDK API 12+
- NDK & CMake 3.5.0+

### 构建步骤

```bash
# 1. 克隆项目
git clone https://github.com/Jiagu1218/mrpohos.git
cd mrpohos

# 2. 使用 DevEco Studio 打开项目
# 3. 同步依赖并构建
# 4. 运行到设备或模拟器
```

### 导入 MRP 应用

1. 启动应用后，点击机身右侧的 **"导入"** 侧键
2. 从系统文件选择器中选择 `.mrp` 文件
3. 文件将自动复制到 `mythroad/` 沙箱目录

## 📖 详细文档

完整的文档请查看 [README.md](README.md)，包含：

- 技术架构说明
- 详细的构建指南
- 使用说明和按键映射
- 开发指南
- 故障排除
- OpenGL ES 渲染调试状态

## 📄 许可证

本项目采用 **GPL-3.0** 许可证开源。

**重要提示**：
- 本项目使用了多个 GPL/LGPL 许可的开源组件
- 您可以自由使用、修改和分发，但必须遵守 GPL-3.0 许可证
- 衍生作品也必须以 GPL-3.0 开源
- 详见 [LICENSE](LICENSE) 文件和 [合规性分析](LICENSE_COMPLIANCE.md)

## 🙏 致谢

- [VMRP](https://github.com/vmrp/vmrp) - Virtual MythRoad Platform
- [Unicorn Engine](https://www.unicorn-engine.org/) - CPU 仿真框架
- [FluidSynth](https://www.fluidsynth.org/) - MIDI 合成器

## 📞 联系方式

- **GitHub**: https://github.com/Jiagu1218/mrpohos
- **Issues**: [提交问题](https://github.com/Jiagu1218/mrpohos/issues)

---

**注意**：本项目仅供学习和研究用途。运行商业 MRP 应用前，请确保您拥有相应的版权许可。

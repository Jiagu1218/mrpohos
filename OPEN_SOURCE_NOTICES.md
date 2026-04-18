# 开源组件声明

本应用使用了以下开源软件组件。我们感谢这些开源项目的贡献者。

---

## 1. VMRP (Virtual MythRoad Platform)

**版本**: 未知  
**许可证**: GNU General Public License v3.0 (GPL-3.0)  
**主页**: https://github.com/vmrp/vmrp  
**源码获取**: 请联系应用开发者获取完整源码

VMRP 是 MRP 应用运行时的核心实现，用于仿真和执行 MRP 格式的应用程序。

**GPL-3.0 声明**:
```
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.
```

---

## 2. Unicorn Engine

**版本**: 未知  
**许可证**: GNU General Public License v2.0 (GPL-2.0) / GNU Library General Public License v2.0 (LGPL-2.0)  
**主页**: https://www.unicorn-engine.org/  
**源码获取**: https://github.com/unicorn-engine/unicorn

Unicorn Engine 是一个轻量级的多架构 CPU 仿真框架，用于执行 ARM 指令集。

**GPL-2.0/LGPL-2.0 声明**:
```
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.
```

---

## 3. FluidSynth

**版本**: 未知  
**许可证**: GNU Lesser General Public License v2.1 or later (LGPL-2.1+)  
**主页**: https://www.fluidsynth.org/  
**源码获取**: https://github.com/FluidSynth/fluidsynth

FluidSynth 是一个实时 MIDI 合成器，用于播放 MRP 应用中的 MIDI 音乐。

**LGPL-2.1+ 声明**:
```
This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
Lesser General Public License for more details.
```

**注意**: FluidSynth 以动态链接库（.so）形式使用，符合 LGPL 许可证要求。您可以替换此库文件的版本。

---

## 4. libsndfile (FluidSynth 依赖)

**许可证**: GNU Lesser General Public License v2.1 (LGPL-2.1)  
**主页**: http://www.mega-nerd.com/libsndfile/  
**源码获取**: https://github.com/libsndfile/libsndfile

---

## 5. libogg (FluidSynth 依赖)

**许可证**: BSD 3-Clause License  
**主页**: https://www.xiph.org/ogg/  
**源码获取**: https://github.com/xiph/ogg

---

## 6. zlib

**许可证**: zlib/libpng License  
**主页**: https://zlib.net/  
**源码获取**: https://github.com/madler/zlib

---

## 如何获取源码

根据 GPL 和 LGPL 许可证的要求，我们提供获取上述开源组件完整源代码的方式：

### 方式 1: 在线获取

大多数组件的源码可以在其官方 GitHub 仓库获取（见上方链接）。

### 方式 2: 联系开发者

如果您无法通过上述方式获取源码，或需要特定版本的源码，请通过以下方式联系我们：

- **Email**: [your-email@example.com]
- **GitHub**: [your-github-repo-url]

我们将在收到请求后的 3 个工作日内提供完整的源码包。

### 方式 3: 随应用分发

部分组件的源码可能已包含在应用的安装包中，路径为：
```
/assets/open-source-sources/
```

---

## 重新链接说明（针对 LGPL 组件）

对于使用 LGPL 许可证的组件（如 FluidSynth），如果您修改了这些库并希望重新链接到您的应用中：

1. 获取修改后的库源码
2. 编译生成新的 `.so` 文件
3. 替换应用中的对应库文件
4. 确保目标文件（object files）可用以便重新链接

我们提供必要的主程序目标文件，以便您能够重新链接修改后的库。如需获取，请联系开发者。

---

## 免责声明

上述开源组件按其原样提供，不提供任何形式的明示或暗示担保。在任何情况下，版权持有者或贡献者不对因使用本软件而产生的任何索赔、损害或其他责任负责。

---

**最后更新**: 2026-04-18  
**应用版本**: 1.0.0

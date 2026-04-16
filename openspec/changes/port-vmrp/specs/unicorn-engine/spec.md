## 新增需求

### 需求:Unicorn Engine 编译集成
系统必须将 Unicorn Engine 作为 HarmonyOS 原生库编译，并集成到应用的 CMake 构建系统中。

#### 场景:成功编译
- **当** 执行 HarmonyOS 项目构建
- **那么** Unicorn Engine 必须成功编译为 ARM64 和 x86_64 架构的静态/动态库，且无编译错误

#### 场景:链接成功
- **当** vmrp 核心代码编译时
- **那么** 必须能正确链接 Unicorn Engine 的头文件和库文件，uc_open、uc_emu_start 等符号必须可解析

### 需求:Unicorn ARM 模拟初始化
系统必须能通过 Unicorn Engine 创建 ARM 32位模拟引擎实例，分配模拟内存空间。

#### 场景:引擎创建
- **当** 调用模拟器初始化
- **那么** 系统必须创建 UC_ARCH_ARM + UC_MODE_ARM 的 Unicorn 引擎实例，分配模拟内存并映射到 Unicorn 地址空间

#### 场景:内存布局正确
- **当** 引擎初始化完成
- **那么** 代码段(CODE_ADDRESS)、栈空间(STACK_ADDRESS)、内存管理区(MEMORY_MANAGER_ADDRESS)必须按 vmrp 定义的地址正确映射

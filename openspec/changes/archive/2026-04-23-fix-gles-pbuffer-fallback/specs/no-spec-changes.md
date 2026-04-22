本变更不引入新功能也不修改现有规范级需求。

变更范围限于实现层面的错误恢复策略：将 PBuffer 路径中 `GL_VERSION == null` 的处理从"永久放弃 GPU"改为"降级重试 Window Surface 路径"。此修改不影响 `path-encoding` 规范的需求定义，不涉及新的能力暴露。

## 1. 在 bridge.c 中实现编码转换

- [x] 1.1 在 `bridge_init()` 中设置 `FLAG_USE_UTF8_FS | FLAG_USE_UTF8_EDIT`（已完成）
- [ ] 1.2 在 bridge.c 中添加 `gbkToUtf8()` 和 `utf8ToGbk()` 辅助函数，复用 encode.c 的转换函数
- [ ] 1.3 在所有接受文件路径的 bridge 函数中调用 `gbkToUtf8()` 转换：`br_mr_open`、`br_info`、`br_mr_remove`、`br_mr_rename`、`br_mr_mkDir`、`br_mr_rmDir`、`br_opendir`、`br_mr_getLen`
- [ ] 1.4 在 `br_readdir` 中对返回的 `d_name` 调用 `utf8ToGbk()` 转换

## 2. 验证

- [ ] 2.1 编译项目，确认无编译错误
- [ ] 2.2 部署到模拟器，确认 MRP 应用正常运行

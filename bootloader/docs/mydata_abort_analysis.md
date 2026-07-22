# U-Boot `mydata abort` 故障分析

## 1. 结论

当前 `u-boot.bin` 的异常由新增命令 `cmd/mytest.c` 引起。

`cmd/mytest.c` 只包含了 `<command.h>`，没有先包含 `<common.h>`。本版本 U-Boot 的板级配置通过 `<common.h>` 间接引入；其中 i.MX6 公共配置定义了 `CONFIG_SYS_LONGHELP` 和 `CONFIG_AUTO_COMPLETE`。因此：

- `cmd/mytest.c` 编译时看不到上述两个宏，生成的 `cmd_tbl_t` 命令项只有 20 字节；
- 其他正常命令源文件先包含 `<common.h>`，生成的命令项是 28 字节；
- 链接器只是把各命令项连续拼到 `.u_boot_list`，不会为 20 字节的异常条目补齐到 28 字节；
- 命令查找和自动补全代码统一按 `sizeof(cmd_tbl_t) == 28` 遍历，经过 `mytest` 后发生 8 字节错位；
- 自动补全扫描到错误解释的命令项时，把 `0x00020200` 当作 `cmdtp->name` 传给 `strlen()`，访问未映射地址后触发 Data Abort。

这是一个**同一数据结构在不同编译单元中布局不一致（ABI 不一致）**的问题。日志中的 `FEC1 address not set` 与本次 Data Abort 没有因果关系；`Reset cause: WDOG` 是异常处理后复位留下的结果，不是首发原因。

## 2. 产物一致性

分析使用工作区当前 ELF、map 和二进制：

| 产物 | 时间 | 大小 |
|---|---:|---:|
| `u-boot` | 2026-07-21 23:37:37 +0800 | 2,572,096 B |
| `u-boot.bin` | 2026-07-21 23:37:37 +0800 | 410,648 B |
| `u-boot.map` | 2026-07-21 23:37:37 +0800 | 423,221 B |

串口横幅为 `U-Boot 2016.03 (Jul 21 2026 - 23:37:33 +0800)`，时间一致；当前 `u-boot` 还带有 DWARF 调试信息，可直接完成源码定位。

## 3. 异常地址还原

串口日志：

```text
pc : 9ff86c90        lr : 9ff628f4
reloc pc : 8783ec90  lr : 8781a8f4
r2 : 00020200        r0 : 00020200
```

运行时重定位偏移为：

```text
0x9ff86c90 - 0x8783ec90 = 0x18748000
0x9ff628f4 - 0x8781a8f4 = 0x18748000
```

用当前 ELF 解析重定位前地址：

```text
0x8783ec90  strlen()       lib/string.c:259
0x8781a8f4  complete_cmdv  common/command.c:217
```

相关反汇编为：

```asm
8781a8ec: ldr r0, [r7, #-28]       /* r0 = cmdtp->name */
8781a8f0: bl  8783ec7c <strlen>
8781a8f4: cmp r0, sl               /* 返回地址 LR */

8783ec88: ldrb r1, [r2]            /* strlen 实际访存 */
8783ec8c: cmp  r1, #0
8783ec90: bne  8783ec80
```

ARM 异常现场的 PC 显示在循环分支 `0x8783ec90`，实际造成异常的是同一循环内对 `[r2]` 的字节读取。现场 `r0 = r2 = 0x00020200`，说明 `strlen()` 收到的字符串地址就是非法值 `0x00020200`。LR 精确落在 `complete_cmdv()` 调用 `strlen(cmdtp->name)` 的下一条指令，因此调用链证据闭合。

该路径由命令行自动补全触发，典型触发方式是在倒计时或命令行阶段按 Tab，而不是执行 `mytest` 命令本身。

## 4. 命令表损坏证据

ELF 符号表显示：

```text
8785ae1c  size 28  _u_boot_list_2_cmd_2_mw
8785ae38  size 20  _u_boot_list_2_cmd_2_mytest
8785ae4c  size 28  _u_boot_list_2_cmd_2_nand
8785ae68  size 28  _u_boot_list_2_cmd_2_nboot
```

`cmd/mytest.o` 自身也明确显示：

```text
.u_boot_list_2_cmd_2_mytest  Size 0x14 (20 bytes)
```

正常的 `cmd/nand.o` 命令项为 28 字节。`common/command.c` 的自动补全循环按 `cmd_tbl_t *` 做 `cmdtp++`，当前主程序中的步长是 28 字节。由于 `mytest` 实际只占 20 字节，下一项虽然从 `0x8785ae4c` 开始，遍历器却会从错误边界读取，后续字段不再对应 `name/maxargs/repeatable/cmd/usage/help/complete`。

`include/command.h` 中该结构的两个条件字段正好解释 8 字节差异：

```c
#ifdef CONFIG_SYS_LONGHELP
char *help;                 /* 4 bytes */
#endif
#ifdef CONFIG_AUTO_COMPLETE
int (*complete)(...);       /* 4 bytes */
#endif
```

板级 `include/configs/mx6_common.h` 定义了这两个宏，但 `cmd/mytest.c` 没有通过 `<common.h>` 引入板级配置，两个字段都被裁掉。

## 5. 编译告警证据

`build.log` 已经对新增源文件给出关键告警：

```text
warning: 'struct cmd_tbl' declared inside parameter list
warning: implicit declaration of function 'printf'
warning: initialization from incompatible pointer type
warning: (near initialization for '_u_boot_list_2_cmd_2_mytest.cmd')
```

原因有两个：

1. 缺少 `<common.h>`，既丢失板级配置宏，也没有得到 `printf()` 声明；
2. 本版本类型名是 `cmd_tbl_t`（底层标签为 `struct cmd_tbl_s`），不是较新版本常见的 `struct cmd_tbl`。

这些告警不应忽略。第 1 项直接造成命令表布局不一致；第 2 项造成命令回调类型不匹配。

## 6. 修复方案

建议将 `cmd/mytest.c` 修改为：

```diff
--- a/cmd/mytest.c
+++ b/cmd/mytest.c
@@
+#include <common.h>
 #include <command.h>
 
-static int do_mytest(struct cmd_tbl *cmdtp, int flag,
+static int do_mytest(cmd_tbl_t *cmdtp, int flag,
                      int argc, char *const argv[])
```

完整的函数和 `U_BOOT_CMD()` 其余部分无需改变。不要通过给 `mytest` 条目手工补两个空指针来绕过问题，因为这会依赖配置并继续保留回调类型错误。

修复后必须执行干净构建，避免旧对象文件继续混入：

```sh
make distclean
make mx6ull_14x14_ddr256_nand_defconfig
make CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc)
```

如果项目实际使用 `configs/mytest_defconfig`，第二条应替换为：

```sh
make mytest_defconfig
```

烧写时应使用本次干净构建新生成的实际启动镜像，并核对串口横幅时间，避免误烧旧的 `imgs/u-boot.bin`。具体烧写文件名仍以当前 NAND 启动脚本的封装流程为准。

## 7. 修复后验证

### 7.1 静态验证

```sh
arm-linux-gnueabihf-readelf -sW u-boot \
  | grep _u_boot_list_2_cmd_2_mytest
```

期望 `mytest` 的 `Size` 从 20 变为 28，且相邻命令符号地址间距连续：

```text
mytest size = 28
nand address = mytest address + 0x1c
```

同时确认构建日志中不再出现以下告警：

- `struct cmd_tbl declared inside parameter list`
- `implicit declaration of function printf`
- `initialization from incompatible pointer type`

### 7.2 板上验证

1. 启动后核对 U-Boot 编译时间为新产物时间；
2. 在命令行输入空字符、`m`、`my` 后反复按 Tab，确认不再 Data Abort；
3. 执行 `help mytest`，确认帮助信息正常；
4. 执行 `mytest`，期望输出 `mytest command executed`；
5. 再检查 `help nand`、`nand info` 等位于 `mytest` 后面的命令，确认命令表后半段均正常。

## 8. 建议的防回归措施

- 本版本所有 `cmd/*.c` 统一以 `<common.h>` 开头，再包含 `<command.h>`；
- 新增命令使用本版本的 `cmd_tbl_t *` 回调签名，不从其他 U-Boot 版本直接复制 `struct cmd_tbl *`；
- 将新增源文件产生的编译告警视为构建失败，至少在 CI 中启用 `KCFLAGS=-Werror` 或针对上述告警做阻断；
- 在产物检查阶段校验 `.u_boot_list_2_cmd_2_*` 的命令对象大小一致。当前配置下所有普通命令项都应为 28 字节。


# platform_led_test 启动时报 shell 语法错误分析

## 1. 测试日志

目标板执行编译后的测试程序：

```text
/home # ./platform_led_test /dev/platform_led 1
./platform_led_test: line 3: syntax error: unterminated quoted string
/home # ./platform_led_test /dev/platform_led 0
./platform_led_test: line 3: syntax error: unterminated quoted string
```

## 2. 现象判断

错误信息来自 `/bin/sh`，而不是 `platform_led_test` 自身的 `fprintf()`。程序没有进入
`main()`，因此尚未执行设备打开、LED 写入或参数检查。

当内核无法直接执行一个文件时，交互式 shell 可能将该文件当作 shell 脚本再次解析。
二进制 ELF 被当作脚本解析时，随机字节会被解释成 shell 文本，常见结果就是
`unterminated quoted string`。因此该现象优先指向可执行文件格式、架构或部署文件错误，
不是 LED 驱动的 `write()` 返回值问题。

## 3. 可能原因

### 3.1 复制了错误文件

本工程用户态程序的实际输出路径是：

```text
/home/gs/code/i.MX6UL/drivers/21-platform/app/platform_led_test
```

目标板应复制该文件，而不是 `platform_led_test.c`、主机上的同名程序或其他目录下的
测试文件。

### 3.2 目标架构不匹配

`app/Makefile` 使用：

```makefile
CROSS_COMPILE ?= /usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-
```

目标板是 ARM，应使用 ARM 交叉编译结果。如果误用主机 `gcc` 生成 x86 ELF，目标板
无法执行；如果交叉编译器输出的 ABI 与目标系统不匹配，也可能出现同类问题。

### 3.3 文件传输损坏或文本模式传输

通过不正确的传输方式复制 ELF，或传输中断导致文件截断，也会使内核无法识别文件格式。
二进制文件必须使用二进制传输，并确认目标板文件大小与主机一致。

## 4. 主机侧检查

在开发主机执行：

```bash
cd /home/gs/code/i.MX6UL/drivers/21-platform
make clean
make app
file app/platform_led_test
readelf -h app/platform_led_test | grep -E 'Class|Data|Machine|Type'
```

预期 `file` 或 `readelf` 显示 ARM 32-bit ELF，例如 `Machine: ARM`。如果显示
x86-64，说明没有使用正确的交叉编译器。

本次主机实际检查结果为：

```text
21-platform/app/platform_led_test: ELF 64-bit LSB executable, ARM aarch64,
statically linked
Class:   ELF64
Machine: AArch64
```

这与 i.MX6ULL 的 ARMv7 32 位执行环境不匹配，是本次目标板启动失败的确定根因。
主机当前环境中的 `CROSS_COMPILE` 被设置为 `aarch64-linux-gnu-`，覆盖了 `app/Makefile`
中的默认值，所以实际调用了 `aarch64-linux-gnu-gcc`。

后续复测时，目标板仍报告相同错误：

```text
/home # ./platform_led_test /dev/platform_led 0
./platform_led_test: line 3: syntax error: unterminated quoted string
/home # ./platform_led_test /dev/platform_led 1
./platform_led_test: line 3: syntax error: unterminated quoted string
```

目标板执行 `od -An -tx1 -N4 platform_led_test` 得到 `7f 45 4c 46`，只能证明文件
具有 ELF magic，不能证明其是 ARMv7 32 位程序。主机侧检查仍显示 `ELF 64-bit` 和
`AArch64`，因此当前部署到目标板的仍是错误的 AArch64 构建产物，尚未替换为正确的
ARM 32 位文件。

## 5. 目标板侧检查

将正确文件复制到目标板后，执行：

```bash
ls -l platform_led_test
od -An -tx1 -N4 platform_led_test
```

ARM ELF 通常以 ELF magic 开头：

```text
7f 45 4c 46
```

如果目标板有 `file` 命令，再执行：

```bash
file platform_led_test
```

确认文件不是脚本、源代码或 x86 可执行文件。复制完成后可比较主机和目标板的字节数：

```bash
# 主机
stat -c '%s' app/platform_led_test

# 目标板（若 stat 支持）
stat -c '%s' platform_led_test
```

## 6. 解决方案

推荐重新编译并复制 `app/platform_led_test`：

```bash
cd /home/gs/code/i.MX6UL/drivers/21-platform
make clean
env CROSS_COMPILE=/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf- make app
file app/platform_led_test
```

如果直接进入 `app` 目录构建，也必须显式指定 32 位 ARM 工具链：

```bash
cd /home/gs/code/i.MX6UL/drivers/21-platform/app
make clean
env CROSS_COMPILE=/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf- make
```

`file app/platform_led_test` 应显示 `ELF 32-bit` 和 `ARM`，不能显示 `ELF 64-bit` 或
`AArch64`。

然后将以下文件以二进制方式部署到目标板：

```text
app/platform_led_test  →  /home/platform_led_test
platform_led.ko       →  /home/platform_led.ko
```

赋予执行权限并运行：

```bash
chmod +x /home/platform_led_test
/home/platform_led_test /dev/platform_led 1
/home/platform_led_test /dev/platform_led 0
```

如果目标板仍报告格式错误，应确认内核配置支持 ARM ELF、动态链接器路径和 ABI；本
工程用户态 Makefile 使用 `-static`，因此正常情况下不依赖目标板动态链接器。

## 7. 预期结果

文件格式正确后，程序应进入自身参数检查和设备访问流程，输出类似：

```text
PASS: LED turned on
PASS: LED turned off
```

内核应分别记录：

```text
platform_led: LED turned on
platform_led: LED turned off
```

## 8. 结论

本次 `unterminated quoted string` 不是 LED 操作失败，而是目标板没有将
`platform_led_test` 作为可执行 ELF 正确启动。应重新使用 ARM 交叉编译器构建，部署
`app/platform_led_test` 二进制文件，并在目标板核对 ELF magic、架构和文件大小。

本次仅生成分析文档，未修改驱动和测试程序源码。

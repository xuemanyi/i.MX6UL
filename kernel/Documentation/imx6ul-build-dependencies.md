# i.MX6UL 内核编译依赖与 Git 状态说明

## 本次编译问题及修复

内核构建过程中需要使用 `lzop` 生成 LZO 格式的压缩镜像。主机缺少该程序时，构建会在生成相关镜像阶段失败。本次通过以下命令补齐主机依赖：

```sh
sudo apt-get install lzop
```

交叉编译器由 `build-kernel.sh` 指定为：

```text
/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf
```

运行编译前，需要保证该目录存在，并且主机已安装内核构建所需的基础工具以及 `lzop`。

## 为什么 `git status` 没有显示 `lzop`

`lzop` 是由系统包管理器安装到工作树之外的主机程序，不是仓库内的文件。因此 Git 无法、也不应该跟踪它，`git status` 不显示该程序与 `.gitignore` 无关。

仓库中的内核 `.gitignore` 会忽略下列构建生成文件：

- `.config`、`include/generated/` 等配置及生成头文件；
- `*.o`、`*.ko`、`*.cmd`、`Module.symvers` 等中间文件；
- `vmlinux`、`zImage`、压缩镜像等最终产物。

这些文件均可由已跟踪的源码、默认配置和构建脚本重新生成，不属于编译输入，因此不应取消对应的忽略规则或提交到仓库。

## `kernel-source/` 与源码压缩包

工作树中的 `kernel-source/` 是当前内核源码树的完整副本，约 690 MiB；它已被 `git status` 识别为未跟踪目录，所以也不存在被 `.gitignore` 隐藏的问题。将同一份源码重复提交会显著增大仓库，并不能补齐新的编译依赖。

`bootloader/linux-imx-4.1.15-2.1.0-g3dc0a4b-v2.7.tar.bz2` 属于 bootloader 源码归档，与本次内核编译修复无关，也不是本内核目录的构建输入。

## 可复现编译

```sh
cd kernel
./build-kernel.sh
```

脚本会检查交叉编译工具链、清理旧产物、载入已纳入版本控制的 `imx_v7_defconfig`、打开配置界面，并使用 8 个并行任务完成构建。配置界面未做修改时直接保存退出即可。

若构建报告某个命令不存在，应安装对应的主机软件包并在本文档记录包名；不要提交从该工具生成的中间文件或二进制产物。

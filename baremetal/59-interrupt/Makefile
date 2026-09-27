# i.MX6UL 裸机构建规则：收集各目录源码并生成 ELF、BIN 和反汇编文件

# 交叉编译工具前缀

# 最终目标名称
TARGET        ?= bsp

# 工具链

# 头文件搜索目录
INCDIRS := imx6ul \
           bsp/clk \
           bsp/led \
           bsp/delay \
		   bsp/beep \
		   bsp/gpio \
		   bsp/key \
		   bsp/int \
		   bsp/exit

# 源码目录
SRCDIRS := project \
           bsp/clk \
           bsp/led \
           bsp/delay \
		   bsp/beep \
		   bsp/gpio \
		   bsp/key \
		   bsp/int \
		   bsp/exit

# 目标文件输出目录
OBJDIR := obj

# 生成头文件搜索参数
INCLUDE := $(addprefix -I,$(INCDIRS))

# 收集源码文件
SFILES := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.S))
CFILES := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.c))

# 将源码名称转换为 obj/ 下的目标文件名称
OBJS := $(patsubst %.S,$(OBJDIR)/%.o,$(notdir $(SFILES))) \
        $(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(CFILES)))

# 允许 make 在源码目录中查找依赖
VPATH := $(SRCDIRS)

# 裸机环境不链接宿主标准库
CFLAGS  := -Wall -O2 -nostdlib -ffreestanding $(INCLUDE)
ASFLAGS := $(CFLAGS)

# 使用自定义链接脚本
LDFLAGS := -Timx6ul.lds

.PHONY: all clean

# 默认目标
all: $(TARGET).bin

# 依次生成 ELF、BIN 和反汇编文件
$(TARGET).bin: $(OBJS)
	@echo "Linking objects:"
	@echo $(OBJS)
	$(LD) $(LDFLAGS) -o $(TARGET).elf $^
	$(OBJCOPY) -O binary -S $(TARGET).elf $@
	$(OBJDUMP) -D -m arm $(TARGET).elf > $(TARGET).dis

# 编译汇编文件
$(OBJDIR)/%.o: %.S | $(OBJDIR)
	$(CC) $(ASFLAGS) -c -o $@ $<

# 编译 C 文件
$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

# 按需创建目标文件目录
$(OBJDIR):
	mkdir -p $@

# 清理全部构建产物
clean:
	rm -rf $(OBJDIR) $(TARGET).elf $(TARGET).dis $(TARGET).bin
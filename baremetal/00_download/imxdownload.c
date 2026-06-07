#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHELLCMD_LEN 200
#define BIN_OFFSET   3072

/*
 * Recovered from the imxdownload ELF .rodata section.  The first 1 KiB is
 * the i.MX6UL IVT/DCD header; the remaining space before BIN_OFFSET is zero.
 */
const int imx6_ivtdcd_table[256] = {
    0x402000d1, 0x87800000, 0x00000000, 0x877ff42c, 0x877ff420, 0x877ff400, 0x00000000, 0x00000000,
    0x877ff000, 0x00200000, 0x00000000, 0x40e801d2, 0x04e401cc, 0x68400c02, 0xffffffff, 0x6c400c02,
    0xffffffff, 0x70400c02, 0xffffffff, 0x74400c02, 0xffffffff, 0x78400c02, 0xffffffff, 0x7c400c02,
    0xffffffff, 0x80400c02, 0xffffffff, 0xb4040e02, 0x00000c00, 0xac040e02, 0x00000000, 0x7c020e02,
    0x30000000, 0x50020e02, 0x30000000, 0x4c020e02, 0x30000000, 0x90040e02, 0x30000000, 0x88020e02,
    0x30000c00, 0x70020e02, 0x00000000, 0x60020e02, 0x30000000, 0x64020e02, 0x30000000, 0xa0040e02,
    0x30000000, 0x94040e02, 0x00000200, 0x80020e02, 0x30000000, 0x84020e02, 0x30000000, 0xb0040e02,
    0x00000200, 0x98040e02, 0x30000000, 0xa4040e02, 0x30000000, 0x44020e02, 0x30000000, 0x48020e02,
    0x30000000, 0x1c001b02, 0x00800000, 0x00081b02, 0x030039a1, 0x0c081b02, 0x0b000300, 0x3c081b02,
    0x44014801, 0x48081b02, 0x302c4040, 0x50081b02, 0x343e4040, 0x1c081b02, 0x33333333, 0x20081b02,
    0x33333333, 0x2c081b02, 0x333333f3, 0x30081b02, 0x333333f3, 0xc0081b02, 0x09409400, 0xb8081b02,
    0x00080000, 0x04001b02, 0x2d000200, 0x08001b02, 0x3030331b, 0x0c001b02, 0xf3526b67, 0x10001b02,
    0x630b6db6, 0x14001b02, 0xdb00ff01, 0x18001b02, 0x40172000, 0x1c001b02, 0x00800000, 0x2c001b02,
    0xd2260000, 0x30001b02, 0x23106b00, 0x40001b02, 0x4f000000, 0x00001b02, 0x00001884, 0x90081b02,
    0x00004000, 0x1c001b02, 0x32800002, 0x1c001b02, 0x33800000, 0x1c001b02, 0x31800400, 0x1c001b02,
    0x30802015, 0x1c001b02, 0x40800004, 0x20001b02, 0x00080000, 0x18081b02, 0x27020000, 0x04001b02,
    0x2d550200, 0x04041b02, 0x06100100, 0x1c001b02, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

void message_print(void)
{
    printf("I.MX6UL bin download software\r\n");
    printf("Edit by:zuozhongkai\r\n");
    printf("Date:2018/8/9\r\n");
    printf("Version:V1.0\r\n");
}

int main(int argc, char *argv[])
{
    FILE *fp;
    unsigned char *buf;
    unsigned char *cmdbuf;
    int nbytes, filelen;
    int i = 0, j = 0;

    message_print();

    if (argc != 3) {
        printf("Error Usage! Reference Below:\r\n");
        printf("sudo ./%s <source_bin> <sd_device>\r\n", argv[0]);
        return -1;
    }

    fp = fopen(argv[1], "rb");
    if (fp == NULL) {
        printf("Can't Open file %s\r\n", argv[1]);
        return -1;
    }

    fseek(fp, 0L, SEEK_END);
    filelen = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    printf("file %s size = %dBytes\r\n", argv[1], filelen);

    buf = malloc(filelen + BIN_OFFSET);
    if (buf == NULL) {
        printf("Mem Malloc Failed!\r\n");
        fclose(fp);
        return -1;
    }

    memset(buf, 0, filelen + BIN_OFFSET);
    fread(buf + BIN_OFFSET, 1, filelen, fp);
    fclose(fp);
    memcpy(buf, imx6_ivtdcd_table, sizeof(imx6_ivtdcd_table));

    printf("Delete Old load.imx\r\n");
    system("rm -rf load.imx");
    printf("Create New load.imx\r\n");
    system("touch load.imx");

    fp = fopen("load.imx", "wb");
    if (fp == NULL) {
        printf("Cant't Open load.imx!!!\r\n");
        free(buf);
        return -1;
    }

    nbytes = fwrite(buf, 1, filelen + BIN_OFFSET, fp);
    if (nbytes != filelen + BIN_OFFSET) {
        printf("File Write Error!\r\n");
        free(buf);
        fclose(fp);
        return -1;
    }

    free(buf);
    fclose(fp);

    cmdbuf = malloc(SHELLCMD_LEN);
    sprintf((char *)cmdbuf,
            "sudo dd iflag=dsync oflag=dsync if=load.imx of=%s bs=512 seek=2",
            argv[2]);
    printf("Download load.imx to %s  ......\r\n", argv[2]);
    system((char *)cmdbuf);
    free(cmdbuf);

    return 0;
}

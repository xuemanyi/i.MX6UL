#include <common.h>
#include <command.h>

static int do_mytest(cmd_tbl_t *cmdtp, int flag,
                     int argc, char *const argv[])
{
        printf("mytest command executed\n");

        return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
        mytest,
        1,
        0,
        do_mytest,
        "run my custom test command",
        ""
);

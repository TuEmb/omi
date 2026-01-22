#include "button.h"

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/shell/shell.h>

static const struct device *const buttons = DEVICE_DT_GET(DT_ALIAS(buttons));

/* Keep for main.c compatibility */
K_MSGQ_DEFINE(input_button, sizeof(struct input_event), 10, 1);

static void input_cb(struct input_event *evt, void *user_data)
{
    ARG_UNUSED(user_data);
    (void)k_msgq_put(&input_button, evt, K_NO_WAIT);
}

INPUT_CALLBACK_DEFINE(buttons, input_cb, NULL);

static int cmd_buttons_check(const struct shell *sh, size_t argc, char **argv)
{
    int ret;

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (!device_is_ready(buttons)) {
        shell_error(sh, "Button device not ready");
        return 0;
    }

    ret = pm_device_runtime_get(buttons);
    if (ret < 0 && ret != -ENOTSUP) {
        shell_error(sh, "Failed to get device (%d)", ret);
        return 0;
    }

    k_msgq_purge(&input_button);
    shell_print(sh, "Waiting for button input (5s timeout)...");

    while (1) {
        struct input_event evt;

        ret = k_msgq_get(&input_button, &evt, K_SECONDS(5));
        if (ret == -EAGAIN) {
            shell_error(sh, "No input received");
            break;
        }

        if (evt.code == INPUT_KEY_ENTER) {
            if (evt.value == 1) {
                shell_print(sh, "usr button pressed");
            } else {
                shell_print(sh, "usr button released");
            }
        }
    }

    pm_device_runtime_put(buttons);
    return 0;
}

static int cmd_buttons_test_one(const struct shell *sh, size_t argc, char **argv)
{
    int ret;
    bool got_press = false;
    bool got_release = false;

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (!device_is_ready(buttons)) {
        shell_error(sh, "Button device not ready");
        return -1;
    }

    ret = pm_device_runtime_get(buttons);
    if (ret < 0 && ret != -ENOTSUP) {
        shell_error(sh, "Failed to get device (%d)", ret);
        return -1;
    }

    k_msgq_purge(&input_button);
    shell_print(sh, "Press and release button (5s timeout)...");

    while (!got_press || !got_release) {
        struct input_event evt;

        ret = k_msgq_get(&input_button, &evt, K_SECONDS(5));
        if (ret == -EAGAIN) {
            shell_error(sh, "Timeout - no input received");
            pm_device_runtime_put(buttons);
            return -1;
        }

        if (evt.code == INPUT_KEY_ENTER) {
            if (evt.value == 1 && !got_press) {
                shell_print(sh, "Button pressed");
                got_press = true;
            } else if (evt.value == 0 && got_press && !got_release) {
                shell_print(sh, "Button released");
                got_release = true;
            }
        }
    }

    shell_print(sh, "Button Test PASSED");
    pm_device_runtime_put(buttons);
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_buttons_cmds,
                               SHELL_CMD(check, NULL, "Check buttons", cmd_buttons_check),
                               SHELL_CMD(test_one, NULL, "Check one button press", cmd_buttons_test_one),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(button, &sub_buttons_cmds, "Buttons", NULL);
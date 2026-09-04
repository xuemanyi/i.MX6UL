// SPDX-License-Identifier: GPL-2.0
/*
* deep_consumer.c - Consumer of versioned service API
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>

#include "deep_service.h"

static const struct deep_service_api *service_api;

static long value_a = 10;
module_param(value_a, long, 0444);
MODULE_PARM_DESC(value_a, "First operand");

static long value_b = 5;
module_param(value_b, long, 0444);
MODULE_PARM_DESC(value_b, "Second operand");

static char *operation = "add";
module_param(operation, charp, 0444);
MODULE_PARM_DESC(operation, "Operation: add, subtract or multiply");

static bool reset_stats;
module_param(reset_stats, bool, 0444);
MODULE_PARM_DESC(reset_stats, "Reset provider statistics during load");

static const char *deep_operation_name(enum deep_service_operation operation)
{
    switch (operation) {
        case DEEP_SERVICE_ADD:
            return "add";

        case DEEP_SERVICE_SUBTRACT:
            return "subtract";

        case DEEP_SERVICE_MULTIPLY:
            return "multiply";

        default:
            return "unknown";
    }
}

static int deep_parse_operation(enum deep_service_operation *selected)
{
    if (!selected || !operation)
        return -EINVAL;

    if (!strcmp(operation, "add")) {
        *selected = DEEP_SERVICE_ADD;
        return 0;
    }

    if (!strcmp(operation, "subtract")) {
        *selected = DEEP_SERVICE_SUBTRACT;
        return 0;
    }

    if (!strcmp(operation, "multiply")) {
        *selected = DEEP_SERVICE_MULTIPLY;
        return 0;
    }

    return -EINVAL;
}

static int deep_event_callback(struct notifier_block *nb,
    unsigned long event, void *data)
{
    const struct deep_service_event_data *event_data = data;

    switch (event) {
    case DEEP_SERVICE_EVENT_CALCULATION:
        if (!event_data)
            return NOTIFY_BAD;

        pr_info("deep_consumer: event sequence=%llu, operation=%s, result=%lld\n",
                event_data->sequence,
                deep_operation_name(event_data->operation),
                event_data->result);
        break;

    case DEEP_SERVICE_EVENT_RESET:
        pr_info("deep_consumer: provider statistics reset\n");
        break;

    default:
        return NOTIFY_DONE;
    }

    return NOTIFY_OK;
}

static struct notifier_block deep_notifier = {
    .notifier_call = deep_event_callback,
};

static int deep_validate_api(const struct deep_service_api *api)
{
    if (!api)
        return -ENODEV;

    if (api->version != DEEP_SERVICE_API_VERSION)
        return -EPROTO;

    if (api->size < sizeof(struct deep_service_api))
        return -EPROTO;

    if (!api->calculate || !api->get_stats ||
        !api->reset_stats || !api->register_notifier ||
        !api->unregister_notifier)
        return -EINVAL;

    return 0;
}

static int __init deep_consumer_init(void)
{
    struct deep_service_request request;
    struct deep_service_result result;
    struct deep_service_stats stats;
    enum deep_service_operation selected;
    int ret;

    ret = deep_parse_operation(&selected);
    if (ret) {
        pr_err("deep_consumer: invalid operation \"%s\"\n",
                operation ?: "<null>");
        return ret;
    }

    service_api = deep_service_get_api();

    ret = deep_validate_api(service_api);
    if (ret) {
        pr_err("deep_consumer: incompatible provider API: %d\n", ret);
        goto err_put_api;
    }

    ret = service_api->register_notifier(&deep_notifier);
    if (ret) {
        pr_err("deep_consumer: failed to register notifier: %d\n", ret);
        goto err_put_api;
    }

    if (reset_stats) {
        ret = service_api->reset_stats();
        if (ret) {
            pr_err("deep_consumer: failed to reset statistics: %d\n", ret);
            goto err_unregister_notifier;
        }
    }

    request.operation = selected;
    request.operand_a = value_a;
    request.operand_b = value_b;

    ret = service_api->calculate(&request, &result);
    if (ret) {
        pr_err("deep_consumer: calculation failed: %d\n", ret);
        goto err_unregister_notifier;
    }

    ret = service_api->get_stats(&stats);
    if (ret) {
        pr_err("deep_consumer: failed to read statistics: %d\n", ret);
        goto err_unregister_notifier;
    }

    pr_info("deep_consumer: %ld %s %ld = %lld, sequence=%llu\n",
            value_a, operation, value_b,
            result.value, result.sequence);

    pr_info("deep_consumer: calculations=%llu errors=%llu\n",
            stats.calculation_count, stats.error_count);

    return 0;

err_unregister_notifier:
    service_api->unregister_notifier(&deep_notifier);

err_put_api:
    if (service_api)
            deep_service_put_api(service_api);

    service_api = NULL;

    return ret;
}

static void __exit deep_consumer_exit(void)
{
    int ret;

    ret = service_api->unregister_notifier(&deep_notifier);
    if (ret)
        pr_warn("deep_consumer: failed to unregister notifier: %d\n",ret);

    deep_service_put_api(service_api);
    service_api = NULL;

    pr_info("deep_consumer: unloaded\n");
}

module_init(deep_consumer_init);
module_exit(deep_consumer_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("Consumer of versioned service API");
MODULE_VERSION("1.0");

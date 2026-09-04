// SPDX-License-Identifier: GPL-2.0
/*
* deep_provider.c - Versioned service API provider
*/

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/string.h>

#include "deep_service.h"

struct deep_service_context {
    struct mutex lock;
    struct blocking_notifier_head notifier;
    struct deep_service_stats stats;
};

static struct deep_service_context service_context;

static bool allow_multiply = true;
module_param(allow_multiply, bool, 0644);
MODULE_PARM_DESC(allow_multiply, "Allow multiply operations");

/*
 * Linux 4.1 does not provide check_{add,sub,mul}_overflow().  Check the
 * bounds before evaluating the expression so that signed overflow itself
 * never occurs.
 */
static bool deep_add_overflow(s64 a, s64 b, s64 *result)
{
    if ((b > 0 && a > S64_MAX - b) ||
        (b < 0 && a < S64_MIN - b))
        return true;

    *result = a + b;
    return false;
}

static bool deep_sub_overflow(s64 a, s64 b, s64 *result)
{
    if ((b < 0 && a > S64_MAX + b) ||
        (b > 0 && a < S64_MIN + b))
        return true;

    *result = a - b;
    return false;
}

static bool deep_mul_overflow(s64 a, s64 b, s64 *result)
{
    bool negative = (a < 0) != (b < 0);
    u64 multiplicand = a < 0 ? -(u64)a : (u64)a;
    u64 multiplier = b < 0 ? -(u64)b : (u64)b;
    u64 limit = negative ? (1ULL << 63) : (u64)S64_MAX;
    u64 product = 0;

    /* Avoid 64-bit division, whose ARM helper is not exported to modules. */
    while (multiplier) {
        if ((multiplier & 1) && multiplicand > limit - product)
            return true;

        if (multiplier & 1)
            product += multiplicand;

        multiplier >>= 1;
        if (multiplier) {
            if (multiplicand > (limit >> 1))
                return true;
            multiplicand <<= 1;
        }
    }

    if (negative)
        *result = product == (1ULL << 63) ? S64_MIN : -(s64)product;
    else
        *result = (s64)product;
    return false;
}

static int deep_calculate(const struct deep_service_request *request,
    struct deep_service_result *result)
{
    struct deep_service_event_data event;
    s64 value;
    int ret = 0;

    if (!request || !result)
        return -EINVAL;

    switch (request->operation) {
    case DEEP_SERVICE_ADD:
        if (deep_add_overflow(request->operand_a, request->operand_b, &value))
            ret = -EOVERFLOW;
        break;

    case DEEP_SERVICE_SUBTRACT:
        if (deep_sub_overflow(request->operand_a,
                            request->operand_b, &value))
            ret = -EOVERFLOW;
        break;

    case DEEP_SERVICE_MULTIPLY:
        if (!READ_ONCE(allow_multiply))
            return -EOPNOTSUPP;

        if (deep_mul_overflow(request->operand_a,
                                request->operand_b, &value))
            ret = -EOVERFLOW;
        break;

    default:
        return -EINVAL;
    }

    mutex_lock(&service_context.lock);

    if (ret) {
        service_context.stats.error_count++;
        mutex_unlock(&service_context.lock);
        return ret;
    }

    service_context.stats.calculation_count++;
    service_context.stats.last_sequence++;

    result->value = value;
    result->sequence = service_context.stats.last_sequence;

    event.operation = request->operation;
    event.operand_a = request->operand_a;
    event.operand_b = request->operand_b;
    event.result = value;
    event.sequence = result->sequence;

    mutex_unlock(&service_context.lock);

    /*
     * Do not hold service_context.lock while calling external callbacks.
     * A callback could sleep or call back into this service.
     */
    blocking_notifier_call_chain(&service_context.notifier,
                                    DEEP_SERVICE_EVENT_CALCULATION,
                                    &event);

    return 0;
}

static int deep_get_stats(struct deep_service_stats *stats)
{
    if (!stats)
        return -EINVAL;

    mutex_lock(&service_context.lock);
    *stats = service_context.stats;
    mutex_unlock(&service_context.lock);

    return 0;
}

static int deep_reset_stats(void)
{
    mutex_lock(&service_context.lock);
    memset(&service_context.stats, 0,
            sizeof(service_context.stats));
    mutex_unlock(&service_context.lock);

    blocking_notifier_call_chain(&service_context.notifier,
                                    DEEP_SERVICE_EVENT_RESET, NULL);

    return 0;
}

static int deep_register_notifier(struct notifier_block *nb)
{
    if (!nb || !nb->notifier_call)
        return -EINVAL;

    return blocking_notifier_chain_register(&service_context.notifier, nb);
}

static int deep_unregister_notifier(struct notifier_block *nb)
{
    if (!nb)
        return -EINVAL;

    return blocking_notifier_chain_unregister(&service_context.notifier, nb);
}

static const struct deep_service_api service_api = {
    .version = DEEP_SERVICE_API_VERSION,
    .size = sizeof(struct deep_service_api),
    .calculate = deep_calculate,
    .get_stats = deep_get_stats,
    .reset_stats = deep_reset_stats,
    .register_notifier = deep_register_notifier,
    .unregister_notifier = deep_unregister_notifier,
};

const struct deep_service_api *deep_service_get_api(void)
{
    if (!try_module_get(THIS_MODULE))
        return NULL;

    return &service_api;
}
EXPORT_SYMBOL_GPL(deep_service_get_api);

void deep_service_put_api(const struct deep_service_api *api)
{
    if (WARN_ON(api != &service_api))
        return;

    module_put(THIS_MODULE);
}
EXPORT_SYMBOL_GPL(deep_service_put_api);

static int __init deep_provider_init(void)
{
    mutex_init(&service_context.lock);
    BLOCKING_INIT_NOTIFIER_HEAD(&service_context.notifier);

    pr_info("deep_provider: API version %u loaded\n",
        DEEP_SERVICE_API_VERSION);

    return 0;
}

static void __exit deep_provider_exit(void)
{
    pr_info("deep_provider: unloaded\n");
}

module_init(deep_provider_init);
module_exit(deep_provider_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Snowclad");
MODULE_DESCRIPTION("Versioned service API provider module");
MODULE_VERSION("1.0");

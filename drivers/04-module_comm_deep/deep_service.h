/* SPDX-License-Identifier: GPL-2.0 */
#ifndef DEEP_SERVICE_H
#define DEEP_SERVICE_H

#include <linux/notifier.h>
#include <linux/types.h>

#define DEEP_SERVICE_API_VERSION        1

enum deep_service_operation {
    DEEP_SERVICE_ADD,
    DEEP_SERVICE_SUBTRACT,
    DEEP_SERVICE_MULTIPLY,
};

enum deep_service_event {
    DEEP_SERVICE_EVENT_CALCULATION,
    DEEP_SERVICE_EVENT_RESET,
};

struct deep_service_request {
    enum deep_service_operation operation;
    s64 operand_a;
    s64 operand_b;
};

struct deep_service_result {
    s64 value;
    u64 sequence;
};

struct deep_service_stats {
    u64 calculation_count;
    u64 error_count;
    u64 last_sequence;
};

struct deep_service_event_data {
    enum deep_service_operation operation;
    s64 operand_a;
    s64 operand_b;
    s64 result;
    u64 sequence;
};

struct deep_service_api {
    u32 version;
    u32 size;

    int (*calculate)(const struct deep_service_request *request,
                        struct deep_service_result *result);
    int (*get_stats)(struct deep_service_stats *stats);
    int (*reset_stats)(void);

    int (*register_notifier)(struct notifier_block *nb);
    int (*unregister_notifier)(struct notifier_block *nb);
};

const struct deep_service_api *deep_service_get_api(void);
void deep_service_put_api(const struct deep_service_api *api);

#endif /* DEEP_SERVICE_H */
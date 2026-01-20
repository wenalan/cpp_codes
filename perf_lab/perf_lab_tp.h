/* perf_lab_tp.h */
#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER perf_lab

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "perf_lab_tp.h"

#if !defined(_PERF_LAB_TP_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define _PERF_LAB_TP_H

#include <lttng/tracepoint.h>

/*
 * 注意：
 * 1) TP_ARGS:  type, name, type, name ... 成对出现
 * 2) TP_FIELDS: 里面每个 ctf_* 宏之间不要写逗号，用换行分隔
 * 3) ctf_integer 的签名是：ctf_integer(type, field_name, var)
 *    ctf_string  的签名是：ctf_string(field_name, var)
 */
TRACEPOINT_EVENT(
    perf_lab,
    work_iter,
    TP_ARGS(
        int, mode,
        const char *, cstring,
        unsigned long, iters,
        long, sum
    ),
    TP_FIELDS(
        ctf_integer(int, mode, mode)
        ctf_string(msg, cstring)
        ctf_integer(unsigned long, iters, iters)
        ctf_integer(long, sum, sum)
    )
)

TRACEPOINT_EVENT(
    perf_lab,
    phase_begin,
    TP_ARGS(
        const char *, mode,
        const char *, variant,
        unsigned long long, t_ns
    ),
    TP_FIELDS(
        ctf_string(mode, mode)
        ctf_string(variant, variant)
        ctf_integer(unsigned long long, t_ns, t_ns)
    )
)

TRACEPOINT_EVENT(
    perf_lab,
    phase_end,
    TP_ARGS(
        const char *, mode,
        const char *, variant,
        unsigned long long, t_ns
    ),
    TP_FIELDS(
        ctf_string(mode, mode)
        ctf_string(variant, variant)
        ctf_integer(unsigned long long, t_ns, t_ns)
    )
)


#endif /* _PERF_LAB_TP_H */

#include <lttng/tracepoint-event.h>

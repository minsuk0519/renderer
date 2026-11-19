#pragma once

#include <system\config.hpp>

#if CONFIG_LOG_ENABLED

#include <system\logger_internal.hpp>

#define TC_CHECK(func) do { if (!(func)) { __debugbreak(); } } while (0)
#define TC_ASSERT(cond) do { if (!(cond)) { __debugbreak(); } } while (0)
#define TC_BREAK() __debugbreak()
//this asserstion is only for static condition
#define TC_STATICASSERT(cond) static_assert(cond)

//call error log when the condition failed
#define TC_CONDITION(cond, str) do { if (!(cond)) { TC_LOG_ERROR(str); } } while (0)
//for the return false version. will be mostly used in init function
#define TC_CONDITIONB(cond, str) do { if (!(cond)) { TC_LOG_ERROR(str); return false; } } while (0)

#define TC_APPEND_STR(A, B) #A " " #B

#define TC_INIT(func) do { TC_LOG_INFO(TC_APPEND_STR(func, "initializing start...")); if (!(func)) { TC_LOG_ERROR(TC_APPEND_STR(func, "initializing failed!")); return false; } TC_LOG_INFO(TC_APPEND_STR(func, "initializing end!")); } while (0)

#define TC_LOG_INFO(str) logger::print_log(str, __LINE__, __FILE__, logger::LOG_LEVEL::LOG_INFO)
#define TC_LOG_WARNING(str) logger::print_log(str, __LINE__, __FILE__, logger::LOG_LEVEL::LOG_WARNING)
#define TC_LOG_ERROR(str) logger::print_log(str, __LINE__, __FILE__, logger::LOG_LEVEL::LOG_ERROR)
#define TC_LOG_FATAL(str) logger::print_log(str, __LINE__, __FILE__, logger::LOG_LEVEL::LOG_FATAL)
#define TC_LOG(str) TC_LOG_INFO(str)

#else // CONFIG_LOG_ENABLED

#define TC_CHECK(func) ((void)(func))
#define TC_ASSERT(cond) ((void)sizeof(cond))
#define TC_BREAK() ((void)0)
//this asserstion is only for static condition
#define TC_STATICASSERT(cond) static_assert(cond)

#define TC_CONDITION(cond, str) ((void)(cond))
#define TC_CONDITIONB(cond, str) do { if (!(cond)) { return false; } } while (0)

#define TC_APPEND_STR(A, B) #A " " #B

#define TC_INIT(func) do { if (!(func)) { return false; } } while (0)

#define TC_LOG_INFO(str) ((void)sizeof(str))
#define TC_LOG_WARNING(str) ((void)sizeof(str))
#define TC_LOG_ERROR(str) ((void)sizeof(str))
#define TC_LOG_FATAL(str) ((void)sizeof(str))
#define TC_LOG(str) TC_LOG_INFO(str)

#endif // #if CONFIG_LOG_ENABLED
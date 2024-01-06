#pragma once

#include <cassert>

#include <system\config.hpp>

#if CONFIG_LOG_ENABLED

#include <system\logger_internal.hpp>

#define TC_CHECK(func) if(!func) __debugbreak()
#define TC_BREAK(cond) if(!cond) __debugbreak()
//this asserstion is only for static condition
#define TC_ASSERT(cond) static_assert(cond)

//call error log when the condition failed
#define TC_CONDITION(cond, str) if(!(cond)) TC_LOG_ERROR(str)
//for the return false version. will be mostly used in init function
#define TC_CONDITIONB(cond, str) if(!(cond)) \
								{ TC_LOG_ERROR(str); \
								return false;}

#define TC_APPEND_STR(A, B) #A " " #B

#define TC_INIT(func)	TC_LOG_INFO(TC_APPEND_STR(func, " start...")); \
						TC_CHECK(func); \
						TC_LOG_INFO(TC_APPEND_STR(func, " end!"))

#define TC_LOG_INFO(str) logger::print_log(str, __LINE__, __FILE__, logger::LOG_LEVEL::LOG_INFO)
#define TC_LOG_WARNING(str) logger::print_log(str, __LINE__, __FILE__, logger::LOG_LEVEL::LOG_WARNING)
#define TC_LOG_ERROR(str) logger::print_log(str, __LINE__, __FILE__, logger::LOG_LEVEL::LOG_ERROR)
#define TC_LOG_FATAL(str) logger::print_log(str, __LINE__, __FILE__, logger::LOG_LEVEL::LOG_FATAL)
#define TC_LOG(str) TC_LOG_INFO(str)

#endif // #if CONFIG_LOG_ENABLED
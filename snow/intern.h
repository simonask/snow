#ifndef INTERN_H_WXDJG2OI
#define INTERN_H_WXDJG2OI

#if defined(__GNUC__) && defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ != 1
#error "Please don't include intern.h in header files."
#endif

#include "snow/snow.h"
#include "snow/basic.h"
#include "snow/symbol.h"
#include "snow/arch.h"
#include "snow/objects.h"
#include "snow/gc.h"
#include "snow/exception.h"

CAPI void warn(const char* msg, ...);
CAPI void error(const char* msg, ...);
#ifdef DEBUG
CAPI void debug(const char* msg, ...);
#else
CAPI inline void debug(const char* msg, ...) {}
#endif

#define _QUOTEME(x) #x
#define Q(x) _QUOTEME(x)

#ifdef DTRACE
#define DTRACE_PROBE(X) SNOWPROBE_ ## X
#include "snow_provider.h"
#else
#define DTRACE_PROBE(X) 
#endif

// API convenience

#define SNOW_FUNC(NAME) static VALUE NAME(SnContext* _context)
#define SELF (_context->self)
#define ARGS (_context->args->data->data)
#define ARG_BY_SYM_AT(SYM, EXPECTED_POSITION) (_context->args ? snow_arguments_get_by_name_at(_context->args, (SYM), (EXPECTED_POSITION)) : NULL)
#define ARG_BY_NAME_AT(NAME, EXPECTED_POSITION) ARG_BY_SYM_AT(snow_symbol(NAME), (EXPECTED_POSITION))
#define ARG_BY_SYM(SYM) (_context->args ? (snow_arguments_get_by_name(_context->args, (SYM))) : NULL)
#define ARG_BY_NAME(NAME) ARG_BY_SYM(snow_symbol(NAME))
#define NUM_ARGS (_context->args ? _context->args->data->size : 0)
#define REQUIRE_ARGS(N) do { if (_context->args->data->size < (N)) snow_throw_exception_with_description("Expected %d argument%s for function call.", (N), (N) == 1 ? "" : "s"); } while (0)

#define SNOW_GC_ALLOC_OBJECT(CLASSNAME) (struct CLASSNAME*)snow_alloc_any_object(CLASSNAME ## Type)

// Implementations that may depend on the definitions above
#include "snow/lock.h"

#endif /* end of include guard: INTERN_H_WXDJG2OI */

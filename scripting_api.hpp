#ifndef SCRIPTING_API_HPP_INCLUDED
#define SCRIPTING_API_HPP_INCLUDED

#include <cstring>

#define DUK_SINGLE_FILE

#define DUK_VERSION                       20200L

#define DUK_GIT_COMMIT                    "external"
#define DUK_GIT_DESCRIBE                  "external"
#define DUK_GIT_BRANCH                    "external"

//#include "duk_config.h"
#include "duktape.h"
#include "scripting_api_fwrd.hpp"

struct duk_memory_functions;
typedef struct duk_memory_functions duk_memory_functions;

typedef duk_ret_t (*duk_c_function)(duk_context *ctx);
typedef void *(*duk_alloc_function) (void *udata, duk_size_t size);
typedef void *(*duk_realloc_function) (void *udata, void *ptr, duk_size_t size);
typedef void (*duk_free_function) (void *udata, void *ptr);
typedef void (*duk_fatal_function) (void *udata, const char *msg);

/*struct duk_memory_functions {
	duk_alloc_function alloc_func;
	duk_realloc_function realloc_func;
	duk_free_function free_func;
	void *udata;
};*/

DUK_EXTERNAL_DECL
void duk_get_memory_functions(duk_context *ctx, duk_memory_functions *out_funcs);

DUK_EXTERNAL_DECL
duk_context *duk_create_heap(duk_alloc_function alloc_func,
                             duk_realloc_function realloc_func,
                             duk_free_function free_func,
                             void *heap_udata,
                             duk_fatal_function fatal_handler);

typedef duk_ret_t (*duk_safe_call_function) (duk_context *ctx, void *udata);

#define duk_create_heap_default() \
	duk_create_heap(NULL, NULL, NULL, NULL, NULL)

DUK_EXTERNAL_DECL void duk_push_undefined(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_push_boolean(duk_context *ctx, duk_bool_t val);
DUK_EXTERNAL_DECL void duk_push_int(duk_context *ctx, duk_int_t val);

DUK_EXTERNAL_DECL void duk_push_this(duk_context *ctx);
DUK_EXTERNAL_DECL duk_idx_t duk_push_c_function(duk_context *ctx, duk_c_function func, duk_idx_t nargs);
DUK_EXTERNAL_DECL duk_idx_t duk_push_proxy(duk_context *ctx, duk_uint_t proxy_flags);
DUK_EXTERNAL_DECL const char *duk_push_string(duk_context *ctx, const char *str);
DUK_EXTERNAL_DECL void duk_push_current_function(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_push_number(duk_context *ctx, duk_double_t val);

DUK_EXTERNAL_DECL duk_idx_t duk_push_object(duk_context *ctx);
DUK_EXTERNAL_DECL duk_idx_t duk_push_array(duk_context *ctx);


DUK_EXTERNAL_DECL void duk_destroy_heap(duk_context *ctx);

DUK_EXTERNAL_DECL const char *duk_json_encode(duk_context *ctx, duk_idx_t idx);
DUK_EXTERNAL_DECL void duk_json_decode(duk_context *ctx, duk_idx_t idx);

DUK_EXTERNAL_DECL duk_bool_t duk_put_prop_index(duk_context *ctx, duk_idx_t obj_idx, duk_uarridx_t arr_idx);

DUK_EXTERNAL_DECL duk_bool_t duk_get_boolean(duk_context *ctx, duk_idx_t idx);
DUK_EXTERNAL_DECL duk_double_t duk_get_number(duk_context *ctx, duk_idx_t idx);
DUK_EXTERNAL_DECL duk_int_t duk_get_int(duk_context *ctx, duk_idx_t idx);
DUK_EXTERNAL_DECL const char *duk_get_string(duk_context *ctx, duk_idx_t idx);
DUK_EXTERNAL_DECL duk_size_t duk_get_length(duk_context *ctx, duk_idx_t idx);
DUK_EXTERNAL_DECL duk_bool_t duk_get_prop_index(duk_context *ctx, duk_idx_t obj_idx, duk_uarridx_t arr_idx);

DUK_EXTERNAL_DECL void duk_pop(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_pop_n(duk_context *ctx, duk_idx_t count);

DUK_EXTERNAL_DECL duk_bool_t duk_put_prop(duk_context *ctx, duk_idx_t obj_idx);
DUK_EXTERNAL_DECL duk_bool_t duk_put_prop_string(duk_context *ctx, duk_idx_t obj_idx, const char *key);
DUK_EXTERNAL_DECL duk_bool_t duk_has_prop_string(duk_context *ctx, duk_idx_t obj_idx, const char *key);
DUK_EXTERNAL_DECL duk_bool_t duk_put_global_string(duk_context *ctx, const char *key);

DUK_EXTERNAL_DECL duk_bool_t duk_get_prop_string(duk_context *ctx, duk_idx_t obj_idx, const char *key);

DUK_EXTERNAL_DECL void duk_freeze(duk_context *ctx, duk_idx_t obj_idx);

DUK_EXTERNAL_DECL void duk_push_heap_stash(duk_context *ctx);

DUK_EXTERNAL_DECL const char *duk_safe_to_lstring(duk_context *ctx, duk_idx_t idx, duk_size_t *out_len);

#define duk_safe_to_string(ctx,idx) \
	duk_safe_to_lstring((ctx), (idx), NULL)

DUK_EXTERNAL_DECL void duk_push_pointer(duk_context *ctx, void *p);
DUK_EXTERNAL_DECL void *duk_get_pointer(duk_context *ctx, duk_idx_t idx);

DUK_EXTERNAL_DECL duk_idx_t duk_get_top(duk_context *ctx);

DUK_EXTERNAL_DECL duk_bool_t duk_is_undefined(duk_context *ctx, duk_idx_t idx);

DUK_EXTERNAL_DECL void duk_dup(duk_context *ctx, duk_idx_t from_idx);

DUK_EXTERNAL_DECL duk_bool_t duk_to_boolean(duk_context *ctx, duk_idx_t idx);

DUK_EXTERNAL_DECL void duk_get_prototype(duk_context *ctx, duk_idx_t idx);
DUK_EXTERNAL_DECL void duk_set_prototype(duk_context *ctx, duk_idx_t idx);

DUK_EXTERNAL_DECL void duk_push_true(duk_context *ctx);

DUK_EXTERNAL_DECL void duk_get_prop_desc(duk_context *ctx, duk_idx_t obj_idx, duk_uint_t flags);

DUK_EXTERNAL_DECL duk_bool_t duk_has_prop(duk_context *ctx, duk_idx_t obj_idx);
DUK_EXTERNAL_DECL duk_bool_t duk_get_prop(duk_context *ctx, duk_idx_t obj_idx);
DUK_EXTERNAL_DECL duk_bool_t duk_del_prop(duk_context *ctx, duk_idx_t obj_idx);


DUK_EXTERNAL_DECL void duk_remove(duk_context *ctx, duk_idx_t idx);

DUK_EXTERNAL_DECL void duk_enum(duk_context *ctx, duk_idx_t obj_idx, duk_uint_t enum_flags);
DUK_EXTERNAL_DECL duk_bool_t duk_next(duk_context *ctx, duk_idx_t enum_idx, duk_bool_t get_value);

DUK_EXTERNAL_DECL void duk_require_stack(duk_context *ctx, duk_idx_t extra);

DUK_EXTERNAL_DECL void duk_call(duk_context *ctx, duk_idx_t nargs);
DUK_EXTERNAL_DECL void duk_call_method(duk_context *ctx, duk_idx_t nargs);


DUK_EXTERNAL_DECL duk_idx_t duk_push_thread_raw(duk_context *ctx, duk_uint_t flags);

#define DUK_THREAD_NEW_GLOBAL_ENV         (1U << 0)

#define duk_push_thread(ctx) \
	duk_push_thread_raw((ctx), 0 /*flags*/)

#define duk_push_thread_new_globalenv(ctx) \
	duk_push_thread_raw((ctx), DUK_THREAD_NEW_GLOBAL_ENV /*flags*/)

DUK_EXTERNAL_DECL duk_context *duk_get_context(duk_context *ctx, duk_idx_t idx);
DUK_EXTERNAL_DECL void duk_set_global_object(duk_context *ctx);
DUK_EXTERNAL_DECL void duk_insert(duk_context *ctx, duk_idx_t to_idx);

DUK_EXTERNAL_DECL void duk_xcopymove_raw(duk_context *to_ctx, duk_context *from_ctx, duk_idx_t count, duk_bool_t is_copy);

#define duk_xmove_top(to_ctx,from_ctx,count) \
	duk_xcopymove_raw((to_ctx), (from_ctx), (count), 0 /*is_copy*/)

DUK_EXTERNAL_DECL void duk_replace(duk_context *ctx, duk_idx_t to_idx);

DUK_EXTERNAL_DECL duk_bool_t duk_check_type_mask(duk_context *ctx, duk_idx_t idx, duk_uint_t mask);

#define DUK_TYPE_NONE                     0U    /* no value, e.g. invalid index */
#define DUK_TYPE_UNDEFINED                1U    /* Ecmascript undefined */
#define DUK_TYPE_NULL                     2U    /* Ecmascript null */
#define DUK_TYPE_BOOLEAN                  3U    /* Ecmascript boolean: 0 or 1 */
#define DUK_TYPE_NUMBER                   4U    /* Ecmascript number: double */
#define DUK_TYPE_STRING                   5U    /* Ecmascript string: CESU-8 / extended UTF-8 encoded */
#define DUK_TYPE_OBJECT                   6U    /* Ecmascript object: includes objects, arrays, functions, threads */
#define DUK_TYPE_BUFFER                   7U    /* fixed or dynamic, garbage collected byte buffer */
#define DUK_TYPE_POINTER                  8U    /* raw void pointer */
#define DUK_TYPE_LIGHTFUNC                9U    /* lightweight function pointer */

#define DUK_TYPE_MASK_NONE                (1U << DUK_TYPE_NONE)
#define DUK_TYPE_MASK_UNDEFINED           (1U << DUK_TYPE_UNDEFINED)
#define DUK_TYPE_MASK_NULL                (1U << DUK_TYPE_NULL)
#define DUK_TYPE_MASK_BOOLEAN             (1U << DUK_TYPE_BOOLEAN)
#define DUK_TYPE_MASK_NUMBER              (1U << DUK_TYPE_NUMBER)
#define DUK_TYPE_MASK_STRING              (1U << DUK_TYPE_STRING)
#define DUK_TYPE_MASK_OBJECT              (1U << DUK_TYPE_OBJECT)
#define DUK_TYPE_MASK_BUFFER              (1U << DUK_TYPE_BUFFER)
#define DUK_TYPE_MASK_POINTER             (1U << DUK_TYPE_POINTER)
#define DUK_TYPE_MASK_LIGHTFUNC           (1U << DUK_TYPE_LIGHTFUNC)

#define duk_is_primitive(ctx,idx) \
	duk_check_type_mask((ctx), (idx), DUK_TYPE_MASK_UNDEFINED | \
	                                  DUK_TYPE_MASK_NULL | \
	                                  DUK_TYPE_MASK_BOOLEAN | \
	                                  DUK_TYPE_MASK_NUMBER | \
	                                  DUK_TYPE_MASK_STRING | \
	                                  DUK_TYPE_MASK_POINTER)

#define duk_is_object_coercible(ctx,idx) \
	duk_check_type_mask((ctx), (idx), DUK_TYPE_MASK_BOOLEAN | \
	                                  DUK_TYPE_MASK_NUMBER | \
	                                  DUK_TYPE_MASK_STRING | \
	                                  DUK_TYPE_MASK_OBJECT | \
	                                  DUK_TYPE_MASK_BUFFER | \
	                                  DUK_TYPE_MASK_POINTER | \
	                                  DUK_TYPE_MASK_LIGHTFUNC)

#define DUK_COMPILE_EVAL                  (1U << 3)    /* compile eval code (instead of global code) */
#define DUK_COMPILE_FUNCTION              (1U << 4)    /* compile function code (instead of global code) */
#define DUK_COMPILE_STRICT                (1U << 5)    /* use strict (outer) context for global, eval, or function code */
#define DUK_COMPILE_SAFE                  (1U << 7)    /* (internal) catch compilation errors */

DUK_EXTERNAL_DECL duk_bool_t duk_is_function(duk_context *ctx, duk_idx_t idx);
DUK_EXTERNAL_DECL duk_bool_t duk_is_object(duk_context *ctx, duk_idx_t idx);
DUK_EXTERNAL_DECL duk_bool_t duk_is_array(duk_context *ctx, duk_idx_t idx);
DUK_EXTERNAL_DECL duk_bool_t duk_is_number(duk_context *ctx, duk_idx_t idx);
DUK_EXTERNAL_DECL duk_bool_t duk_is_string(duk_context *ctx, duk_idx_t idx);


DUK_EXTERNAL_DECL duk_int_t duk_safe_call(duk_context *ctx, duk_safe_call_function func, void *udata, duk_idx_t nargs, duk_idx_t nrets);
DUK_EXTERNAL_DECL duk_int_t duk_pcall_prop(duk_context *ctx, duk_idx_t obj_idx, duk_idx_t nargs);
DUK_EXTERNAL_DECL duk_int_t duk_pcall(duk_context *ctx, duk_idx_t nargs);


#define DUK_EXEC_SUCCESS 0

#define duk_pcompile(ctx,flags)  \
	(duk_compile_raw((ctx), NULL, 0, 2 /*args*/ | (flags) | DUK_COMPILE_SAFE))

DUK_EXTERNAL_DECL duk_int_t duk_compile_raw(duk_context *ctx, const char *src_buffer, duk_size_t src_length, duk_uint_t flags);
DUK_EXTERNAL_DECL void duk_push_global_object(duk_context *ctx);

DUK_EXTERNAL_DECL const char *duk_require_string(duk_context *ctx, duk_idx_t idx);

#define DUK_VARARGS                       ((duk_int_t) (-1))


#endif // SCRIPTING_API_HPP_INCLUDED

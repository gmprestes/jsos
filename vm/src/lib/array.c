#include <stdbool.h>
#include <string.h>
#include "lib.h"
#include "gc.h"
#include "object.h"

typedef struct {
    js_value_t base;
    uint32_t length;
    uint32_t items_length;
    uint32_t capacity;
    VAL* items;
} js_array_t;

static bool statically_initialized;
static js_object_internal_methods_t array_vtable;

static VAL array_length_get(js_vm_t* vm, void* state, VAL this, uint32_t argc, VAL* argv)
{
    if(js_value_get_type(this) != JS_T_ARRAY) {
        js_throw_error(vm->lib.TypeError, "cannot find length of non array");
    }
    return js_value_make_double(((js_array_t*)js_value_get_pointer(this))->length);
}

VAL js_make_array(struct js_vm* vm, uint32_t count, VAL* items)
{
    js_array_t* ary = js_alloc(sizeof(js_array_t));
    ary->base.type = JS_T_ARRAY;
    ary->base.object.vtable = &array_vtable;
    ary->base.object.prototype = vm->lib.Array_prototype;
    ary->base.object.class = vm->lib.Array;
    ary->base.object.properties = js_st_table_new();
    ary->length = count;
    ary->items_length = count;
    ary->capacity = count < 4 ? 4 : count;
    ary->items = js_alloc(sizeof(VAL) * ary->capacity);
    memcpy(ary->items, items, sizeof(VAL) * ary->capacity);
    
    // length property
    js_property_descriptor_t* length = js_alloc(sizeof(js_property_descriptor_t));
    length->is_accessor = true;
    length->enumerable = false;
    length->configurable = false;
    length->accessor.get = js_value_make_native_function(vm, NULL, NULL, array_length_get, NULL);
    length->accessor.set = js_value_undefined();
    st_insert(ary->base.object.properties, (st_data_t)js_cstring("length"), (st_data_t)length);
    
    return js_value_make_pointer((js_value_t*)ary);
}

static void array_put(js_array_t* ary, uint32_t index, VAL val)
{
    if(index >= ary->length) {
        ary->length = index + 1;
    }
    if(index >= ary->capacity) {
        while(index >= ary->capacity) {
            ary->capacity *= 2;
        }
        ary->items = js_realloc(ary->items, sizeof(VAL) * ary->capacity);
    }
    while(index >= ary->items_length) {
        ary->items[ary->items_length++] = js_value_undefined();
    }
    ary->items[index] = val;
}

static bool is_string_integer(js_string_t* str)
{
    uint32_t i;
    for(i = 0; i < str->length; i++) {
        if(str->buff[i] < '0' || str->buff[i] > '9') {
            // not an integer
            return false;
        }
    }
    return true;
}

static VAL array_vtable_get(js_value_t* obj, js_string_t* prop)
{
    uint32_t index;
    js_array_t* ary = (js_array_t*)obj;
    if(is_string_integer(prop)) {
        index = atoi(prop->buff);
        if(index < ary->items_length) {
            return ary->items[index];
        }
        if(index >= ary->items_length && index < ary->length) {
            // sparse array
            return js_value_undefined();
        }
    }
    return js_object_base_vtable()->get(obj, prop);
}

static void array_vtable_put(js_value_t* obj, js_string_t* prop, VAL val)
{
    uint32_t index;
    js_array_t* ary = (js_array_t*)obj;
    if(is_string_integer(prop)) {
        index = atoi(prop->buff);
        array_put(ary, index, val);
    } else {
        js_object_base_vtable()->put(obj, prop, val);
    }
}

static bool array_vtable_has_property(js_value_t* obj, js_string_t* prop)
{
    uint32_t index;
    js_array_t* ary = (js_array_t*)obj;
    if(is_string_integer(prop)) {
        index = atoi(prop->buff);
        return index <= ary->items_length;
    }
    return js_object_base_vtable()->has_property(obj, prop);
}

static VAL Array_call(js_vm_t* vm, void* state, VAL this, uint32_t argc, VAL* argv)
{
    js_array_t* ary;
    uint32_t ary_length;
    if(argc == 1) {
        ary_length = js_to_uint32(argv[0]);
        ary = (js_array_t*)js_value_get_pointer(js_make_array(vm, ary_length, NULL));
        ary->length = ary_length;
        return js_value_make_pointer((js_value_t*)ary);
    } else {
        return js_make_array(vm, argc, argv);
    }
}

static VAL Array_prototype_push(js_vm_t* vm, void* state, VAL this, uint32_t argc, VAL* argv)
{
    if(js_value_get_type(this) != JS_T_ARRAY) {
        js_throw_error(vm->lib.TypeError, "Array.prototype.push is not generic");
    }
    js_array_t* ary = (js_array_t*)js_value_get_pointer(this);
    uint32_t i;
    for(i = 0; i < argc; i++) {
        array_put(ary, ary->length, argv[i]);
    }
    return js_value_make_double(ary->length);
}

void js_lib_array_initialize(js_vm_t* vm)
{
    if(!statically_initialized) {
        statically_initialized = true;
        memcpy(&array_vtable, js_object_base_vtable(), sizeof(js_object_internal_methods_t));
        array_vtable.get = array_vtable_get;
        array_vtable.put = array_vtable_put;
        array_vtable.has_property = array_vtable_has_property;
    }
    
    vm->lib.Array = js_value_make_native_function(vm, NULL, js_cstring("Array"), Array_call, Array_call);
    vm->lib.Array_prototype = js_object_get(vm->lib.Array, js_cstring("prototype"));
    js_object_put(vm->global_scope->global_object, js_cstring("Array"), vm->lib.Array);
    js_object_put(vm->lib.Array_prototype, js_cstring("push"), js_value_make_native_function(vm, NULL, js_cstring("push"), Array_prototype_push, NULL));
}
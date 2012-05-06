#include <gc.h>
#include <vm.h>
#include <value.h>
#include <exception.h>
#include <stdarg.h>
#include "multiboot.h"
#include "mm.h"
#include "console.h"
#include "panic.h"
#include "gdt.h"
#include "interrupt.h"
#include "io.h"
#include "lib.h"

bool x86_64_support();
uint32_t rdtsc();

void load_modules(VAL object, multiboot_module_t* modules, uint32_t count)
{
    kprintf("Loading %d modules:\n", count);
    uint32_t i;
    for(i = 0; i < count; i++) {
        kprintf("  %s\n", modules[i].cmdline);
        js_string_t* name = js_cstring((char*)modules[i].cmdline);
        uint32_t size = modules[i].mod_end - modules[i].mod_start;
        VAL data = js_value_make_string((char*)modules[i].mod_start, size);
        js_object_put(object, name, data);
    }
}

static void unhandled_exception(VAL exception)
{
    if(js_value_is_object(exception) && js_value_get_pointer(exception)->object.stack_trace) {
        panicf("Unhandled exception: %s\n%s",
            js_value_get_pointer(js_to_string(exception))->string.buff,
            js_value_get_pointer(exception)->object.stack_trace->buff);
    } else {
        panicf("Unhandled exception: %s\n", js_value_get_pointer(js_to_string(exception))->string.buff);
    }
}

void kmain_(struct multiboot_info* mbd, uint32_t magic)
{
    console_clear();
    
    kprintf("JSOS\n");
    uint32_t highest_module = 0;
    multiboot_module_t* mods = (multiboot_module_t*)mbd->mods_addr;
    uint32_t i;
    for(i = 0; i < mbd->mods_count; i++) {
        if(mods[i].mod_end > highest_module) {
            highest_module = mods[i].mod_end;
        }
    }
    
    mm_init((multiboot_memory_map_t*)mbd->mmap_addr, mbd->mmap_length, highest_module);
    gdt_init();
    
    js_vm_t* vm = js_vm_new();
    js_lib_math_seed_random(rdtsc());
    js_set_panic_handler(js_panic_handler);
    
    console_init(vm);
    lib_kernel_init(vm);
    lib_binary_utils_init(vm);
    lib_vm_init(vm);
    lib_buffer_init(vm);
    idt_init(vm);
    io_init(vm);
    
    VAL Kernel = js_object_get(vm->global_scope->global_object, js_cstring("Kernel"));
    
    js_object_put(Kernel, js_cstring("bootDevice"), js_value_make_double(mbd->boot_device >> 24));
    
    VAL modules = js_make_object(vm);
    js_object_put(Kernel, js_cstring("modules"), modules);
    load_modules(modules, (multiboot_module_t*)mbd->mods_addr, mbd->mods_count);
    
    VAL vinit = js_object_get(modules, js_cstring("/kernel/init.jmg"));
    if(js_value_get_type(vinit) != JS_T_STRING) {
        panic("could not load /kernel/init.jmg");
    }
    js_value_t* sinit = js_value_get_pointer(vinit);
    js_image_t* image = js_image_parse(sinit->string.buff, sinit->string.length);
    if(!image) {
        panic("/kernel/init.jmg is an invalid image file");
    }
    
    kprintf("Handing over control to JavaScript:\n\n");
    
    VAL exception;
    JS_TRY({
        js_vm_exec(vm, image, 0, vm->global_scope, vm->global_scope->global_object, 0, NULL);
        for(;;) {
            interrupt_dispatch_events();
            __asm__ volatile("hlt");
        }
    }, exception, {
        unhandled_exception(exception);
    });
}

extern int kstack_max;

void kmain(struct multiboot_info* mbd, uint32_t magic)
{    
    if(magic != MULTIBOOT_BOOTLOADER_MAGIC)         panic("multiboot did not pass correct magic number");
    if(!(mbd->flags & MULTIBOOT_INFO_MEMORY))       panic("multiboot did not pass memory information");
    if(!(mbd->flags & MULTIBOOT_INFO_MEM_MAP))      panic("multiboot did not pass memory map");
    if(!(mbd->flags & MULTIBOOT_INFO_BOOTDEV))      panic("multiboot did not pass boot device");
    
    int dummy;
    js_gc_init(&dummy);
    js_vm_set_stack_limit(&kstack_max);
    
    kmain_(mbd, magic);
}


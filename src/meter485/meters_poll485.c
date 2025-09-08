#include "meters_poll485.h"

LOG_MODULE_DECLARE(meters, CONFIG_STRIM_METERS_LOG_LEVEL);

static K_THREAD_STACK_DEFINE(meters_basestack, CONFIG_STRIM_METERS_MAIN_STACK_SIZE);

static void meters_poll_bus485_Thread(void *args0, void *args1, void *args2){
    meters_context_t *context = (meters_context_t*)args0;
    (void)args1;
    (void)args2;

    int32_t ret = 0;
    //bool isInitialize = false;

    while(true){

        //meters data call
        #ifdef CONFIG_STRIM_METERS_BUS485_ENABLE
        for(uint32_t i = 0; i < context->item_count; i++){
            //TODO! set periodic call meters data
            meters_read_t read_func = meters_get_read_func(context->parameters[i].type);
            if (read_func != NULL) {
                ret = read_func(context, i);
                if (ret != 0)
                {
                    LOG_ERR("read meter %d error: %d", i, ret);
                    goto exit_poll485_thread;
                }
            }
        }
        k_sleep(K_MSEC(1000));
        #endif

    }

    exit_poll485_thread:
    //LOG_ERR("thread %s stopped\r\n", log_strdup(k_thread_name_get(k_current_get())));
}

void meters_poll485_thread_run(meters_context_t *context){

    context->poll485_stack = meters_basestack;
    context->poll485_stack_size = K_THREAD_STACK_SIZEOF(meters_basestack);

    k_thread_create(&context->poll485_thread, context->poll485_stack, context->poll485_stack_size,
                    meters_poll_bus485_Thread, context, NULL, NULL,
                    CONFIG_STRIM_METERS_INIT_PRIORITY, 0, K_NO_WAIT);
    
    k_thread_name_set(&context->poll485_thread, "meters_bus485");
}
#include "meters_call.h"

LOG_MODULE_REGISTER(meters, CONFIG_STRIM_METERS_LOG_LEVEL);

static K_THREAD_STACK_DEFINE(meters_basestack, CONFIG_STRIM_METERS_MAIN_STACK_SIZE);

static void meters_baseThread(void *args0, void *args1, void *args2){
    meters_context_t *context = (meters_context_t*)args0;
    (void)args1;
    (void)args2;

    int32_t ret = 0;
    //bool isInitialize = false;

    while(true){
        /*ret = k_sem_take(&context->reinitSem, K_MSEC(250));
        if (ret == 0)
            isInitialize = false;
 
        if(!isInitialize){
            ret = meters_initialize_context(context);
            if(ret < 0)
                goto exitBaseThread;
            isInitialize = true;
        }*/
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
                goto exitBaseThread;
                }
            }
        }
        k_sleep(K_MSEC(1000));
        #endif

    }

    exitBaseThread:
    LOG_ERR("thread %s stopped\r\n", log_strdup(k_thread_name_get(k_current_get())));
}

void meters_call_thread_run(meters_context_t *context){

    context->baseStack = meters_basestack;
    context->base_stack_size = K_THREAD_STACK_SIZEOF(meters_basestack);

    k_thread_create(&context->baseThread, context->baseStack, context->base_stack_size,
                    meters_baseThread, context, NULL, NULL,
                    CONFIG_STRIM_METERS_INIT_PRIORITY, 0, K_NO_WAIT);
    
    k_thread_name_set(&context->baseThread, "meters_bus485");
}
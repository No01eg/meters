#include "meters_private.h"
LOG_MODULE_REGISTER(meters, CONFIG_STRIM_METERS_LOG_LEVEL);

static K_THREAD_STACK_DEFINE(Meters_BaseStack, CONFIG_STRIM_METERS_MAIN_STACK_SIZE);

static void meters_baseThread(void *args0, void *args1, void *args2);

meters_context_t metersContext = {
    .baseStack = Meters_BaseStack,
    .baseStackSize = K_THREAD_STACK_SIZEOF(Meters_BaseStack),
};

int32_t meters_init(meters_get_parameters_t cb, void *user_data){
    meters_context_t * context = &metersContext;
    int32_t ret;
    
    if(cb == NULL)
        return -EINVAL;

    k_mutex_init(&context->dataAccessMutex);

    #ifdef CONFIG_STRIM_METERS_BUS485_ENABLE
    //TODO! init meters 
    #endif

    (void)ret;

    k_thread_create(&context->baseThread, context->baseStack, context->baseStackSize,
                    meters_baseThread, context, NULL, NULL,
                    CONFIG_STRIM_METERS_INIT_PRIORITY, 0, K_NO_WAIT);
    
    k_thread_name_set(&context->baseThread, "meters");
    return 0;
}
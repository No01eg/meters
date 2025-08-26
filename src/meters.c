#include "meters_private.h"
#include <stdbool.h>

LOG_MODULE_REGISTER(meters, CONFIG_STRIM_METERS_LOG_LEVEL);

static K_THREAD_STACK_DEFINE(Meters_BaseStack, CONFIG_STRIM_METERS_MAIN_STACK_SIZE);

static void meters_baseThread(void *args0, void *args1, void *args2);
static int32_t initializeMetersContext(meters_context_t *context);

meters_context_t metersContext = {
    .baseStack = Meters_BaseStack,
    .baseStackSize = K_THREAD_STACK_SIZEOF(Meters_BaseStack),
};

typedef struct {
    const char* name;
    meters_type_t valuesType;
    meters_init_t init;
    meters_read_t read;
}meters_typeDescription_t;

static const meters_typeDescription_t meters_typeDescription[meters_type_lastIndex] = {
    [meters_type_externDC] = {.name = "EXTERNAL.DC",
                            .valuesType = meters_currentType_DC,
                            .init = NULL,
                            .read = NULL},
    [meters_type_externAC] = {.name = "EXTERNAL.AC",
                            .valuesType = meters_currentType_AC,
                            .init = NULL,
                            .read = NULL},
};

int32_t meters_init(meters_get_parameters_t cb, void *user_data){
    meters_context_t * context = &metersContext;
    int32_t ret;
    
    if(cb == NULL)
        return -EINVAL;

    k_mutex_init(&context->dataAccessMutex);
    k_sem_init(&context->reinitSem, 0 ,1);

    context->getParameters = cb;
    context->user_data = user_data;

    #ifdef CONFIG_STRIM_METERS_BUS485_ENABLE
    //TODO! init meters with bus485
    #endif

    (void)ret;

    k_thread_create(&context->baseThread, context->baseStack, context->baseStackSize,
                    meters_baseThread, context, NULL, NULL,
                    CONFIG_STRIM_METERS_INIT_PRIORITY, 0, K_NO_WAIT);
    
    k_thread_name_set(&context->baseThread, "meters");
    return 0;
}

int32_t meters_reinit(void){
    meters_context_t *context = &metersContext;
    k_sem_give(&context->reinitSem);
    return 0;
}

int32_t meters_set_values(uint32_t idx, const meters_values_t *buffer){
    meters_context_t *context = &metersContext;

    if(buffer == NULL)
        return -EINVAL;
    
    if(idx >= context->itemCount)
        return -ERANGE;
    
    k_mutex_lock(&context->dataAccessMutex, K_FOREVER);
    {
        memcpy(&context->items[idx].values, buffer, sizeof(meters_values_t));
        context->items[idx].timemark = k_uptime_get();
        context->items[idx].isValidValues = true;
    }
    k_mutex_unlock(&context->dataAccessMutex);
    return 0;
}

int32_t meters_get_values(uint32_t idx, meters_values_t *buffer){
    meters_context_t *context = &metersContext;
    
    if(buffer == NULL)
        return -EINVAL;
    
    if(idx >= context->itemCount)
        return -ERANGE;

    int32_t ret = -ENXIO;

    k_mutex_lock(&context->dataAccessMutex, K_FOREVER);
    {
        uint64_t curTimemark = k_uptime_get();
        if((curTimemark - context->items[idx].timemark) > (CONFIG_STRIM_METERS_VALID_DATA_TIMEOUT))
            context->items[idx].isValidValues = false;
        if(context->items[idx].isValidValues){
            memcpy(buffer, &context->items[idx].values, sizeof(meters_values_t));
            ret = 0;
        }
    }
    k_mutex_unlock(&context->dataAccessMutex);

    return ret;
}

const uint8_t * meters_get_typename(meters_type_t type){
    if(type < meters_type_lastIndex)
        return meters_typeDescription[type].name;
    
    return "unknown";
}

int32_t meters_get_all(meters_values_collection_t *buffer){
    meters_context_t *context = &metersContext;

    if(buffer == NULL)
        return -EINVAL;
    buffer->count = 0;

    k_mutex_lock(&context->dataAccessMutex, K_FOREVER);
    {
        for(uint32_t i = 0; i < context->itemCount; i++){
            if((i < ARRAY_SIZE(context->items)) &&
                (i < ARRAY_SIZE(context->parameters)) && 
                (i < ARRAY_SIZE(buffer->items))){
                    buffer->items[i].isValid = context->items[i].isValidValues;
                    memcpy(&buffer->items[i].values, &context->items[i].values, sizeof(meters_values_t));
                    memcpy(&buffer->items[i].parameters, &context->parameters[i], sizeof(meter_parameters_t));
                    buffer->count++;
            }
        }
    }
    k_mutex_unlock(&context->dataAccessMutex);

    return 0;
}


static void meters_baseThread(void *args0, void *args1, void *args2){
    meters_context_t *context = (meters_context_t*)args0;
    (void)args1;
    (void)args2;

    int32_t ret = 0;
    bool isInitialize = false;

    while(true){
        ret = k_sem_take(&context->reinitSem, K_MSEC(250));
        if (ret == 0)
            isInitialize = false;
 
        if(!isInitialize){
            ret = initializeMetersContext(context);
            if(ret < 0)
                goto exitBaseThread;
            isInitialize = true;
        }

        //meters data call
        #ifdef CONFIG_STRIM_METERS_BUS485_ENABLE
        for(uint32_t i = 0; i < context->itemsCount; i++){
            //TODO! set periodic call meters data

            k_sleep_(K_MSEC(1000));
        }
        #endif

    }

    exitBaseThread:
    LOG_ERR("thread %s stopped\r\n", log_strdup(k_thread_name_get(k_current_get())));
}

static int32_t initializeMetersContext(meters_context_t *context){
    int32_t ret;
    //default properties
    for(uint32_t i = 0; i < CONFIG_STRIM_METERS_ITEMS_MAX_COUNT; i++){
        context->parameters[i].currentFactor = 1;
        context->parameters[i].address = 0;
        context->parameters[i].type = meters_type_lastIndex;
    }

    ret = context->getParameters(context->parameters, 
                                CONFIG_STRIM_METERS_ITEMS_MAX_COUNT,
                                context->user_data);
    if(ret < 0){
        LOG_ERR("get parameters error: %d\r\n", ret);
        return ret;
    }

    context->itemCount = ret;

    for(uint32_t i = 0; i < context->itemCount; i++){
        meters_type_t type = context->parameters[i].type;
        if(type >= meters_type_lastIndex){
            LOG_ERR("meter %u have unknown type %u\r\n", i, context->parameters[i].type);
            context->itemCount = 0;
            return -1; //TODO set error type
        }
    }
    return 0;
}
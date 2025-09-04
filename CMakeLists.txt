if(CONFIG_STRIM_METERS)
    list(APPEND ZEPHYR_EXTRA_MODULES "${CMAKE_SOURCE_DIR}/../Bus485")

    zephyr_include_directories(src)
    
    zephyr_library()

    zephyr_library_sources_ifdef(CONFIG_STRIM_METERS src/meters.c)
    zephyr_library_sources_ifdef(CONFIG_STRIM_METERS_BUS485_ENABLE src/meters_spm90.c)
    zephyr_library_sources_ifdef(CONFIG_STRIM_METERS_SHELL src/meters_shell.c)
endif()
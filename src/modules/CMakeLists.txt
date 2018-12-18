add_library (veloc-modules SHARED 
  module_manager.cpp
  compression_module.cpp
  client_watchdog.cpp transfer_module.cpp
  client_aggregator.cpp ec_module.cpp
  ${VELOC_SOURCE_DIR}/src/common/config.cpp
)
target_link_libraries(veloc-modules ${ER_LIBRARIES} ${AXL_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

add_library (compression-modules SHARED compression_module.cpp)
target_link_libraries(compression-modules ${SZ_DIR} ${ZSTD_DIR} ${ZLIB_DIR})

# Install libraries
install (TARGETS veloc-modules
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

add_library (veloc-client SHARED 
  veloc.cpp
  client.cpp
  ${VELOC_SOURCE_DIR}/src/common/config.cpp
)

# Link the executable to the necessary libraries.
target_link_libraries (veloc-client veloc-modules ${ER_LIBRARIES} ${MPI_CXX_LIBRARIES} rt)

# Install libraries
install (TARGETS veloc-client
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

install (FILES
  ${VELOC_SOURCE_DIR}/include/veloc.h
  DESTINATION include
)

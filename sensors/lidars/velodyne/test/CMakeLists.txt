SET( KIT velodyne )

FILE( GLOB source *test.cpp )
FILE( GLOB extras *.cpp
    *.h )
LIST( REMOVE_ITEM extras ${source} )

ADD_EXECUTABLE( ${CMAKE_PROJECT_NAME}_test_${KIT} ${source} ${extras} )
TARGET_LINK_LIBRARIES( ${CMAKE_PROJECT_NAME}_test_${KIT}
    snark_velodyne
    ${snark_ALL_EXTERNAL_LIBRARIES}
    ${GTEST_BOTH_LIBRARIES}
    )
INSTALL (
    FILES ${PROJECT_BINARY_DIR}/bin/${CMAKE_PROJECT_NAME}_test_${KIT}
    PERMISSIONS WORLD_READ GROUP_READ OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_EXECUTE WORLD_EXECUTE
    DESTINATION ${snark_CPP_TESTS_INSTALL_DIR} )

add_test( NAME ${CMAKE_PROJECT_NAME}_test_${KIT} COMMAND ${CMAKE_PROJECT_NAME}_test_${KIT} WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/bin )

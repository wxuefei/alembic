##-*****************************************************************************
##
## Copyright (c) 2023-2024, PASSION PAINT ANIMATION 
##
##-*****************************************************************************

ADD_EXECUTABLE(abcjoin AbcJoin.cpp util.cpp)
TARGET_LINK_LIBRARIES(abcjoin Alembic::Alembic)
#e:/Projects/maya/Imath/bin/bin/Debug_lib/Imath-3_2_d.lib

set_target_properties(abcjoin PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH TRUE
    INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

INSTALL(TARGETS abcstitcher DESTINATION bin)

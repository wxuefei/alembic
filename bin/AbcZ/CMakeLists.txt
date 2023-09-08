##-*****************************************************************************
##
## Copyright(c) 2023-2024, PASSION PAINT ANIMATION
##
##-*****************************************************************************

ADD_EXECUTABLE(abcz AbcZ.cpp Main.cpp Float16.cpp)
TARGET_LINK_LIBRARIES(abcz Alembic::Alembic 
    debug ../../../bin/AbcZ/lib7z/lib/lib7zr_debug
    optimized ../../../bin/AbcZlib7z/lib/lib7zr_release)
INCLUDE_DIRECTORIES(lib7z/include)

set_target_properties(abcz PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH TRUE
    INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)

INSTALL(TARGETS abcz DESTINATION bin)

# Builds the Lua 5.3 core and auxiliary library (no interpreter, no compiler) as a static C library. Headers install to
# include/lua53 so they cannot collide with another Lua.
cmake_minimum_required(VERSION 3.20)
project(lua53 C)

set(CMAKE_C_STANDARD 99)

if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()
if(UNIX)
    add_compile_definitions(LUA_USE_POSIX)
endif()

file(GLOB lua_sources "${PROJECT_SOURCE_DIR}/src/*.c")
list(REMOVE_ITEM lua_sources "${PROJECT_SOURCE_DIR}/src/lua.c" "${PROJECT_SOURCE_DIR}/src/luac.c")

add_library(lua53 STATIC ${lua_sources})
target_include_directories(lua53 PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
    $<INSTALL_INTERFACE:include>
)
if(UNIX)
    find_library(lua53_libm NAMES m)
    if(lua53_libm)
        target_link_libraries(lua53 PRIVATE m)
    endif()
endif()

install(FILES src/lua.h src/luaconf.h src/lualib.h src/lauxlib.h src/lua.hpp DESTINATION include/lua53)
install(TARGETS lua53 EXPORT lua53Targets ARCHIVE DESTINATION lib)
install(EXPORT lua53Targets NAMESPACE lua53:: FILE lua53Config.cmake DESTINATION share/lua53)

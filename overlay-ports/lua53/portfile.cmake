# Stock Lua 5.3.6, unpatched. Warcraft III embeds Lua 5.3.4; 5.3.6 has the same grammar. Named lua53 so it never resolves to the
# baseline's "lua" port (5.5).
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_download_distfile(ARCHIVE
    URLS "https://www.lua.org/ftp/lua-5.3.6.tar.gz"
    FILENAME "lua-5.3.6.tar.gz"
    SHA512 ccc380d5e114d54504de0bfb0321ca25ec325d6ff1bfee44b11870b660762d1a9bf120490c027a0088128b58bb6b5271bbc648400cab84d2dc22b512c4841681
)
vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE "${ARCHIVE}")
file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(PACKAGE_NAME lua53 CONFIG_PATH share/lua53)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(READ "${SOURCE_PATH}/src/lua.h" lua_header)
# The MIT license text is the comment block at the end of lua.h.
string(FIND "${lua_header}" "* Copyright (C) 1994" license_start)
string(SUBSTRING "${lua_header}" ${license_start} -1 license)
file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/copyright" "${license}")

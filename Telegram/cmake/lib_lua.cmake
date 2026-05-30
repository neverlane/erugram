# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

add_library(lib_lua STATIC)
init_target(lib_lua "(external)")

add_library(desktop-app::lib_lua ALIAS lib_lua)

set(lua_loc ${third_party_loc}/lua)
set(sol2_loc ${third_party_loc}/sol2/include)

nice_target_sources(lib_lua ${lua_loc}
PRIVATE
    lapi.c
    lauxlib.c
    lbaselib.c
    lcode.c
    lcorolib.c
    lctype.c
    ldblib.c
    ldebug.c
    ldo.c
    ldump.c
    lfunc.c
    lgc.c
    linit.c
    liolib.c
    llex.c
    lmathlib.c
    lmem.c
    loadlib.c
    lobject.c
    lopcodes.c
    loslib.c
    lparser.c
    lstate.c
    lstring.c
    lstrlib.c
    ltable.c
    ltablib.c
    ltm.c
    lundump.c
    lutf8lib.c
    lvm.c
    lzio.c
)

if (LINUX)
    target_compile_definitions(lib_lua PRIVATE LUA_USE_LINUX)
    target_link_libraries(lib_lua PRIVATE ${CMAKE_DL_LIBS})
elseif (APPLE)
    target_compile_definitions(lib_lua PRIVATE LUA_USE_MACOSX)
endif()

target_include_directories(lib_lua
PUBLIC
    ${lua_loc}
    ${sol2_loc}
)

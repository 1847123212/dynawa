#include "lua.h"
#include "lauxlib.h"
#include "debug/trace.h"

static int l_peek (lua_State *L) {
    TRACE_LUA("l_peek\r\n");

    uint32_t addr = (uint32_t)luaL_checkint(L, 1);

    uint8_t *memory = (uint8_t *)0;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TUSERDATA);
        memory = (uint8_t *)lua_touserdata(L, 2);
    }
    lua_pushnumber(L, memory[addr]);
    return 1;
}

static int l_ticks (lua_State *L) {
    TRACE_LUA("l_ticks\r\n");

    lua_pushnumber(L, xTaskGetTickCount());
    return 1;
}

static const struct luaL_reg dynawa [] = {
    {"peek", l_peek},
    {"ticks", l_ticks},
    {NULL, NULL}  /* sentinel */
};


int dynawa_timer_register(lua_State *L);
int dynawa_bitmap_register(lua_State *L);
int dynawa_file_register(lua_State *L);
int dynawa_time_register(lua_State *L);
int dynawa_bt_register(lua_State *L);

struct {
    char *module;
    int (*module_register)(lua_State *L);
} modules[] = 
{
    {"bitmap", dynawa_bitmap_register},
    {"timer", dynawa_timer_register},
    {"file", dynawa_file_register},
    {"time", dynawa_time_register},
    {"bt", dynawa_bt_register},
    {NULL, NULL}  /* sentinel */
};

int luaopen_dynawa (lua_State *L) {
    luaL_register(L, "dynawa", dynawa);

    int i = 0;
    while(modules[i].module) {
        lua_pushstring(L, modules[i].module);
        lua_newtable(L);
        modules[i].module_register(L);
        lua_settable(L, -3);
        i++;
    }

    return 1;
}

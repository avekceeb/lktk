
#include "lktkassert.h"

const char * nomsg = "mute";

static void addFailure(lua_State *L) {
    (kit.failures)++;
}

// TODO: all the asserts are almost same code
static int assertTrue(lua_State *L) {
    if (kit.strict < 0) {
        return 0;
    }
    const char * msg = nomsg;
    int n = lua_gettop(L);
    if (1 > n) {
        log_error("Wrong arguments to assert");
        return 0;
    }
    if (n > 1) {
        if (LUA_TSTRING == lua_type(L,2)) {
            msg = lua_tostring(L, 2);
        }
    }
    if (!lua_toboolean(L, 1)) {
        log_error("Assert True Failed [%s]", msg);
        addFailure(L);
        if (kit.strict) {
            luaL_error(L, "Assert True Failed");
        }
    } else {
        log_info("Assert pass [%s]", msg);
    }
    return 0;
}

static int assertEquals(lua_State *L) {
    if (kit.strict < 0) {
        return 0;
    }
    const char * msg = nomsg;
    int n = lua_gettop(L);
    if (n < 2) {
        log_error("Wrong arguments to assert");
        return 0;
    }
    // TODO: check types
    long actual = lua_tointeger(L, 1);
    long expected = lua_tointeger(L, 2);
    if (n > 2) {
        if (LUA_TSTRING == lua_type(L,3)) {
            msg = lua_tostring(L, 3);
        }
    }
    if (actual != expected) {
        log_error("Assert Equals Failed [%s] expected: %ld actual: %ld",
                msg, expected, actual);
        addFailure(L);
        if (kit.strict) {
            luaL_error(L, "Assert Equals Failed");
        }
    } else {
        log_info("Assert pass [%s]", msg);
    }
    return 0;
}

static int assertGreater(lua_State *L) {
    if (kit.strict < 0) {
        return 0;
    }
    const char * msg = nomsg;
    int n = lua_gettop(L);
    if (n < 2) {
        log_error("Wrong arguments to assert");
        return 0;
    }
    // TODO: check types
    long actual = lua_tointeger(L, 1);
    long expected = lua_tointeger(L, 2);
    if (n > 2) {
        if (LUA_TSTRING == lua_type(L,3)) {
            msg = lua_tostring(L, 3);
        }
    }
    if (actual <= expected) {
        log_error("Assert Greater Failed [%s] expected: >%ld actual: %ld",
                msg, expected, actual);
        addFailure(L);
        if (kit.strict) {
            luaL_error(L, "Assert Greater Failed");
        }
    } else {
        log_info("Assert pass [%s]", msg);
    }
    return 0;
}

static int assertGreaterOrEquals(lua_State *L) {
    if (kit.strict < 0) {
        return 0;
    }
    const char * msg = nomsg;
    int n = lua_gettop(L);
    if (n < 2) {
        log_error("Wrong arguments to assert");
        return 0;
    }
    // TODO: check types
    long actual = lua_tointeger(L, 1);
    long expected = lua_tointeger(L, 2);
    if (n > 2) {
        if (LUA_TSTRING == lua_type(L,3)) {
            msg = lua_tostring(L, 3);
        }
    }
    if (actual < expected) {
        log_error("Assert GreaterOrEquals Failed [%s] expected: >=%ld actual: %ld",
                    msg, expected, actual);
        addFailure(L);
        if (kit.strict) {
            luaL_error(L, "Assert GreaterOrEquals Failed");
        }
    } else {
        log_info("Assert pass [%s]", msg);
    }
    return 0;
}
const struct luaL_Reg lktkassert_globals[] = {
    {"assert_true", assertTrue},
    {"assert_eq", assertEquals},
    {"assert_gt", assertGreater},
    {"assert_ge", assertGreaterOrEquals},
    {NULL, NULL}
};

void inject_lktkassert(lua_State* L) {
    lua_pushglobaltable(L);
    luaL_setfuncs(L, lktkassert_globals, 0);
    lua_pop(L, 1);
}




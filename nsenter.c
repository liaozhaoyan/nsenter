//
// Created by 廖肇燕 on 2023/9/27.
//

#include <lua.h>
#include <lauxlib.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <linux/sched.h>

#define register_constant(L, s)\
    lua_pushinteger(L, s);\
    lua_setfield(L, -2, #s);

#define MT_NAME "NSENTER_HANDLE"

static int nsenter(lua_State *L) {
    char path[PATH_MAX];
    int ret;
    pid_t self = getpid();

    int *priv = (int *)luaL_checkudata(L, 1, MT_NAME);
    pid_t pid  = luaL_checkint(L, 2);
    const char *ns = luaL_checkstring(L, 3);
    int fd, fd_self;

    if (*(priv) != 0) {
        luaL_error(L, "current namespace is not exit.");
    }

    snprintf(path, PATH_MAX, "/proc/%d/ns/%s", pid, ns);
    fd = open(path, O_RDONLY);
    if (fd == -1) {
        ret = errno;
        luaL_error(L, "pid %d fd open failed, errno:%d, %s\n", pid,
                errno, strerror(errno));
        goto endOpen;
    }

    snprintf(path, PATH_MAX, "/proc/%d/ns/%s", self, ns);
    fd_self = open(path, O_RDONLY);
    if (fd == -1) {
        ret = errno;
        luaL_error(L, "self pid fd open failed, errno:%d, %s\n",
                errno, strerror(errno));
        goto endOpenself;
    }

    ret = setns(fd, 0);
    if (ret < 0) {
        goto endSetns;
    }

    *(priv + 0) = fd;
    *(priv + 1) = fd_self;
    lua_pushnumber(L, ret);
    return 1;

    endSetns:
    close(fd_self);
    endOpenself:
    close(fd);
    endOpen:
    lua_pushnumber(L, ret);
    return 1;
}

static int nsexit(lua_State *L) {
    int *priv = (int *)luaL_checkudata(L, 1, MT_NAME);
    int fd, fd_self;

    luaL_argcheck(L, priv != NULL, 1, "`array' expected");
    fd = *(priv);
    if (fd > 0) {
        close(fd);
    }

    fd_self = *(priv + 1);
    if (fd_self > 0) {
        setns(fd_self, 0);
        close(fd_self);
    }

    *(priv) = 0;
    *(priv + 1) = 0;
    return 0;
}

static int new(lua_State *L) {
    size_t nbytes = sizeof(int) * 2;

    int *priv = (int *)lua_newuserdata(L, nbytes);
    *(priv ++) = 0;
    *(priv ++) = 0;

    luaL_getmetatable(L, MT_NAME);
    lua_setmetatable(L, -2);
    return 1;
}


static luaL_Reg module_m[] = {
        {"enter", nsenter},
        {"exit", nsexit},
        {NULL, NULL}
};

static luaL_Reg module_f[] = {
        {"new", new},
        {NULL, NULL}
};

int luaopen_nsenter(lua_State *L) {
    luaL_newmetatable(L, MT_NAME);

    lua_createtable(L, 0, sizeof(module_m) / sizeof(luaL_Reg) - 1);
#if LUA_VERSION_NUM > 501
    luaL_setfuncs(L, module_m, 0);
#else
    luaL_register(L, NULL, module_m);
#endif
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, nsexit);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

#if LUA_VERSION_NUM > 501
    luaL_newlib(L, module_f);
#else
    luaL_register(L, "nsenter", module_f);
#endif
    return 1;
}
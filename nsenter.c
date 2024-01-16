//
// Created by 廖肇燕 on 2023/9/27.
//

#include <lua.h>
#include <lauxlib.h>

#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sched.h>

#define MT_NAME "NSENTER_HANDLE"

#define NS_NUM  8
struct ns_manager {
    int count;
    int host_fd[NS_NUM];
    int ns_fd[NS_NUM];
};

static void escape_ch(struct ns_manager *priv, int fdNs, int fdSelf ) {
    int i;
    for (i = 0; i < fdNs; i ++) {
        close(priv->ns_fd[i]);
    }

    for (i = 0; i < fdSelf; i ++) {
        close(priv->host_fd[i]);
    }
}

static int nsenter(lua_State *L) {
    int ret;
    int i;
    int top = lua_gettop(L);

    if (top <= 2 || top > 2 + NS_NUM) {
        luaL_error(L, "entry arg failed, try ns:nsenter(pid, 'mnt', 'pid')");
    }

    struct ns_manager *priv = (struct ns_manager *)luaL_checkudata(L, 1, MT_NAME);
    luaL_argcheck(L, priv != NULL, 1, "bad self.");
    if (priv->count != 0) {
        luaL_error(L, "current namespace is not exit.");
    }
    priv->count = top - 2;

    pid_t pid  = luaL_checkint(L, 2);
    for (i = 0; i < priv->count; i ++) {
        int fd, fd_self;
        char path[PATH_MAX];
        const char *ns = luaL_checkstring(L, i + 3);

        snprintf(path, PATH_MAX, "/proc/%d/ns/%s", pid, ns);
        fd = open(path, O_RDONLY);
        if (fd == -1) {
            ret = errno;
            escape_ch(priv, i, i);
            luaL_error(L, "pid %d fd open failed, errno:%d, %s\n", pid,
                       errno, strerror(errno));
        }
        priv->ns_fd[i] = fd;

        snprintf(path, PATH_MAX, "/proc/self/ns/%s", ns);
        fd_self = open(path, O_RDONLY);
        if (fd == -1) {
            ret = errno;
            escape_ch(priv, i + 1, i);
            luaL_error(L, "self pid fd open failed, errno:%d, %s\n",
                       errno, strerror(errno));
        }
        priv->host_fd[i] = fd_self;
    }

    for (i = 0; i < priv->count; i ++) {
        ret = setns(priv->ns_fd[i], 0);
        if (ret < 0) {
            int j;
            for (j = i; j < priv->count; j ++) {
                close(priv->ns_fd[j]);
            }
            escape_ch(priv, 0, priv->count);
            luaL_error(L, "set ns failed, errno:%d, %s\n",
                       errno, strerror(errno));
        }

        close(priv->ns_fd[i]);  // if setns success, the fd can closed.
        priv->ns_fd[i] = 0;
    }
    lua_pushnumber(L, ret);
    return 1;
}

static int nsexit(lua_State *L) {
    int i, fd, host_fd;
    struct ns_manager *priv = (struct ns_manager *)luaL_checkudata(L, 1, MT_NAME);
    luaL_argcheck(L, priv != NULL, 1, "bad self.");

    for (i = 0; i < priv->count; i ++) {
        host_fd = priv->host_fd[i];
        if (host_fd > 0) {
            setns(host_fd, 0);
            close(host_fd);
        }
    }
    memset(priv, 0, sizeof(struct ns_manager));
    return 0;
}

static int l_setns(lua_State *L) {
    int fd, ret;
    const char *path;
    struct ns_manager *priv = (struct ns_manager *)luaL_checkudata(L, 1, MT_NAME);
    luaL_argcheck(L, priv != NULL, 1, "bad self.");

    path = luaL_checkstring(L, 2);
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        luaL_error(L, "%s open failed, errno:%d, %s\n", path,
                   errno, strerror(errno));
    }

    ret = setns(fd, 0);
    if (ret < 0) {
        close(fd);
        luaL_error(L, "set ns failed, errno:%d, %s\n",
                   errno, strerror(errno));
    }
    close(fd);
    return 0;
}

static int new(lua_State *L) {
    size_t nbytes = sizeof(struct ns_manager);

    struct ns_manager *priv = (struct ns_manager *)lua_newuserdata(L, nbytes);
    priv->count = 0;

    luaL_getmetatable(L, MT_NAME);
    lua_setmetatable(L, -2);
    return 1;
}


static luaL_Reg module_m[] = {
        {"enter", nsenter},
        {"exit", nsexit},
        {"setns", l_setns},
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
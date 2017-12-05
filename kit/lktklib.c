#include "lktklib.h"
#include <stdio.h>
#include <stdarg.h>

int isRoot(lua_State *L) {
    uid_t uid = getuid();
    if (0 == uid) {
		lua_pushboolean(L, 1);
        return 1;
    }
    uid = geteuid();
    if (0 == uid) {
		lua_pushboolean(L, 1);
        return 1;
    }
	lua_pushboolean(L, 0);
    return 1;
}

/*
 * Data structures with 'visible' fileds
 * i.e. wrappers for access each field by name
 * Other (not 'implemented') data types
 * could be provided as string.pack-ed blobs
 * (TODO: Could they?)
 */

//////// SHED_ATTR ///////////////////////

#include "sched/types.h"

// lua table --> struct *sched_attr
static void* unmarshall_sched_attr(lua_State *L, int idx) {
    if (lua_type(L, idx) != LUA_TTABLE) {
        log_error("sched_attr got no table!");
        return 0;
    }
	struct sched_attr *sa =
	        (struct sched_attr *)lua_newuserdata(L, sizeof(struct sched_attr));
    lua_setfield(L, idx, "__userdata");
    sa->size = sizeof(struct sched_attr);
    if (LUA_TNUMBER == lua_getfield(L, idx, "size")) {
        sa->size = (__u32)lua_tointeger(L, -1);
    }
    // TODO: what would be pushed in case of no such attr?
    lua_getfield(L, idx, "sched_policy");
    sa->sched_policy = (__u32)lua_tointeger(L, -1);
    lua_getfield(L, idx, "sched_flags");
    sa->sched_flags = (__u64)lua_tointeger(L, -1);
    lua_getfield(L, idx, "sched_nice");
    sa->sched_nice = (__s32)lua_tointeger(L, -1);
    lua_getfield(L, idx, "sched_priority");
    sa->sched_priority = (__u32)lua_tointeger(L, -1);
    lua_getfield(L, idx, "sched_runtime");
    sa->sched_runtime = (__u64)lua_tointeger(L, -1);
    lua_getfield(L, idx, "sched_deadline");
    sa->sched_deadline = (__u64)lua_tointeger(L, -1);
    lua_getfield(L, idx, "sched_period");
    sa->sched_period = (__u64)lua_tointeger(L, -1);
    lua_pop(L,8);
    return (void*)sa;
}

// struct sched_attr --> lua table
static void marshall_sched_attr(lua_State *L, int idx) {
    lua_getfield(L, idx, "__userdata");
    struct sched_attr *a = 
        (struct sched_attr*)lua_touserdata(L, -1);
    lua_pop(L, 2);
    lua_pushinteger(L, a->size);
    lua_setfield(L, idx, "size");
    lua_pushinteger(L, a->sched_policy);
    lua_setfield(L, idx, "sched_policy");
    lua_pushinteger(L, a->sched_flags);
    lua_setfield(L, idx, "sched_flags");
    lua_pushinteger(L, a->sched_nice);
    lua_setfield(L, idx, "sched_nice");
    lua_pushinteger(L, a->sched_priority);
    lua_setfield(L, idx, "sched_priority");
    lua_pushinteger(L, a->sched_runtime);
    lua_setfield(L, idx, "sched_runtime");
    lua_pushinteger(L, a->sched_deadline);
    lua_setfield(L, idx, "sched_deadline");
    lua_pushinteger(L, a->sched_period);
    lua_setfield(L, idx, "sched_period");
}

//////// FLOCK ///////////////////////////

// lua table --> struct *flock
static void* unmarshall_flock(lua_State *L, int idx) {
    if (lua_type(L, idx) != LUA_TTABLE) {
        log_error("flock got no table!");
        return 0;
    }
	struct flock* lock =
        (struct flock*)lua_newuserdata(L, sizeof(struct flock));
    lua_setfield(L, idx, "__userdata");
    lua_getfield(L, idx, "l_type");
    lock->l_type = (short)lua_tointeger(L, -1);
    lua_getfield(L, idx, "l_whence");
    lock->l_whence = (short)lua_tointeger(L, -1);
    lua_getfield(L, idx, "l_start");
    lock->l_start = (off_t)lua_tointeger(L, -1);
    lua_getfield(L, idx, "l_len");
    lock->l_len = (off_t)lua_tointeger(L, -1);
    lua_getfield(L, idx, "l_pid");
    lock->l_pid = (pid_t)lua_tointeger(L, -1);
    lua_pop(L, 5);
    return (void*)lock;
}

// struct flock --> lua table
static void marshall_flock(lua_State *L, int idx) {
    lua_getfield(L, idx, "__userdata");
    if (lua_type(L, -1) != LUA_TUSERDATA) {
        log_error("flock userdata!");
    }
    struct flock *l = 
        (struct flock*)lua_touserdata(L, -1);
    lua_pop(L, 1);
    lua_pushinteger(L, l->l_type);
    lua_setfield(L, idx, "l_type");
    lua_pushinteger(L, l->l_whence);
    lua_setfield(L, idx, "l_whence");
    lua_pushinteger(L, l->l_start);
    lua_setfield(L, idx, "l_start");
    lua_pushinteger(L, l->l_len);
    lua_setfield(L, idx, "l_len");
    lua_pushinteger(L, l->l_pid);
    lua_setfield(L, idx, "l_pid");
}

//////// STAT ////////////////////////////

static void marshall_stat(lua_State *L, int idx) {
    lua_getfield(L, idx, "__userdata");
    struct stat *s = (struct stat*)lua_touserdata(L, -1);
    lua_pop(L, 2);
    lua_pushinteger(L, s->st_dev);
    lua_setfield(L, idx, "st_dev");
    lua_pushinteger(L, s->st_ino);
    lua_setfield(L, idx, "st_ino");
    lua_pushinteger(L, s->st_mode);        /* file type and mode */
    lua_setfield(L, idx, "st_mode");
    lua_pushinteger(L, s->st_nlink);       /* number of hard links */
    lua_setfield(L, idx, "st_nlink");
    lua_pushinteger(L, s->st_uid);         /* user ID of owner */
    lua_setfield(L, idx, "st_uid");
    lua_pushinteger(L, s->st_gid);         /* group ID of owner */
    lua_setfield(L, idx, "st_gid");
    lua_pushinteger(L, s->st_rdev);        /* device ID (if special file) */
    lua_setfield(L, idx, "st_rdev");
    lua_pushinteger(L, s->st_size);        /* total size, in bytes */
    lua_setfield(L, idx, "st_size");
    lua_pushinteger(L, s->st_blksize);     /* blocksize for filesystem I/O */
    lua_setfield(L, idx, "st_blksize");
    lua_pushinteger(L, s->st_blocks);      /* number of 512B blocks allocated */
    lua_setfield(L, idx, "st_blocks");
}

// .. end of boring stuff

/*** helpers from posix lib ***/

static int argtypeerror(lua_State *L, int narg, const char *expected) {
    const char *got = luaL_typename(L, narg);
	return luaL_argerror(L, narg,
		lua_pushfstring(L, "%s expected, got %s",
            expected, got));
}

static lua_Integer checkinteger(lua_State *L,
        int narg, const char *expected) {
	lua_Integer d = lua_tointeger(L, narg);
	if (d == 0 && !lua_isinteger(L, narg))
		argtypeerror(L, narg, expected);
	return d;
}

static int optint(lua_State *L, int narg,
        lua_Integer dflt) {
	if (lua_isnoneornil(L, narg)) {
		return (int) dflt;
    }
	return (int)checkinteger(L, narg, "int or nil");
}

static int posixSleep(lua_State *L) {
	unsigned int seconds = luaL_checkinteger(L, 1);
	lua_pushinteger(L, sleep(seconds));
	return 1;
}

// TODO: this is a mess and should be re-done:
static int posixWait(lua_State *L) {
	int status = 0;
	pid_t pid = optint(L, 1, -1);
	int options = optint(L, 2, 0);
	//checknargs(L, 2);

	pid = waitpid(pid, &status, options);
    lua_pushinteger(L, pid);
	if (pid == -1) {
        //return pusherror(L, NULL);
        lua_pushliteral(L,"???");
        return 2;
    }
	if (pid == 0)
	{
		lua_pushliteral(L,"running");
		return 2;
	}
	else if (WIFEXITED(status))
	{
		lua_pushliteral(L,"exited");
		lua_pushinteger(L, WEXITSTATUS(status));
		return 3;
	}
	else if (WIFSIGNALED(status))
	{
		lua_pushliteral(L,"killed");
		lua_pushinteger(L, WTERMSIG(status));
		return 3;
	}
	else if (WIFSTOPPED(status))
	{
		lua_pushliteral(L,"stopped");
		lua_pushinteger(L, WSTOPSIG(status));
		return 3;
	}
	return 1;
}

static int posixFork(lua_State *L) {
	//checknargs(L, 0);
	lua_pushinteger(L, fork());
    return 1;
}

//////////////////////////

static long unmarshall(lua_State *L, int idx) {
    void *ud;
	lua_getfield(L, idx, "__type");
	// TODO: check type
	int datatype = lua_tointeger(L, -1);
	lua_pop(L, 1);
	// TODO: ??? probably usefull feature
	// to provide __SIZE for creating unstructured blobs

	// TODO: if __userdata already set - just return it???

	switch(datatype) {
	case LKTK_stat:
		ud = lua_newuserdata(L, sizeof(struct stat));
		/* no setting fields: output only struct */
	    lua_setfield(L, idx, "__userdata");
		break;
    case LKTK_flock:
        ud = unmarshall_flock(L, idx);
        break;
	case LKTK_sched_attr:
        ud = unmarshall_sched_attr(L, idx);
	    break;
	default:
		ud = 0;
		break;
	}
	return (long)ud;
}

static void marshall(lua_State *L, int idx) {
    lua_getfield(L, idx, "__type");
    int datatype = lua_tointeger(L, -1);
    lua_pop(L, 1);
    switch(datatype) {
    case LKTK_stat:
        marshall_stat(L, idx);
        break;
    case LKTK_flock:
        marshall_flock(L, idx);
        break;
    case LKTK_sched_attr:
        marshall_sched_attr(L, idx);
        break;
    default:
        break;
    }
}

/*
 * Converts any type to long
 * (or long pointer to void)
 */
static long any_to_long(lua_State* L, int idx, int *table_flag) {
    switch (lua_type(L, idx)) {
	case LUA_TBOOLEAN:
		return (long)lua_toboolean(L, idx);
	case LUA_TNUMBER:
		return (long)lua_tointeger(L, idx);
	case LUA_TSTRING:
		return (long)lua_tostring(L, idx);
	case LUA_TUSERDATA:
		return (long)lua_touserdata(L, idx);
	case LUA_TTABLE:
		*table_flag = 1;
		return unmarshall(L, idx);
	case LUA_TLIGHTUSERDATA:
	case LUA_TFUNCTION:
	case LUA_TTHREAD:
	case LUA_TNONE:
	case LUA_TNIL:
        // TODO: stop or not here ?
	    // TODO: treat nil somehow...
		return luaL_error(L, "wrong type to syscall");
	default:
		return luaL_error(L, "unknown type to syscall");
    }
}

/*
 * This is main procedure in this module
 * Calling sys by number
 * Parameters are just 'blindly' converted to long
 * That is probably OK for 64-bit linux calling conv.
 */
// TODO: return (syscall ret, error)
static int sysCall(lua_State *L) {
    long result = -1; 
    long argz[6];
    int arg_typez[6];
	int i;
	int table_flag;
    int arg_cnt = lua_gettop(L);
	if (arg_cnt < 1) {
		// TODO: handle error
        goto end;
	}
	int syscall_nr = luaL_checkinteger(L, 1);
	if (syscall_nr < 0) {
		// TODO: handle error
        goto end;
    }
	for (i=2; i<=arg_cnt; i++) {
		// TODO: save types: arg_typez[]
		table_flag = 0;
		argz[i-2] = any_to_long(L, i, &table_flag);
		arg_typez[i-2] = table_flag;
	}
	// TODO: write only used args ; parse types
	/* log before call */
    log_info("syscall #%d (0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx)",
        syscall_nr,
        argz[0], argz[1], argz[2],
        argz[3], argz[4], argz[5]);
	// do the main stuff:
    result = syscall(syscall_nr,
		argz[0], argz[1], argz[2],
		argz[3], argz[4], argz[5]);

	// TODO: parse struct args back
    for (i=0; i<=arg_cnt-2; i++) {
    	if (arg_typez[i]) {
    		marshall(L, i+2);// set i+2 param on the lua stack
    	}
    }
end:
    lua_pushinteger(L, result);
    return 1;
}

static int sizeof_sched_attr(lua_State *L) {
    lua_pushinteger(L, sizeof(struct sched_attr));
    return 1;
}

const struct luaL_Reg lktklib_globals[] = {
    {"is_root", isRoot},
    {"sizeof_sched_attr", sizeof_sched_attr},
    {"fork", posixFork},
    {"wait", posixWait},
    {"sleep", posixSleep},
    {"syscall", sysCall},
    {NULL, NULL}
};

void inject_lktklib(lua_State* L) {
    lua_pushglobaltable(L);
    luaL_setfuncs(L, lktklib_globals, 0);
	LPOSIX_CONST( FD_CLOEXEC	);
	LPOSIX_CONST( F_DUPFD		);
	LPOSIX_CONST( F_GETFD		);
	LPOSIX_CONST( F_SETFD		);
	LPOSIX_CONST( F_GETFL		);
	LPOSIX_CONST( F_SETFL		);
	LPOSIX_CONST( F_GETLK		);
	LPOSIX_CONST( F_SETLK		);
	LPOSIX_CONST( F_SETLKW		);
	LPOSIX_CONST( F_GETOWN		);
	LPOSIX_CONST( F_SETOWN		);
	LPOSIX_CONST( F_RDLCK		);
	LPOSIX_CONST( F_WRLCK		);
	LPOSIX_CONST( F_UNLCK		);
	/* file creation & status flags */
	LPOSIX_CONST( O_RDONLY		);
	LPOSIX_CONST( O_WRONLY		);
	LPOSIX_CONST( O_RDWR		);
	LPOSIX_CONST( O_APPEND		);
	LPOSIX_CONST( O_CREAT		);
	LPOSIX_CONST( O_DSYNC		);
	LPOSIX_CONST( O_EXCL		);
	LPOSIX_CONST( O_NOCTTY		);
	LPOSIX_CONST( O_NONBLOCK	);
	LPOSIX_CONST( O_RSYNC		);
	LPOSIX_CONST( O_SYNC		);
	LPOSIX_CONST( O_TRUNC		);
	LPOSIX_CONST( O_CLOEXEC		);
    /* ??? */
	LPOSIX_CONST( S_IFMT		);
	LPOSIX_CONST( S_IFBLK		);
	LPOSIX_CONST( S_IFCHR		);
	LPOSIX_CONST( S_IFDIR		);
	LPOSIX_CONST( S_IFIFO		);
	LPOSIX_CONST( S_IFLNK		);
	LPOSIX_CONST( S_IFREG		);
	LPOSIX_CONST( S_IFSOCK		);
	LPOSIX_CONST( S_IRWXU		);
	LPOSIX_CONST( S_IRUSR		);
	LPOSIX_CONST( S_IWUSR		);
	LPOSIX_CONST( S_IXUSR		);
	LPOSIX_CONST( S_IRWXG		);
	LPOSIX_CONST( S_IRGRP		);
	LPOSIX_CONST( S_IWGRP		);
	LPOSIX_CONST( S_IXGRP		);
	LPOSIX_CONST( S_IRWXO		);
	LPOSIX_CONST( S_IROTH		);
	LPOSIX_CONST( S_IWOTH		);
	LPOSIX_CONST( S_IXOTH		);
	LPOSIX_CONST( S_ISGID		);
	LPOSIX_CONST( S_ISUID		);
	/* datatype handles */
    LKTK_DATATYPE(stat);
    LKTK_DATATYPE(timespec);
    LKTK_DATATYPE(timeval);
    LKTK_DATATYPE(sockaddr);
    LKTK_DATATYPE(rlimit);
    LKTK_DATATYPE(perf_event_attr);
    LKTK_DATATYPE(sched_param);
    LKTK_DATATYPE(sched_attr);
    LKTK_DATATYPE(file_handle);
    LKTK_DATATYPE(epoll_event);
    LKTK_DATATYPE(flock);
    // TODO: more...
    lua_pop(L, 1);
}

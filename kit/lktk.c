/*
 ** $Id: lua.c,v 1.230 2017/01/12 17:14:26 roberto Exp $
 ** Lua stand-alone interpreter
 ** See Copyright Notice in lua.h
 */

#define lua_c

#include "lprefix.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "lktklib.h"
#include "lktkassert.h"

#include <getopt.h>

#if !defined(LUA_PROMPT)
#define LUA_PROMPT      "#> "
#define LUA_PROMPT2     "##> "
#endif

#if !defined(LUA_PROGNAME)
#define LUA_PROGNAME        "lktkit"
#endif

#if !defined(LUA_MAXINPUT)
#define LUA_MAXINPUT        512
#endif

#if !defined(LUA_INIT_VAR)
#define LUA_INIT_VAR        "LUA_INIT"
#endif

#define LUA_INITVARVERSION  LUA_INIT_VAR LUA_VERSUFFIX

/*
 ** lua_stdin_is_tty detects whether the standard input is a 'tty' (that
 ** is, whether we're running lua interactively).
 */
#if !defined(lua_stdin_is_tty)  /* { */

#if defined(LUA_USE_POSIX)  /* { */

#include <unistd.h>
#define lua_stdin_is_tty()  isatty(0)

#endif              /* } */
#endif              /* } */

/*
 ** lua_readline defines how to show a prompt and then read a line from
 ** the standard input.
 ** lua_saveline defines how to "save" a read line in a "history".
 ** lua_freeline defines how to free a line read by lua_readline.
 */
#if !defined(lua_readline)  /* { */

#if defined(LUA_USE_READLINE)   /* { */

#include <readline/readline.h>
#include <readline/history.h>
#define lua_readline(L,b,p) ((void)L, ((b)=readline(p)) != NULL)
#define lua_saveline(L,line)    ((void)L, add_history(line))
#define lua_freeline(L,b)   ((void)L, free(b))

#else               /* }{ */

#define lua_readline(L,b,p) \
        ((void)L, fputs(p, stdout), fflush(stdout),  /* show prompt */ \
        fgets(b, LUA_MAXINPUT, stdin) != NULL)  /* get line */
#define lua_saveline(L,line)    { (void)L; (void)line; }
#define lua_freeline(L,b)   { (void)L; (void)b; }

#endif              /* } */

#endif              /* } */


/********* globals *************************/

static lua_State *globalL = NULL;

static const char *progname = LUA_PROGNAME;

//TODO: multiple libraries ; avoid global
static const char *libname;
/********************************************/

static inline int is_bit_set(const int bit, const unsigned int mask) {
    return ((1U << bit) & mask);
}

/************** tainted *******************************/

#define tainted_file "/proc/sys/kernel/tainted"
#define TAINT_FLAGS_COUNT       16
#define TAINT_OOPS_BIT (1<<7)
#define TAINT_WARN_BIT (1<<9)
#define TAINT_SOFTLOCKUP_BIT (1<<14)
#define TAINT_ERROR \
    (TAINT_OOPS_BIT | TAINT_WARN_BIT | TAINT_SOFTLOCKUP_BIT)

static const char* kern_taint_msg[TAINT_FLAGS_COUNT] = {
    "Proprietary module has been loaded",
    "Module has been forcibly loaded",
    "SMP with CPUs not designed for SMP",
    "User forced a module unload",
    "System experienced a machine check exception",
    "System has hit bad_page",
    "Userspace-defined naughtiness",
    "Kernel has oopsed before",
    "ACPI table overridden",
    "Taint on warning",
    "Modules from drivers/staging are loaded",
    "Working around severe firmware bug",
    "Out-of-tree module has been loaded",
    "Unsigned module has been loaded",
    "A soft lockup has previously occurred",
    "Kernel has been live patched"
};

unsigned int get_tainted(void) {
    char buffer[16];
    int tainted_fd = open(tainted_file, O_RDONLY);
    // todo: throw
    buffer[15] = 0;
    //lseek(taint_fd, 0, SEEK_SET);
    read(tainted_fd, buffer, 10);
    return (unsigned int)atoi(buffer);
}

void print_tainted(void) {
    int mask = get_tainted();
    if (!mask) {
        echo_good("Kernel not tainted");
        return;
    }
    int i;
    echo_warn("Taint info: %x", mask);
    for (i = 0; i < TAINT_FLAGS_COUNT; i++) {
        if (is_bit_set(i, mask)) {
            echo_warn("%s", kern_taint_msg[i]);
        }
    }
}

// actually only new errors are of interest
// but there is no way to clear bits
int did_kernel_error(void) {
    return (0 != (get_tainted() & TAINT_ERROR));
}

/************** status ********************************/

void print_status(lua_State *L) {
    int failures = kit.failures;
    if (failures) {
        echo_error("Assertions failed: %d", failures);
    } else {
        echo_good("Assertions succeed");
    }
    if (did_kernel_error()) {
        echo_error("There were kernel errors!");
        print_tainted();
    } else {
        echo_good("No new kernel errors");
    }
}

/************** tainted ends **************************/

/*
 ** Hook set by signal function to stop the interpreter.
 */
static void lstop(lua_State *L, lua_Debug *ar) {
    (void) ar; /* unused arg. */
    lua_sethook(L, NULL, 0, 0); /* reset hook */
    luaL_error(L, "interrupted!");
}

/*
 ** Function to be called at a C signal. Because a C signal cannot
 ** just change a Lua state (as there is no proper synchronization),
 ** this function only sets a hook that, when called, will stop the
 ** interpreter.
 */
static void laction(int i) {
    signal(i, SIG_DFL); /* if another SIGINT happens, terminate process */
    lua_sethook(globalL, lstop, LUA_MASKCALL | LUA_MASKRET | LUA_MASKCOUNT, 1);
}

/*
 ** Prints an error message, adding the program name in front of it
 ** (if present)
 */
static void l_message(const char *pname, const char *msg) {
    if (pname)
        lua_writestringerror("%s: ", pname);
    lua_writestringerror("%s\n", msg);
}

/*
 ** Check whether 'status' is not OK and, if so, prints the error
 ** message on the top of the stack. It assumes that the error object
 ** is a string, as it was either generated by Lua or by 'msghandler'.
 */
static int report(lua_State *L, int status) {
    if (status != LUA_OK) {
        const char *msg = lua_tostring(L, -1);
        l_message(progname, msg);
        lua_pop(L, 1); /* remove message */
    }
    return status;
}

/*
 ** Message handler used to run all chunks
 */
static int msghandler(lua_State *L) {
    const char *msg = lua_tostring(L, 1);
    if (msg == NULL) { /* is error object not a string? */
        if (luaL_callmeta(L, 1, "__tostring") && /* does it have a metamethod */
        lua_type(L, -1) == LUA_TSTRING) /* that produces a string? */
            return 1; /* that is the message */
        else
            msg = lua_pushfstring(L, "(error object is a %s value)",
                    luaL_typename(L, 1));
    }
    luaL_traceback(L, L, msg, 1); /* append a standard traceback */
    return 1; /* return the traceback */
}

/*
 ** Interface to 'lua_pcall', which sets appropriate message function
 ** and C-signal handler. Used to run all chunks.
 */
static int docall(lua_State *L, int narg, int nres) {
    int status;
    int base = lua_gettop(L) - narg; /* function index */
    lua_pushcfunction(L, msghandler); /* push message handler */
    lua_insert(L, base); /* put it under function and args */
    globalL = L; /* to be available to 'laction' */
    signal(SIGINT, laction); /* set C-signal handler */
    status = lua_pcall(L, narg, nres, base);
    signal(SIGINT, SIG_DFL); /* reset C-signal handler */
    lua_remove(L, base); /* remove message handler from the stack */
    return status;
}

/*
 ** Create the 'arg' table, which stores all arguments from the
 ** command line ('argv'). It should be aligned so that, at index 0,
 ** it has 'argv[script]', which is the script name. The arguments
 ** to the script (everything after 'script') go to positive indices;
 ** other arguments (before the script name) go to negative indices.
 ** If there is no script name, assume interpreter's name as base.
 */
//static void createargtable(lua_State *L, char **argv, int argc, int script) {
//    int i, narg;
//    if (script == argc)
//        script = 0; /* no script name? */
//    narg = argc - (script + 1); /* number of positive indices */
//    lua_createtable(L, narg, script + 1);
//    for (i = 0; i < argc; i++) {
//        lua_pushstring(L, argv[i]);
//        lua_rawseti(L, -2, i - script);
//    }
//	lua_newtable(L);
//	lua_setglobal(L, "arg");
//}

static int dochunk(lua_State *L, int status) {
    if (status == LUA_OK)
        status = docall(L, 0, 0);
    return report(L, status);
}

static int dofile(lua_State *L, const char *name) {
    return dochunk(L, luaL_loadfile(L, name));
}

static int dostring(lua_State *L, const char *s, const char *name) {
    return dochunk(L, luaL_loadbuffer(L, s, strlen(s), name));
}

/*
 ** Calls 'require(name)' and stores the result in a global variable
 ** with the given name.
 */
static int dolibrary(lua_State *L, const char *name) {
    int status;
    lua_getglobal(L, "require");
    lua_pushstring(L, name);
    /* call 'require(name)' */
    status = docall(L, 1, 1);
    /* global[name] = require return */
    if (status == LUA_OK)
        lua_setglobal(L, name);
    return report(L, status);
}

/*
 ** Returns the string to be used as a prompt by the interpreter.
 */
static const char *get_prompt(lua_State *L, int firstline) {
    const char *p;
    lua_getglobal(L, firstline ? "_PROMPT" : "_PROMPT2");
    p = lua_tostring(L, -1);
    if (p == NULL)
        p = (firstline ? LUA_PROMPT : LUA_PROMPT2);
    return p;
}

/* mark in error messages for incomplete statements */
#define EOFMARK     "<eof>"
#define marklen     (sizeof(EOFMARK)/sizeof(char) - 1)

/*
 ** Check whether 'status' signals a syntax error and the error
 ** message at the top of the stack ends with the above mark for
 ** incomplete statements.
 */
static int incomplete(lua_State *L, int status) {
    if (status == LUA_ERRSYNTAX) {
        size_t lmsg;
        const char *msg = lua_tolstring(L, -1, &lmsg);
        if (lmsg >= marklen && strcmp(msg + lmsg - marklen, EOFMARK) == 0) {
            lua_pop(L, 1);
            return 1;
        }
    }
    return 0; /* else... */
}

/*
 ** Prompt the user, read a line, and push it into the Lua stack.
 */
static int pushline(lua_State *L, int firstline) {
    char buffer[LUA_MAXINPUT];
    char *b = buffer;
    size_t l;
    const char *prmt = get_prompt(L, firstline);
    int readstatus = lua_readline(L, b, prmt);
    if (readstatus == 0)
        return 0; /* no input (prompt will be popped by caller) */
    lua_pop(L, 1); /* remove prompt */
    l = strlen(b);
    if (l > 0 && b[l - 1] == '\n') /* line ends with newline? */
        b[--l] = '\0'; /* remove it */
    if (firstline && b[0] == '=') /* for compatibility with 5.2, ... */
        lua_pushfstring(L, "return %s", b + 1); /* change '=' to 'return' */
    else
        lua_pushlstring(L, b, l);
    lua_freeline(L, b);
    return 1;
}

/*
 ** Try to compile line on the stack as 'return <line>;'; on return, stack
 ** has either compiled chunk or original line (if compilation failed).
 */
static int addreturn(lua_State *L) {
    const char *line = lua_tostring(L, -1); /* original line */
    const char *retline = lua_pushfstring(L, "return %s;", line);
    int status = luaL_loadbuffer(L, retline, strlen(retline), "=stdin");
    if (status == LUA_OK) {
        lua_remove(L, -2); /* remove modified line */
        if (line[0] != '\0') /* non empty? */
            lua_saveline(L, line); /* keep history */
    } else
        lua_pop(L, 2); /* pop result from 'luaL_loadbuffer' and modified line */
    return status;
}

/*
 ** Read multiple lines until a complete Lua statement
 */
static int multiline(lua_State *L) {
    for (;;) { /* repeat until gets a complete statement */
        size_t len;
        const char *line = lua_tolstring(L, 1, &len); /* get what it has */
        int status = luaL_loadbuffer(L, line, len, "=stdin"); /* try it */
        if (!incomplete(L, status) || !pushline(L, 0)) {
            lua_saveline(L, line); /* keep history */
            return status; /* cannot or should not try to add continuation line */
        }
        lua_pushliteral(L, "\n"); /* add newline... */
        lua_insert(L, -2); /* ...between the two lines */
        lua_concat(L, 3); /* join them */
    }
}

/*
 ** Read a line and try to load (compile) it first as an expression (by
 ** adding "return " in front of it) and second as a statement. Return
 ** the final status of load/call with the resulting function (if any)
 ** in the top of the stack.
 */
static int loadline(lua_State *L) {
    int status;
    lua_settop(L, 0);
    if (!pushline(L, 1))
        return -1; /* no input */
    if ((status = addreturn(L)) != LUA_OK) /* 'return ...' did not work? */
        status = multiline(L); /* try as command, maybe with continuation lines */
    lua_remove(L, 1); /* remove line from the stack */
    lua_assert(lua_gettop(L) == 1);
    return status;
}

/*
 ** Prints (calling the Lua 'print' function) any values on the stack
 */
static void l_print(lua_State *L) {
    int n = lua_gettop(L);
    if (n > 0) { /* any result to be printed? */
        luaL_checkstack(L, LUA_MINSTACK, "too many results to print");
        lua_getglobal(L, "print");
        lua_insert(L, 1);
        if (lua_pcall(L, n, 0, 0) != LUA_OK)
            l_message(progname,
                    lua_pushfstring(L, "error calling 'print' (%s)",
                            lua_tostring(L, -1)));
    }
}

/*
 ** Do the REPL: repeatedly read (load) a line, evaluate (call) it, and
 ** print any results.
 */
static void doREPL(lua_State *L) {
    int status;
    const char *oldprogname = progname;
    progname = NULL; /* no 'progname' on errors in interactive mode */
    while ((status = loadline(L)) != -1) {
        if (status == LUA_OK)
            status = docall(L, 0, LUA_MULTRET);
        if (status == LUA_OK)
            l_print(L);
        else
            report(L, status);
    }
    lua_settop(L, 0); /* clear stack */
    lua_writeline();
    progname = oldprogname;
}

/*
 ** Push on the stack the contents of table 'arg' from 1 to #arg
 */
static int pushargs(lua_State *L) {
    int i, n;
    if (lua_getglobal(L, "arg") != LUA_TTABLE)
        luaL_error(L, "'arg' is not a table");
    n = (int) luaL_len(L, -1);
    luaL_checkstack(L, n + 3, "too many arguments to script");
    for (i = 1; i <= n; i++)
        lua_rawgeti(L, -i, i);
    lua_remove(L, -i); /* remove table from the stack */
    return n;
}

static int handle_script(lua_State *L, char **argv) {
    int status;
    const char *fname = argv[0];
    if (strcmp(fname, "-") == 0 && strcmp(argv[-1], "--") != 0)
        fname = NULL; /* stdin */
    status = luaL_loadfile(L, fname);
    if (status == LUA_OK) {
    	log_info("starting script: %s", fname);
        int n = pushargs(L); /* push arguments to script */
        status = docall(L, n, LUA_MULTRET);
    }
    return report(L, status);
}

/*
 ** Processes options 'e' and 'l', which involve running Lua code.
 ** Returns 0 if some code raises an error.
 */
//static int runargs(lua_State *L, char **argv, int n) {
//    int i;
//    for (i = 1; i < n; i++) {
//        int option = argv[i][1];
//        lua_assert(argv[i][0] == '-'); /* already checked */
//        if (option == 'e' || option == 'l') {
//            int status;
//            const char *extra = argv[i] + 2; /* both options need an argument */
//            if (*extra == '\0')
//                extra = argv[++i];
//            lua_assert(extra != NULL);
//            status =
//                    (option == 'e') ?
//                            dostring(L, extra, "=(command line)") :
//                            dolibrary(L, extra);
//            if (status != LUA_OK)
//                return 0;
//        }
//    }
//    return 1;
//}

static int handle_luainit(lua_State *L) {
    const char *name = "=" LUA_INITVARVERSION;
    const char *init = getenv(name + 1);
    if (init == NULL) {
        name = "=" LUA_INIT_VAR;
        init = getenv(name + 1); /* try alternative name */
    }
    if (init == NULL)
        return LUA_OK;
    else if (init[0] == '@')
        return dofile(L, init + 1);
    else
        return dostring(L, init, name);
}

// TODO: exclude some of the libs
static const luaL_Reg kerntest_preload_libs[] = {
	{ "_G", luaopen_base },
	{ LUA_LOADLIBNAME, luaopen_package },
	{ LUA_COLIBNAME, luaopen_coroutine },
	{ LUA_TABLIBNAME, luaopen_table },
	{ LUA_IOLIBNAME, luaopen_io },
	{ LUA_OSLIBNAME, luaopen_os },
    { LUA_STRLIBNAME, luaopen_string },
	{ LUA_MATHLIBNAME, luaopen_math },
	{ LUA_UTF8LIBNAME, luaopen_utf8 },
	{ LUA_DBLIBNAME, luaopen_debug },
	//#if defined(LUA_COMPAT_BITLIB)
	{ LUA_BITLIBNAME, luaopen_bit32 },
	//#endif
	{ NULL, NULL }
};

/*
 ** Main body of stand-alone interpreter (to be called in protected mode).
 ** Reads the options and handles them all.
 */
static int pmain(lua_State *L) {

	int argc = (int) lua_tointeger(L, 1);
    char **argv = (char **) lua_touserdata(L, 2);

//    int status;

    /* check that interpreter has correct version */
    luaL_checkversion(L);

    /* signal for libraries to ignore env. vars. */
	lua_pushboolean(L, 1);
	lua_setfield(L, LUA_REGISTRYINDEX, "LUA_NOENV");

    /* open standard libraries,
     * actually luaL_openlibs(L); */
    const luaL_Reg *lib;
    /* "require" functions from 'loadedlibs'
     * and set results to global table */
    for (lib = kerntest_preload_libs; lib->func; lib++) {
        luaL_requiref(L, lib->name, lib->func, 1);
        lua_pop(L, 1);
    }

    inject_lktklib(L);
    inject_lktkassert(L);

    // TODO: should scripts have args?
    /* create table 'arg' - TODO: fake call */
    lua_newtable(L);
    lua_setglobal(L, "arg");

	if (handle_luainit(L) != LUA_OK) {
		return 0;
	}

	// load custom library (-l)
	if(libname) {
		if (dolibrary(L, libname) != LUA_OK) {
			return 0;
		}
	}

    /* execute all the scripts */
    // TODO: do 'interations' times
	// in 'mutant' mode start by mutating only last syscall
	// and then radndomize upper and upper..
    // TODO: if processes == 1 dont spawn child
#define processes (kit.parallel)
    if (0 < argc) {
    	int cur_script = 0;
    	while (cur_script < argc) {
    		if (!processes) {
				handle_script(L, argv + cur_script);
				if (kit.verbose) print_status(L);
    		} else {
				for (int i=0; i<processes; i++) {
				    if (kit.verbose) {
				        echo_debug("forking script %s:%d", argv[cur_script], i);
				    }
					int pid = fork();
					if (0 > pid) {
						l_message(argv[0], "cannot fork");
						return EXIT_FAILURE;
					}
					if (0 == pid) {
						// TODO: check if != LUA_OK
						handle_script(L, argv + cur_script);
					    if (kit.verbose) {
					        print_status(L);
					    }
						exit(EXIT_SUCCESS /* TODO: check if EXIT_FAILURE*/ );
					}
				}
    		}
    		cur_script++;
    	}
    	if (processes) {
			int wstatus, ret_pid;
			while((ret_pid = waitpid(-1, &wstatus, 0)) > 0) {
				if (WIFEXITED(wstatus) && (WEXITSTATUS(wstatus)==0)) {
				    if (kit.verbose) {
				    	echo_good("%d exited ok", ret_pid);
				    }
				} else {
				    if (kit.verbose) {
				    	echo_error("%d exited badly", ret_pid);
				    }
				}
			}
    	}
    } // scripts
#undef processes

    /* interactive */
    if (kit.interactive) {
        /* do read-eval-print loop */
        doREPL(L);
    } else if (0 == argc) {
        if (lua_stdin_is_tty()) {
            doREPL(L);
        } else {
        	// ?? from stdin
            dofile(L, NULL);
        }
    }
    /* no errors */
    lua_pushboolean(L, 1);
    return 1;
}

/********** non protected *************************/

static void print_usage() {
    echo_debug("usage: %s [options] [script (NO ARGS)]\n"
            "Available options are:\n"
            "  -v       verbose (print out each assert)\n"
            "  -L       log to syslog\n"
            "  -A       abort on failed assert\n"
            "  -x       no asserts\n"
            "  -q       be queit\n"
            "  -k       check kernel stuff\n"
            "  -s       collect sys stats\n"
            "  -t       collect traces\n"
            "  -u u,u   users to use\n"
            "  -w d,d   workdirs\n"
            "  ------------------\n"
            "  -e stat  execute string 'stat'\n"
            "  -i       enter interactive mode after executing 'script'\n"
            "  -l name  require library 'name'\n"
            "  -E       ignore environment variables\n"
            "  --       stop handling options\n"
            "  -        stop handling options and execute stdin\n", progname);
}

static void print_version(void) {
    echo_debug(LUA_COPYRIGHT);
    echo_info(":: Linux Kernel Test Kit ::")
}

static void parse_cmdline(int argc, char **args, int* first, lua_State *L) {
    struct option long_option[] = {
        {"verbose",      0, NULL, 'v'},
        {"execute",      1, NULL, 'e'},
        {"interactive",  0, NULL, 'i'},
        {"preload",      1, NULL, 'l'},
        {"noenv",        0, NULL, 'E'},
        {"assert",       0, NULL, 'A'},
        {"process",      1, NULL, 'p'},
        {"timeout",      1, NULL, 'T'},
        {"count",        1, NULL, 'c'},
        {"quiet",        0, NULL, 'q'},
        {"syslog",       0, NULL, 'L'},
        {"noassert",     0, NULL, 'x'},
        {"kernel",       0, NULL, 'k'},
        {"stats",        0, NULL, 's'},
        {"traces",       0, NULL, 't'},
        {NULL,           0, NULL,  0 },
    };
    while (1) {
    	int x;
        int c;
        if ((c = getopt_long(argc, args, "eil:Ep:kstc:T:AxLvq", long_option, NULL)) < 0) {
            break;
        }
        switch (c) {
        case 'v':
            kit.verbose = 1; break;
        case 'A':
            kit.strict = 1; break;
        case 'x':
            kit.strict = -1; break;
        case 'L':
            kit.syslog = 1; break;
        case 'i':
        	print_version();
            kit.interactive = 1; break;
        case 'c':
            if (sscanf(optarg, "%d" , &x) == 1 ) {
            	kit.iterations = x;
            } else {
                goto error_happened;
            }
            break;
        case 'p':
            if (sscanf(optarg, "%d" , &x) == 1 ) {
            	kit.parallel = x;
            } else {
                goto error_happened;
            }
            break;
        case 'q':
            kit.syslog = 0;
            kit.verbose = 0;
            break;
        case 'l':
            if (!optarg) {
                goto error_happened;
            } else {
            	libname = optarg;
            }
            break;
        case 'k':
        case 's':
        case 't':
            break;
        default:
            *first = optind;
            goto error_happened;
        }
    }
    *first = optind;
    return;
error_happened:
	print_usage(args[optind]);
	exit(1);
}

static void start(lua_State * L) {
	if (kit.syslog) {
		openlog(LUA_PROGNAME, LOG_NDELAY | LOG_CONS, LOG_LOCAL0);
	}
	// print_tainted
}

static void stop(lua_State *L) {
	if (kit.syslog) {
		closelog();
	}
	/* TODO: L is meaningless here (due to fork)) */
	lua_close(L);
}

int main(int argc, char **argv) {

	int script;

    // TODO: question create L once or for each fork?
	lua_State *L = luaL_newstate();
	if (L == NULL) {
		l_message(argv[0], "cannot create state: not enough memory");
		return EXIT_FAILURE;
	}

    parse_cmdline(argc, argv, &script, L);

    start(L);

	/* call 'pmain' in protected mode */
	lua_pushcfunction(L, &pmain);
	lua_pushinteger(L, argc - script); /* 1st argument */
	lua_pushlightuserdata(L, argv + script); /* 2nd argument */
	lua_pcall(L, 2, 1, 0);

	stop(L);

    return 0;
}


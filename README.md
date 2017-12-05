# L.K.T.K. - Linux Kernel Test Kit #

### Experimental infrastructure for scripting Linux system tests in Lua ###

This project is driven by these statements, taken as 'axioms':

- test should be a script (i.e. a scenario describing user case)
- test infrastructure should not prohibit tests to do wrong/invalid/stupid things
- infrastructure (library, framework..) should do all the dirty job

Some 'consequences' of 'axioms':

- the less logic put in test the better


### TODO ###

- errno handling helpers in C
- assert_zero ; assert_noerr ???
- push syscalls as _nr_xxx_ and override if needed
- impersonate user
- randomizers
- dmesg grabbing
- timeout on each test
- do (-a) all tests ; do multiple scripts
- fork test proc ; number of repetitions
- multi-instance ; parallel (different) tests
- option to set different sys options
- 'learning' mode : strace-ing syscalls with params and saving to db
- Reporting: collecting test assertions ; grabbing kernel messages ; ftrace ; sar stats
- Protocol testing - probably ??
- Fuzzing - probably ??


### Usage example ###

```bash
$ ./lktk dummy.lua 
	Assert GreaterOrEquals Failed [1 >= 2] expected: >=2 actual: 1

$ ./lktk --verbose dummy.lua 
	starting script: dummy.lua
	Assert pass [1 is True]
	Assert pass [1 = 1]
	Assert GreaterOrEquals Failed [1 >= 2] expected: >=2 actual: 1
	Assert pass [mute]
	Assertions failed: 1
	No new kernel errors

$ ./lktk --verbose --assert dummy.lua 
	starting script: dummy.lua
	Assert pass [1 is True]
	Assert pass [1 = 1]
	Assert GreaterOrEquals Failed [1 >= 2] expected: >=2 actual: 1
	lktkit: dummy.lua:4: Assert GreaterOrEquals Failed
	stack traceback:
		[C]: in function 'assert_ge'
		dummy.lua:4: in main chunk
		[C]: in ?
	Assertions failed: 1
	No new kernel errors

$ ./lktk --verbose --noassert dummy.lua 
	starting script: dummy.lua
	Assertions succeed
	No new kernel errors

$ ./lktk --preload mutator sanity-test.lua
    # example of overloading 'syscall' function by mutator module 
```

### notes ###

- how to include headers to kernel structures? LTP duplicates them sometimes ; or use kernel sources | headers ? **ok now it just has been copied to linux-api**
[glibc version hacks](https://stackoverflow.com/questions/4032373/linking-against-an-old-version-of-libc-to-provide-greater-application-coverage)

[Programming In Lua Code](https://github.com/xfbs/PiL3)

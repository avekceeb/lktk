local pid = syscall(require "syscalls".getpid)
math.randomseed(os.time()+pid)
local t = math.random(1,10)
assert_eq(0 , pid % 2, "assert random is even, probability of failure - 50%")
print(string.format("going to sleep %d s",t)) 
sleep(t)


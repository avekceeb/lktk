
function forkme(child)
    local pid, errmsg = fork ()
    if pid == nil then
        error (errmsg)
    elseif pid == 0 then
        child()
        os.exit(0)
    else
       return 
    end
end

function th()
    print ("child "..syscall(require "syscalls".getpid))
    local line = io.read()
end

print([[

    this will fork 3 processes
    each waiting for some input from stdin
    ]])

for i=1,3 do
    forkme(th)
end

while 0 < wait() do
    print("parent waits")
end

set pagination off
set confirm off
set breakpoint pending on
python
created = {}
class Created(gdb.Breakpoint):
    def stop(self):
        f = gdb.newest_frame()
        p = int(f.read_var('proxy'))
        name = f.read_var('interface').dereference()['name'].string()
        created[p] = name
        return False
class Destroyed(gdb.Breakpoint):
    def stop(self):
        p = int(gdb.newest_frame().read_var('proxy'))
        created.pop(p, None)
        return False
Created('/home/kyau/.conan2/p/b/wayla064c9fb324c75/b/src/src/wayland-client.c:554')
Destroyed('proxy_destroy')
end
run
python
for name in created.values(): print('NOT_DESTROYED_INTERFACE:', name)
print('NOT_DESTROYED_COUNT:', len(created))
end

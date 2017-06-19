test_run = require('test_run').new()
fiber = require('fiber')

-- Temporary table to restore variables after restart.
tmp = box.schema.space.create('tmp')
_ = tmp:create_index('primary', {parts = {1, 'string'}})

--
-- Check that range splits, compactions, and dumps don't break recovery.
--
s = box.schema.space.create('test', {engine='vinyl'})
_ = s:create_index('primary', {page_size=256, range_size=2048, run_count_per_level=1, run_size_ratio=10})

function vyinfo() return box.space.test.index.primary:info() end

range_count = 4
tuple_size = math.ceil(s.index.primary.options.page_size / 4)
pad_size = tuple_size - 30
assert(pad_size >= 16)
keys_per_range = math.floor(s.index.primary.options.range_size / tuple_size)
key_count = range_count * keys_per_range

-- Add a number of tuples to the test space to trigger range splitting.
-- Rewrite them several times with different values so that different
-- generations of ranges on disk have different contents.
test_run:cmd("setopt delimiter ';'")
iter = 0
function gen_tuple(k)
    local pad = {}
    for i = 1,pad_size do
        pad[i] = string.char(math.random(65, 90))
    end
    return {k, k + iter, table.concat(pad)}
end
while vyinfo().range_count < range_count do
    iter = iter + 1
    for k = key_count,1,-1 do s:replace(gen_tuple(k)) end
    box.snapshot()
    fiber.sleep(0.01)
end;
test_run:cmd("setopt delimiter ''");

-- Remember the number of iterations and the number of keys
-- so that we can check data validity after restart.
_ = tmp:insert{'iter', iter}
_ = tmp:insert{'key_count', key_count}

test_run:cmd('restart server default')

s = box.space.test
tmp = box.space.tmp

-- Check that the test space content was not corrupted.
iter = tmp:get('iter')[2]
key_count = tmp:get('key_count')[2]
for k = 1,key_count do v = s:get(k) assert(v == nil or v[2] == k + iter) end

s:drop()

--
-- Check that vinyl stats are restored correctly.
--
s = box.schema.space.create('test', {engine='vinyl'})
_ = s:create_index('primary', {run_count_per_level=10})

-- Generate data.
for i=1,2 do for j=1,10 do s:insert{i*100+j, 'test' .. j} end box.snapshot() end

-- Remember stats before restarting the server.
_ = tmp:insert{'vyinfo', s.index.primary:info()}

test_run:cmd('restart server default')

s = box.space.test
tmp = box.space.tmp

-- Check that stats didn't change after recovery.
vyinfo1 = tmp:get('vyinfo')[2]
vyinfo2 = s.index.primary:info()

vyinfo1.memory.rows == vyinfo2.memory.rows
vyinfo1.memory.bytes == vyinfo2.memory.bytes
vyinfo1.disk.rows == vyinfo2.disk.rows
vyinfo1.disk.bytes == vyinfo2.disk.bytes
vyinfo1.disk.bytes_compressed == vyinfo2.disk.bytes_compressed
vyinfo1.disk.pages == vyinfo2.disk.pages
vyinfo1.run_count == vyinfo2.run_count
vyinfo1.range_count == vyinfo2.range_count

s:drop()

tmp:drop()

test_run:cmd('create server force_recovery with script="vinyl/force_recovery.lua"')
test_run:cmd('start server force_recovery')
test_run:cmd('switch force_recovery')
fio = require'fio'

test = box.schema.space.create('test', {engine = 'vinyl'})
_ = test:create_index('pk')
for i = 0, 9999 do test:replace({i, i, string.rep('a', 512)}) end
box.snapshot()
for i = 10000, 11999 do test:delete({i - 10000}) end
box.snapshot()
for i = 12000, 13999 do test:upsert({i - 10000, i, string.rep('a', 128)}, {{'+', 2, 5}}) end
box.snapshot()
for _, f in pairs(fio.glob(box.cfg.vinyl_dir .. '/' .. test.id .. '/0/*.index')) do fio.unlink(f) end

test2 = box.schema.space.create('test2', {engine = 'vinyl'})
_ = test2:create_index('pk')
_ = test2:create_index('sec', {parts = {4, 'NUM', 2, 'STR'}})
test2:replace({1, 'a', 2, 3})
test2:replace({2, 'd', 4, 1})
test2:replace({3, 'c', 6, 7})
test2:replace({4, 'b', 6, 3})
box.snapshot()
for _, f in pairs(fio.glob(box.cfg.vinyl_dir .. '/' .. test2.id .. '/0/*.index')) do fio.unlink(f) end
for _, f in pairs(fio.glob(box.cfg.vinyl_dir .. '/' .. test2.id .. '/1/*.index')) do fio.unlink(f) end

test_run = require('test_run').new()
test_run:cmd('switch default')
test_run:cmd('stop server force_recovery')
test_run:cmd('start server force_recovery')
test_run:cmd('switch force_recovery')
sum = 0
for k, v in pairs(box.space.test:select()) do sum = sum + v[2] end
-- should be a sum(2005 .. 4004) + sum(4000 .. 9999) = 48006000
sum
box.space.test2:select()
box.space.test2.index.sec:select()
test_run:cmd('switch default')
test_run:cmd('stop server force_recovery')

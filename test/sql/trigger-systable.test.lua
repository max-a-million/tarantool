test_run = require('test_run').new()

-- Try to create triggers on system tables

-- On _trigger
box.sql.execute("CREATE TRIGGER tt BEFORE UPDATE ON _trigger BEGIN SELECT 1; END")

-- On _space
box.sql.execute("CREATE TRIGGER tt BEFORE UPDATE ON _space BEGIN SELECT 1; END")

-- On _index
box.sql.execute("CREATE TRIGGER tt BEFORE UPDATE ON _index BEGIN SELECT 1; END")






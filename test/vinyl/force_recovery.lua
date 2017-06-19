#!/usr/bin/env tarantool

box.cfg{
    force_recovery      = true
}

require('console').listen(os.getenv('ADMIN'))


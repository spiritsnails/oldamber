#!/bin/sh
# Entry point on PATH. The real binary stays in /app/oldamber, because it finds
# the setup importer relative to itself and reads shaders and mod_runtime from
# beside itself before seeding them into the writable data directory.
exec /app/oldamber/OldAmber "$@"

"""Compatibility entry for the scoped, read-only R10 telemetry check."""
import asyncio
from verify_ws import check

if __name__ == "__main__":
    asyncio.run(check())

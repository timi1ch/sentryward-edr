import asyncio, signal, json
import os, sys, time
from datetime import datetime, timezone

SOCK_PATH = "/run/sentryward/events.sock"

def transform_time_to_iso(old_json: str):
    data = json.loads(old_json)
    monotonic_ns = data.get('monotonic_ns')

    if monotonic_ns is not None:
        event_time_ns = time.time_ns() - time.monotonic_ns() + int(monotonic_ns)
        seconds, nanoseconds = divmod(event_time_ns, 1_000_000_000)
        dt = datetime.fromtimestamp(seconds, tz=timezone.utc).replace(
            microsecond=nanoseconds // 1_000
        )
        data['timestamp'] = dt.isoformat()

    return json.dumps(data)

async def handle_sender(reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
    peer = writer.get_extra_info('peername')
    peer = os.path.basename(peer) if peer else 'client'
    print(f'[INFO] A listener has connected: {peer}', file=sys.stderr)
    try:
        while line := await reader.readline():
            if not line.endswith(b"\n"):
                break
            text = line.decode('utf-8', errors='replace').strip()
            if not text:
                continue
            try:
                event = transform_time_to_iso(text)
                print("Event:", event)
            except json.JSONDecodeError as e:
                print(f"[ERROR] Invalid JSON {e}", file=sys.stderr)

    except asyncio.CancelledError:
        pass
    finally:
        print(f'[INFO] A listener has disconnected: {peer}', file=sys.stderr)
        writer.close()
        await writer.wait_closed()


async def main():
    if os.path.exists(SOCK_PATH):
        os.unlink(SOCK_PATH)

    server = await asyncio.start_unix_server(handle_sender, path=SOCK_PATH)
    os.chmod(SOCK_PATH, 0o220)

    loop = asyncio.get_running_loop()
    stop_event = asyncio.Event()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, stop_event.set)
    
    print(f'Listening {SOCK_PATH}...')
    async with server:
        await stop_event.wait()

    if os.path.exists(SOCK_PATH):
        os.unlink(SOCK_PATH)

if __name__ == '__main__':
    asyncio.run(main())

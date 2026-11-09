# SentryWard EDR

**Cross-platform Endpoint Detection and Response System**

SentryWard is an educational EDR project for collecting endpoint telemetry,
investigating process activity, and detecting suspicious behavior. The goal is
to build a modular system with process, network, and filesystem monitoring,
PostgreSQL storage, rule-based detection, and investigation tools.

Development starts on Linux with C and Python. Windows support is planned
through a separate sensor that produces the same event protocol. Collection,
storage, analysis, and response are separate responsibilities.

## Current status

The current implementation is a Linux process exec sensor and a C collector.
It emits telemetry to the terminal; storage and threat detection are not yet
implemented. This is an early prototype, not a production-ready EDR.

| Component | Status |
| --- | --- |
| Successful process exec events | Implemented with `sched_process_exec` |
| BPF ring buffer transport to userspace | Implemented |
| C collector with a dedicated process worker | Implemented |
| JSON serialization | Implemented with cJSON |
| Shutdown through SIGINT and SIGTERM | Implemented |
| Fork/clone and exit events | Planned |
| Executable paths and command-line arguments | Planned |
| Network and filesystem sensors | Planned |
| Python receiver, PostgreSQL, and detection engine | Planned |
| Web interface, Windows support, and response actions | Planned |

## Architecture

The implemented data flow is:

```text
Successful exec
      |
      v
Linux eBPF sensor
      |
      v
BPF ring buffer
      |
      v
C collector worker
      |
      v
JSON events on stdout
```

The next stages extend this flow:

```text
C collector -> JSON / Unix socket -> Python receiver -> PostgreSQL
                                                            |
                                                            v
                                                    Python analyzer
                                                            |
                                                            v
                                                     Stored alerts
```

The receiver will validate and persist incoming events. A separate analyzer
will process stored events, evaluate detection rules, and save alerts. Network
and filesystem sensors will feed the same pipeline.

Initially, the collector and Python services will run on the Linux host, with
PostgreSQL in a container. A web API and frontend can be added as separate
services later. The Linux eBPF collector observes the host kernel and is not
currently packaged as a container.

## Technology stack

- **Current:** C, eBPF, libbpf, BPF CO-RE, BPF ring buffer, Clang/LLVM, bpftool,
  cJSON, POSIX threads, and Make.
- **Planned:** Python, PostgreSQL, psycopg, YAML detection rules, Unix domain
  sockets, Docker Compose, and a web interface.
- **Later:** Windows telemetry sources and a separate response component.

## Requirements

The current sensor requires Linux. Initial testing was performed on Arch Linux
with an x86-64 kernel; compatibility with other distributions and architectures
has not yet been verified.

The running kernel must provide:

- eBPF support, BPF ring buffers, and the helpers used by the sensor, including
  `bpf_get_current_task_btf`.
- Readable kernel BTF at `/sys/kernel/btf/vmlinux`.
- The `sched:sched_process_exec` tracepoint.
- Permission to load and attach tracing BPF programs. The documented launch
  command uses `sudo`; local kernel security policies may impose additional
  restrictions.

A minimum supported kernel version has not yet been established. CO-RE adapts
field accesses to compatible kernel layouts; it does not provide missing
kernel features.

Build dependencies:

- GCC with C11 atomics and POSIX threads support.
- Clang/LLVM with the BPF target.
- Make and bpftool.
- libbpf and cJSON, including development headers and linkable libraries.

On Arch Linux, install the dependencies with:

```bash
sudo pacman -S --needed base-devel clang llvm bpf libbpf cjson
```

The Arch package providing `bpftool` is named `bpf`. On other distributions,
package names and development-header packages differ.

## Event format

An event is shown below with indentation for readability. Actual output uses
one line per event.

```json
{
  "version": 1,
  "platform": "linux",
  "event_type": "process.exec",
  "monotonic_ns": "85369879736820",
  "process": {
    "pid": 329284,
    "tid": 329284,
    "parent_pid": 145474,
    "name_bytes_hex": "736c656570"
  },
  "user": {
    "uid": 1000,
    "gid": 1000
  }
}
```

- `pid` identifies the process; `tid` identifies the executing thread.
- `parent_pid` is the parent's process ID at collection time.
- `name_bytes_hex` stores the short task name as hexadecimal bytes. For example,
  `736c656570` decodes to `sleep`. This preserves names containing invalid UTF-8
  without producing invalid UTF-8 JSON. It is not an executable path or command
  line. The current name buffer holds up to 15 bytes plus a terminating zero.
- `monotonic_ns` comes from `bpf_ktime_get_ns()`: monotonic nanoseconds, excluding
  time spent suspended. It is not a Unix timestamp and cannot be directly
  interpreted as a calendar date or compared across host boots. It is encoded
  as a decimal string to preserve 64-bit integer precision through cJSON.
- UID and GID are Linux numeric identifiers; future Windows SIDs will be
  represented separately.

In Python, the encoded fields can be decoded with:

```python
raw_name = bytes.fromhex(event["process"]["name_bytes_hex"])
display_name = raw_name.decode("utf-8", errors="replace")
monotonic_ns = int(event["monotonic_ns"])
```

Keep the raw name bytes when storing evidence; the display string may contain
replacement characters. The current JSON shape is an initial format. Host,
boot, and event identifiers, calendar timestamps, and protocol compatibility
rules are still to be specified before persistent ingestion.

## Repository layout

```text
bpf/process.bpf.c       Linux exec tracepoint handler and ring buffer
collector/main.c        Worker startup, signal waiting, and shutdown coordination
collector/process.c     Process sensor lifecycle and event consumption
collector/json.c        Event serialization
includes/               Shared structures and collector interfaces
generated/              Generated headers, BPF object, and collector binary
Makefile                Build rules
```

## Current limitations

- Only successful exec events are collected. Exec replaces a program inside
  an existing process; fork/clone, exits, and failed exec attempts are not yet
  captured.
- Executable paths, arguments, and a process identity resistant to PID reuse
  are not yet collected. PID namespace mapping is not implemented.
- Ring buffer reservation and field-read failures can drop events. Loss
  counters and load tests are not implemented.
- Pending events are not explicitly drained during shutdown.
- Synchronous stdout output can block event consumption and delay shutdown
  if its destination stops accepting data. Durable delivery is not implemented.
- There is no Unix socket receiver, database storage, process tree,
  detection engine, web interface, Windows sensor, or automated response yet.
- No automated test suite or CI workflow is currently included in the repository.

## Roadmap

1. Complete and harden process collection: lifecycle events, richer exec data,
   loss accounting, shutdown draining, and reproducible tests.
2. Specify the event protocol and design the PostgreSQL telemetry schema.
3. Implement JSON delivery and a Python receiver that validates and persists
   events; run PostgreSQL through Docker Compose.
4. Build a separate Python analyzer with YAML rules, processing checkpoints,
   and stored alerts.
5. Add Linux network and filesystem sensors and correlate their events.
6. Add a web interface for event searches, process trees, and investigations.
7. Add Windows collection through the shared event model.
8. Extend toward centralized endpoint management and audited response actions.

## Development

This project is developed incrementally. GitHub issues describe upcoming work
and acceptance criteria. Commit messages follow
[Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/).

Before committing a change, review the diff, rebuild affected components, and
run the checks relevant to the change. Build artifacts, credentials, and real
endpoint telemetry should remain outside version control.

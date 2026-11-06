#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>
#include "events.h" 

char LICENSE[] SEC("license") = "GPL";

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 256*1024);
} events SEC(".maps");

SEC("tracepoint/sched/sched_process_exec")
int handle_exec(struct trace_event_raw_sched_process_exec *ctx) {
    struct event *event = bpf_ringbuf_reserve(&events, sizeof(struct event), 0);
    if (!event) {
        bpf_printk("Unable to reserve memory in ringbuf\n");
        return 0;
    }
    __builtin_memset(event, 0, sizeof(struct event));
    event->type = EVENT_PROCESS_EXEC;
    event->monotonic_ns = bpf_ktime_get_ns();

    __u64 pid_tgid = bpf_get_current_pid_tgid();
    event->tid = (__u32)pid_tgid;
    event->pid = (__u32)(pid_tgid >> 32);

    struct task_struct *task = bpf_get_current_task_btf();
    if (BPF_CORE_READ_INTO(&event->ppid, task, real_parent, tgid) != 0) {
        bpf_printk("Error writing data to ringbuf\n");
        bpf_ringbuf_discard(event, 0);
        return 0;
    }

    __u64 uid_gid = bpf_get_current_uid_gid();
    event->uid = (__u32)uid_gid;
    event->gid = (__u32)(uid_gid >> 32);
    
    if (bpf_get_current_comm(event->task, sizeof(event->task)) != 0) {
        bpf_printk("Error writing data to ringbuf\n");
        bpf_ringbuf_discard(event, 0);
        return 0;
    }
    bpf_ringbuf_submit(event, 0);
    return 0;
}

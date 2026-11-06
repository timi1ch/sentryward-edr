# Tools and flags
CC = gcc
CLANG = clang
BPFTOOL = bpftool

CPPFLAGS = -Igenerated -Iincludes
CFLAGS = -Wall -Wextra -Werror -g -pthread
BPF_CFLAGS = -target bpf -O2 -g
LDLIBS = -lbpf -lcjson

# Sources and generated files
GENERATED_DIR = generated
VMLINUX_H = $(GENERATED_DIR)/vmlinux.h
PROCESS_BPF_OBJ = $(GENERATED_DIR)/process.bpf.o
PROCESS_SKEL = $(GENERATED_DIR)/process.skel.h
COLLECTOR = $(GENERATED_DIR)/collector

COLLECTOR_SOURCES = \
	collector/main.c \
	collector/process.c \
	collector/json.c

COLLECTOR_HEADERS = \
	includes/collector.h \
	includes/process.h \
	includes/events.h \
	includes/json.h

# Build rules
.PHONY: all
.DELETE_ON_ERROR:

all: $(COLLECTOR)

$(GENERATED_DIR):
	mkdir -p $@

$(VMLINUX_H): /sys/kernel/btf/vmlinux | $(GENERATED_DIR)
	$(BPFTOOL) btf dump file $< format c > $@

$(PROCESS_BPF_OBJ): bpf/process.bpf.c $(VMLINUX_H) includes/events.h | $(GENERATED_DIR)
	$(CLANG) $(BPF_CFLAGS) $(CPPFLAGS) -c $< -o $@

$(PROCESS_SKEL): $(PROCESS_BPF_OBJ) | $(GENERATED_DIR)
	$(BPFTOOL) gen skeleton $< > $@

$(COLLECTOR): $(COLLECTOR_SOURCES) $(COLLECTOR_HEADERS) $(PROCESS_SKEL) | $(GENERATED_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(COLLECTOR_SOURCES) -o $@ $(LDLIBS)

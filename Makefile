SRCS = $(wildcard buffer/ccan/*/*.c) $(wildcard buffer/ccan/*/*/*.c) $(wildcard buffer_nofree/*.c) $(wildcard buffer_simple/*.c) $(wildcard mplite/*.c) $(wildcard shalloc/*.c)  $(wildcard buffer_slab/*.c) $(wildcard shalloc/interface/*.c)
HEADERS = $(wildcard include/*.h) $(wildcard include/common/alloc/*/*.h) $(wildcard include/common/alloc/*/*/*.h) $(wildcard buffer/ccan/*/*.h) $(wildcard buffer/ccan/*/*/*.h)

CFLAGS = -Wall -Wextra -O2 -I./buffer -I./include -fpic
LDFLAGS = -shared -O2
OBJS := $(SRCS:%.c=%.o)

all: shalloc.so

shalloc.so: $(OBJS) $(HEADERS)
	@echo "[LINK] $@"
	@$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c $(HEADERS)
	@echo "[CC] $<"
	@$(CC) -c -o $@ $< $(CFLAGS)

clean:
	@echo "[RM] shalloc.so"
	@rm shalloc.so
	@echo "[RM] $(OBJS)"
	@rm $(OBJS)

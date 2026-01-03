# compiler flags, (-MMD -MP track dependencies)
CC	:= clang
CFLAGS  := -fPIC -Wall -Wextra -mpku -MMD -MP -std=c23
LDFLAGS := -shared -ldl -lmimalloc

# project files
SRCS := runtime.c segment_heap.c bookkeeper.c stack.c
OBJS := $(SRCS:.c=.o)
EXE  := libruntime.so

# debug build
DBG_DIR    := debug
DBG_EXE    := $(DBG_DIR)/$(EXE)
DBG_OBJS   := $(addprefix $(DBG_DIR)/, $(OBJS))
DBG_CFLAGS := -ggdb3 -O0 -D_BOOKKEEPER_DEBUG
DBG_DEPS   := $(DBG_OBJS:.o=.d)

# release build
REL_DIR    := release
REL_EXE    := $(REL_DIR)/$(EXE)
REL_OBJS   := $(addprefix $(REL_DIR)/, $(OBJS))
REL_CFLAGS := -O3
REL_DEPS   := $(REL_OBJS:.o=.d)

all: debug release

# debug rules
debug: prep_dbg $(DBG_EXE)

$(DBG_EXE): $(DBG_OBJS)
	$(CC) $(CFLAGS) $(DBG_CFLAGS) -o $(DBG_EXE) $^ $(LDFLAGS)

$(DBG_DIR)/%.o: %.c
	$(CC) -c $(CFLAGS) $(DBG_CFLAGS) -o $@ $<
	
# release rules
release: prep_rel $(REL_EXE)

$(REL_EXE): $(REL_OBJS)
	$(CC) $(CFLAGS) $(REL_CFLAGS) -o $(REL_EXE) $^ $(LDFLAGS)

$(REL_DIR)/%.o: %.c
	$(CC) -c $(CFLAGS) $(REL_CFLAGS) -o $@ $<

# other rules
prep_dbg:
	@mkdir -p $(DBG_DIR)
	
prep_rel:
	@mkdir -p $(REL_DIR)

remake: clean all

clean:
	rm -rf $(DBG_DIR) $(REL_DIR)

.PHONY: all clean debug release prep_dbg prep_rel remake

# include the .d makefiles. The - at the front suppresses the errors of missing
# Makefiles. Initially, all the .d files will be missing, and we don't want
# those errors to show up.
-include $(DBG_DEPS)
-include $(REL_DEPS)

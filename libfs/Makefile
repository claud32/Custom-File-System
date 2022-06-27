# Target library
lib 	:= libfs.a
objs	:= disk.o fs.o

CC	:= gcc
CFLAGS	:= -Wall -Wextra -Werror -MMD
CFLAGS	+= -g

AR	:= ar
AFLAGS	:= rcs

ifneq ($(V),1)
	Q = @
endif

all: $(lib)

#Dep tracking
deps	:= $(patsubst %.o,%.d,$(objs))
-include $(deps)


libfs.a: $(objs)
	@echo "AR $@"
	$(Q)$(AR) $(AFLAGS) $@ $^

%.o: %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo "CLEAN"
	$(Q)rm -f $(lib) $(objs) $(deps) *.d

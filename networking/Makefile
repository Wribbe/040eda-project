dir_source := src
dir_objects := objects
dir_executables := executables

CC = gcc

vpath %.c $(dir_source)
vpath %.o $(dir_objects)

conditional_mkdir = \
	@if [ ! -d $(1) ]; then \
		mkdir $(1); \
	fi

sources := $(foreach filename,$(wildcard $(dir_source)/*.c),$(notdir $(filename)))
objects := $(sources:.c=.o)
executables := $(foreach filename,$(sources:.c=),$(dir_executables)/$(filename))

my_flags = -Wall -Wextra -pedantic -std=gnu11 -Wwrite-strings -g
libraries = -lpthread
FLAGS = $(libraries) $(my_flags) $(CFLAGS)

.PHONY: all mkdirs clean
all: $(executables)

mkdirs:
	$(call conditional_mkdir,$(dir_objects))
	$(call conditional_mkdir,$(dir_executables))

clean:
	rm -rf $(dir_objects)
	rm -rf $(dir_executables)
	rm -rf vgcore.*

%.o : %.c | mkdirs
	$(CC) -c $(FLAGS) $< -o $(dir_objects)/$@

$(dir_executables)/% : %.o
	$(CC) $(FLAGS) $(dir_objects)/$< -o $@

dir_source := src
dir_objects := objects
dir_executables := executables

# Embedded compiler variables.
emb_comp_base_dir := $(shell pwd)/comptools-mips-r12_1.2-0/comptools-mips-r12_1.2-0_amd64/mipsisa32r2el/r12
emb_bin := $(emb_comp_base_dir)/bin

CC := gcc

vpath %.c $(dir_source)
vpath %.o $(dir_objects)

conditional_mkdir = \
	@if [ ! -d $(1) ]; then \
		mkdir $(1); \
	fi

sources := $(foreach filename,$(wildcard $(dir_source)/*.c),$(notdir $(filename)))
objects := $(sources:.c=.o)
executables := $(foreach filename,$(sources:.c=),$(dir_executables)/$(filename))

my_flags := -Wall -Wextra -pedantic -std=gnu99 -Wwrite-strings -g
libraries := -lpthread -pthread
FLAGS := $(libraries) $(my_flags) $(CFLAGS) -I.
embed_include := -I$(emb_comp_base_dir)/mipsisa32r2el-axis-linux-gnu/sys-include -I.

.PHONY: all mkdirs clean embed change_to_embedded
all: $(executables)

embed: change_to_embedded $(executables)

change_to_embedded:
	$(eval CC = $(emb_bin)/mipsisa32r2el-axis-linux-gnu-gcc)
	$(eval FLAGS = $(libraries) $(my_flags) $(CFLAGS) $(embed_include))

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

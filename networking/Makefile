dir_source := src/executables
dir_objects = objects
dir_emb_objects := $(dir_objects)/embedded
dir_executables = executables
dir_emb_executables := $(dir_executables)/embedded
dir_libfolders := ./lib ./lib/embedded
dir_include := include
dir_images := data/images

# Embedded compiler variables.
emb_comp_base_dir := $(shell pwd)/data/comptools-mips-r12_1.2-0/comptools-mips-r12_1.2-0_amd64/mipsisa32r2el/r12
emb_bin := $(emb_comp_base_dir)/bin

CC := gcc

vpath %.c $(dir_source)
vpath %.o $(dir_objects)

conditional_mkdir = \
	@if [ ! -d $(1) ]; then \
		mkdir $(1); \
	fi

cameras := 1 2 3 4 5 6
push_cameras = \
	@for camera in $(cameras) ; do \
		./push_to_camera.sh $$camera ; \
	done

sources = $(foreach filename,$(wildcard $(dir_source)/*.c),$(notdir $(filename)))
objects = $(sources:.c=.o)
executables = $(foreach filename,$(sources:.c=),$(dir_executables)/$(filename))
emb_executables = $(foreach filename,$(sources:.c=),$(dir_emb_executables)/$(filename))

libdir_flags := $(foreach lib_folder,$(dir_libfolders),-L$(lib_folder))
libraries := -lpthread -pthread -lcapture -ljpeg -lm
embed_libraries := -lpthread -pthread -lrapp -lcapture -lm
embed_include := -I$(emb_comp_base_dir)/mipsisa32r2el-axis-linux-gnu/sys-include
linker_flags := -Wl,-rpath=./lib

my_flags := -Wall -Wextra -pedantic -std=gnu99 -Wwrite-strings -g $(libdir_flags) -I./include
FLAGS := $(my_flags) $(CFLAGS) $(libraries)

# Commands that add/remove files.
.PHONY: all embed emb both clean
# Utility commands.
.PHONY:	embed_mkdirs mkdirs change_to_embedded serv push pushall

all: $(executables)

embed: change_to_embedded $(emb_executables)
emb: embed

both: all embed

push: embed
	./push_to_camera.sh $(cam)

push_all: embed
	$(call push_cameras)

serv: both
	./$(dir_executables)/eda040_server

cli: both
	./$(dir_executables)/eda040_client localhost

change_to_embedded:
	$(eval CC = $(emb_bin)/mipsisa32r2el-axis-linux-gnu-gcc)
	$(eval FLAGS = $(my_flags) $(CFLAGS) $(embed_include) $(embed_libraries))

mkdirs:
	$(call conditional_mkdir,$(dir_objects))
	$(call conditional_mkdir,$(dir_executables))
	$(call conditional_mkdir,$(dir_images))

embed_mkdirs:
	$(call conditional_mkdir,$(dir_emb_objects))
	$(call conditional_mkdir,$(dir_emb_executables))

clean:
	rm -rf $(dir_objects)
	rm -rf $(dir_executables)
	rm -rf $(dir_images)
	rm -rf vgcore.*

# Regular executables.
%.o : %.c | mkdirs
	$(CC) $< -c $(FLAGS) -o $(dir_objects)/$@
	@echo

$(dir_executables)/% : %.o
	$(CC) $(dir_objects)/$< $(linker_flags) $(FLAGS) -o $@
	@echo

# Embedded executables.
$(dir_emb_objects)/%.o : %.c | mkdirs embed_mkdirs
	$(CC) $< -c $(FLAGS) -o $@
	@echo

$(dir_emb_executables)/% : objects/embedded/%.o
	$(CC) $< $(FLAGS) $(libdir_flags) -o $@
	@echo


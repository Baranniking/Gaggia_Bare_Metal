# Headless PC build of the UI (screenshots / logic testing).
LVGL ?= ../../lvgl
CC   ?= gcc
CFLAGS = -O1 -g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
         -DLV_CONF_INCLUDE_SIMPLE -DLV_LVGL_H_INCLUDE_SIMPLE -DGAGGIA_SIM=1 \
         -I. -I../src -I../src/ui -I../src/app -I../src/proto -I$(LVGL) -I$(LVGL)/src
LVGL_SRC := $(shell find $(LVGL)/src -name '*.c')
APP_SRC  := $(wildcard ../src/ui/*.c) $(wildcard ../src/app/*.c) $(wildcard ../src/proto/*.c) pc_main.c
LVGL_OBJ := $(patsubst $(LVGL)/%.c,build/lvgl/%.o,$(LVGL_SRC))
APP_OBJ  := $(patsubst %.c,build/app/%.o,$(notdir $(APP_SRC)))
vpath %.c ../src/ui ../src/app ../src/proto .

gaggia_sim: $(LVGL_OBJ) $(APP_OBJ)
	$(CC) -o $@ $^ -lm
build/lvgl/%.o: $(LVGL)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -w -c $< -o $@
build/app/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -rf build gaggia_sim

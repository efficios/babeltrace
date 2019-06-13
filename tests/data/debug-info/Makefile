BUILD_ID_PREFIX=cd
BUILD_ID_SUFFIX=d98cdd87f7fe64c13b6daad553987eafd40cbb
BUILD_ID=$(BUILD_ID_PREFIX)$(BUILD_ID_SUFFIX)

BUILD_DIR ?= build

OBJS=$(BUILD_DIR)/dwarf_full/libhello_so \
     $(BUILD_DIR)/elf_only/libhello_so \
     $(BUILD_DIR)/build_id/libhello_so \
     $(BUILD_DIR)/debug_link/libhello_so

all: $(OBJS)

%.o: %.c
	$(CC) -gdwarf -fdebug-prefix-map=$(CURDIR)=. -fPIC -c -I. -o $@ $<

# Master copy: ELF with DWARF and build-id
$(BUILD_DIR)/dwarf_full/libhello_so: tp.o libhello.o
	mkdir -p $(@D)
	$(CC) -shared -gdwarf -llttng-ust -ldl -Wl,-soname,libhello.so -Wl,--build-id=0x$(BUILD_ID) -o $@ $^

# ELF only, no debug symbols, no build-d
$(BUILD_DIR)/elf_only/libhello_so: $(BUILD_DIR)/dwarf_full/libhello_so
	mkdir -p $(@D)
	objcopy -g $< $@.tmp
	objcopy --remove-section=.note.gnu.build-id $@.tmp
	mv $@.tmp $@

# ELF with external build-id DWARF
$(BUILD_DIR)/build_id/libhello_so: $(BUILD_DIR)/dwarf_full/libhello_so
	mkdir -p $(@D)/.build-id/$(BUILD_ID_PREFIX)
	objcopy --only-keep-debug $< $(@D)/.build-id/$(BUILD_ID_PREFIX)/$(BUILD_ID_SUFFIX).debug
	objcopy -g $< $@

# ELF with external debug link DWARF
$(BUILD_DIR)/debug_link/libhello_so: $(BUILD_DIR)/dwarf_full/libhello_so
	mkdir -p $(@D)
	objcopy --remove-section=.note.gnu.build-id $< $@.tmp
	objcopy --only-keep-debug $@.tmp $(@D)/libhello_so.debug
	objcopy -g $@.tmp
	cd $(@D) && objcopy --add-gnu-debuglink=libhello_so.debug $(@F).tmp
	mv $@.tmp $@

clean:
	rm -f *.o

dist-clean: clean
	rm -rf $(BUILD_DIR)

.PHONY: all clean dist-clean

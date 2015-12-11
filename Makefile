BUILD_DIR=build_dir
STATIC_LIB=$(BUILD_DIR)/libusys.a
SHARED_LIB=$(BUILD_DIR)/libusys.so 
SOURCES=\
	src/uloop_process.c \
	src/uloop_timeout.c \
	src/runqueue.c \
	src/ulog.c \
	src/uloop.c \
	src/usock.c \
	src/ustream-fd.c \
	src/ustream.c 

INSTALL_PREFIX=/usr

OBJECTS=$(addprefix $(BUILD_DIR)/,$(patsubst %.c,%.o,$(SOURCES)))

CFLAGS+=-I. -fPIC -Wall -Werror -std=gnu99

all: $(BUILD_DIR) $(STATIC_LIB) $(SHARED_LIB) 

$(BUILD_DIR): 
	mkdir -p $(BUILD_DIR)

$(SHARED_LIB): $(OBJECTS) 
	$(CC) -shared -Wl,--no-undefined -o $@ $^ -ldl -lutype

$(STATIC_LIB): $(OBJECTS)
	$(AR) rcs -o $@ $^

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $^ -o $@

install: 
	mkdir -p $(INSTALL_PREFIX)/include/libusys
	cp -R src/*.h $(INSTALL_PREFIX)/include/libusys/
	mkdir -p $(INSTALL_PREFIX)/lib/
	cp -R $(SHARED_LIB) $(STATIC_LIB) $(INSTALL_PREFIX)/lib/

clean: 
	rm -rf build_dir
	rm -f examples/*.o

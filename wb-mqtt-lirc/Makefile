BIN = wb-mqtt-lirc

CXXFLAGS += -Wall -Wextra -std=c++0x #-ggdb -O0
LDFLAGS += -lm -lmosquittopp -ljsoncpp -lwbmqtt -pthread -llirc_client

ifeq ($(DEB_TARGET_ARCH),armel)
CROSS_COMPILE=arm-linux-gnueabi-
endif

CXX=$(CROSS_COMPILE)g++

CXX_PATH := $(shell which $(CROSS_COMPILE)g++-4.7)
ifneq ($(CXX_PATH),)
	CXX=$(CROSS_COMPILE)g++-4.7
endif

.PHONY: all install clean

all: $(BIN)

$(BIN): $(BIN).cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $< -o $@

install: $(BIN) config.json
	install -d $(DESTDIR)/etc
	install -d $(DESTDIR)/usr/bin
	install -d $(DESTDIR)/usr/share/wb-mqtt-confed/schemas
	install -m 0755 $(BIN) $(DESTDIR)/usr/bin/
	install -m 0644 config.json $(DESTDIR)/etc/$(BIN).conf
	install -m 0644 $(BIN).schema.json $(DESTDIR)/usr/share/wb-mqtt-confed/schemas/$(BIN).schema.json

clean:
	rm -f $(BIN)

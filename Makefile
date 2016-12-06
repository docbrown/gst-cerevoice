CEREVOICE_SDK ?= /opt/cerevoice_sdk
CEREVOICE_SHARED ?= 1
DEBUG ?= 1

# A literal space.
SPACE :=
SPACE +=

# Joins elements of the list in arg 2 with the separator in arg 1.
join-with = $(subst $(SPACE),$1,$(strip $2))

CEREVOICE_LIB_DIRS  = $(CEREVOICE_SDK)/cerevoice/lib
CEREVOICE_LIB_DIRS += $(CEREVOICE_SDK)/cerevoice_eng/lib
CEREVOICE_LIB_DIRS += $(CEREVOICE_SDK)/cerevoice_pmod/lib
CEREVOICE_LIB_DIRS += $(CEREVOICE_SDK)/cerehts/lib

PLUGIN   = gstcerevoice
PKGS     = gstreamer-1.0 gstreamer-audio-1.0
CFLAGS   = -Wall -Werror
CFLAGS  += -I$(CEREVOICE_SDK)/cerevoice_eng/include
CFLAGS  += $(shell pkg-config $(PKGS) --cflags)
LDFLAGS  = -shared -fPIC
LDFLAGS += $(foreach dir,$(CEREVOICE_LIB_DIRS),-L$(dir))
ifeq ($(CEREVOICE_SHARED),1)
LDFLAGS += -lcerevoice_eng_shared -lcerevoice_shared
else
LDFLAGS += -lcerevoice_eng -lcerevoice_pmod -lcerehts -lcerevoice -lstdc++
endif
LDFLAGS += $(shell pkg-config $(PKGS) --libs)

ifeq ($(DEBUG),1)
	CFLAGS += -g
endif

TARGET = lib$(PLUGIN).so

.PHONY: all
all: $(TARGET)

$(TARGET): $(PLUGIN).c
	$(CC) -o $@ $(CFLAGS) $< $(LDFLAGS)

.PHONY: check
check: $(TARGET)
	echo 'How are you today?' | \
	  LD_LIBRARY_PATH=$(call join-with,:,$(CEREVOICE_LIB_DIRS)) \
	  gst-launch-1.0 --gst-plugin-path=. --gst-debug=cerevoice:9 \
	  fdsrc fd=0 ! cerevoice voice_file=heather.voice license_file=heather.lic \
	  ! audioconvert ! autoaudiosink

.PHONY: clean
clean:
	rm $(TARGET)

.PHONY: help
help:
	@echo "CereVoice Text-to-Speech Plugin for GStreamer Makefile"
	@echo
	@echo "Usage: $(MAKE) [options] [target]"
	@echo
	@echo "Options:"
	@echo
	@echo "  CEREVOICE_SDK=path   - Path to the CereVoice SDK directory"
	@echo "                         (default: /opt/cerevoice_sdk)"
	@echo "  CEREVOICE_SHARED=0,1 - Whether to link against shared CereVoice"
	@echo "                         libraries (default: 1)"
	@echo "  DEBUG=0,1            - Build with debug information (default: 1)"
	@echo
	@echo "Targets:"
	@echo
	@echo "  all   - Build the plugin library (default)"
	@echo "  check - Run a simple test pipeline with gst-launch"
	@echo "  clean - Remove all build artifacts"
	@echo "  help  - Show this message"
	@echo

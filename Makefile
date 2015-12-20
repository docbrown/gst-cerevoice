CEREVOICE_SDK ?= /opt/cerevoice
DEBUG ?= 1

PLUGIN   = gstcerevoice
PKGS     = gstreamer-1.0 gstreamer-audio-1.0
CFLAGS   = -Wall -Werror -DPACKAGE=\"$(PLUGIN)\"
CFLAGS  += -I$(CEREVOICE_SDK)/cerevoice_eng/include
CFLAGS  += $(shell pkg-config $(PKGS) --cflags)
LDFLAGS  = -shared -fPIC
LDFLAGS += -L$(CEREVOICE_SDK)/cerevoice/lib -L$(CEREVOICE_SDK)/cerevoice_eng/lib
LDFLAGS += -L$(CEREVOICE_SDK)/cerevoice_pmod/lib -L$(CEREVOICE_SDK)/cerehts/lib
LDFLAGS += -lcerevoice_eng -lcerevoice_pmod -lcerehts -lcerevoice -lstdc++
LDFLAGS += $(shell pkg-config $(PKGS) --libs)

ifeq ($(DEBUG),1)
	CFLAGS += -g
endif

ifeq ($(OS),Windows_NT)
	TARGET = $(PLUGIN).dll
else
	TARGET = $(PLUGIN).so
endif

all: $(TARGET)

$(TARGET): $(PLUGIN).c
	$(CC) -o $@ $(CFLAGS) $< $(LDFLAGS)

check: $(TARGET)
	echo 'How are you today?' | gst-launch-1.0 --gst-plugin-path=. \
		--gst-debug=cerevoice:9 fdsrc fd=0 \
		! cerevoice voice_file=heather.voice license_file=heather.lic \
		! audioconvert ! autoaudiosink

clean:
	rm $(TARGET)


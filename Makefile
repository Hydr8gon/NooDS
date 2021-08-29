NAME     := noods
BUILD    := build
SOURCES  := src src/common src/desktop
ARGS     := -Ofast -flto -std=c++11 #-DDEBUG
LIBS     := $(shell pkg-config --libs portaudio-2.0)
INCLUDES := $(shell pkg-config --cflags portaudio-2.0)

APPNAME := NooDS
DESTDIR ?= /usr

ifeq ($(OS),Windows_NT)
  ARGS += -static -DWINDOWS
  LIBS += $(shell wx-config-static --libs --gl-libs) -lole32 -lsetupapi -lwinmm
  INCLUDES += $(shell wx-config-static --cxxflags)
else
  LIBS += $(shell wx-config --libs --gl-libs)
  INCLUDES += $(shell wx-config --cxxflags)
  ifeq ($(shell uname -s),Darwin)
    ARGS += -DMACOS
  else
    ARGS += -no-pie
    LIBS += -lGL
  endif
endif

CPPFILES := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
HFILES   := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.h))
OFILES   := $(patsubst %.cpp,$(BUILD)/%.o,$(CPPFILES))

ifeq ($(OS),Windows_NT)
  OFILES += $(BUILD)/icon.o
endif

all: $(NAME)

ifneq ($(OS),Windows_NT)
ifeq ($(uname -s),Darwin)

install: $(NAME)
	./mac-bundle.sh
	cp -r $(APPNAME).app /Applications/

uninstall:
	rm -rf /Applications/$(APPNAME).app

else

install: $(NAME)
	install -Dm755 $(NAME) "$(DESTDIR)/bin/$(NAME)"
	install -Dm644 icon/icon.xpm "$(DESTDIR)/share/icons/hicolor/64x64/apps/$(NAME).xpm"
	install -Dm644 $(NAME).desktop "$(DESTDIR)/share/applications/$(NAME).desktop"

uninstall: 
	rm -f "$(DESTDIR)/bin/$(NAME)"
	rm -f "$(DESTDIR)/share/applications/$(NAME).desktop"
	rm -f "$(DESTDIR)/share/icons/hicolor/64x64/apps/$(NAME).xpm"

endif
endif

$(NAME): $(OFILES)
	g++ -o $@ $(ARGS) $^ $(LIBS)

$(BUILD)/%.o: %.cpp $(HFILES) $(BUILD)
	g++ -c -o $@ $(ARGS) $(INCLUDES) $<

$(BUILD)/icon.o:
	windres icon/icon.rc $@

$(BUILD):
	for dir in $(SOURCES); \
	do \
	mkdir -p $(BUILD)/$$dir; \
	done

clean:
	rm -rf $(BUILD)
	rm -f $(NAME)

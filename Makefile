NAME    := noods
BUILD   := build
SOURCES := src src/common src/desktop
ARGS    := -Ofast -flto -std=c++11 #-DDEBUG
LIBS    := -lportaudio

APPNAME      ?= NooDS
ORGANIZATION ?= com.github.hydr8gon
DESTDIR      ?= /usr/local

ifeq ($(OS),Windows_NT)
  ARGS += -static -DWINDOWS
  LIBS += `wx-config-static --cxxflags --libs --gl-libs` -lole32 -lsetupapi -lwinmm
else
  LIBS += `wx-config --cxxflags --libs --gl-libs`
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
	rm -r /Applications/$(APPNAME).app

else

install: $(NAME)
	install -Dm755 $(NAME) "$(DESTDIR)/bin/$(NAME)"
	install -Dm644 icon/icon.xpm "$(DESTDIR)/share/icons/hicolor/64x64/apps/$(ORGANIZATION).$(APPNAME).xpm"
	install -Dm644 pkg/$(ORGANIZATION).$(APPNAME).appdata.xml "$(DESTDIR)/share/metainfo/$(ORGANIZATION).$(APPNAME).appdata.xml"
	install -Dm644 pkg/$(ORGANIZATION).$(APPNAME).desktop "$(DESTDIR)/share/applications/$(ORGANIZATION).$(APPNAME).desktop"

uninstall: 
	-rm "$(DESTDIR)/bin/$(NAME)"
	-rm "$(DESTDIR)/share/metainfo/$(ORGANIZATION).$(APPNAME).appdata.xml"
	-rm "$(DESTDIR)/share/applications/$(ORGANIZATION).$(APPNAME).desktop"
	-rm "$(DESTDIR)/share/icons/hicolor/64x64/apps/$(ORGANIZATION).$(APPNAME).xpm"

endif
endif

$(NAME): $(OFILES)
	g++ -o $@ $(ARGS) $^ $(LIBS)

$(BUILD)/%.o: %.cpp $(HFILES) $(BUILD)
	g++ -c -o $@ $(ARGS) $< $(LIBS)

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

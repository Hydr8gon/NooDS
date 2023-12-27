NAME := noods
BUILD := build
SRCS := src src/common src/desktop
ARGS := -Ofast -flto -std=c++11 -DUSE_GL_CANVAS #-DDEBUG
LIBS := $(shell pkg-config --libs portaudio-2.0)
INCS := $(shell pkg-config --cflags portaudio-2.0)

APPNAME := NooDS
PKGNAME := com.hydra.noods
DESTDIR ?= /usr

ifeq ($(OS),Windows_NT)
  ARGS += -static -DWINDOWS
  LIBS += $(shell wx-config-static --libs --gl-libs) -lole32 -lsetupapi -lwinmm
  INCS += $(shell wx-config-static --cxxflags)
else
  LIBS += $(shell wx-config --libs --gl-libs)
  INCS += $(shell wx-config --cxxflags)
  ifeq ($(shell uname -s),Darwin)
    ARGS += -DMACOS
    LIBS += -headerpad_max_install_names
  else
    ARGS += -no-pie
    LIBS += -lGL
  endif
endif

CPPFILES := $(foreach dir,$(SRCS),$(wildcard $(dir)/*.cpp))
HFILES := $(foreach dir,$(SRCS),$(wildcard $(dir)/*.h))
OFILES := $(patsubst %.cpp,$(BUILD)/%.o,$(CPPFILES))

ifeq ($(OS),Windows_NT)
  OFILES += $(BUILD)/icon-windows.o
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

flatpak:
	flatpak-builder --repo=repo --force-clean build-flatpak $(PKGNAME).yml
	flatpak build-bundle repo $(NAME).flatpak $(PKGNAME)

flatpak-clean:
	rm -rf .flatpak-builder
	rm -rf build-flatpak
	rm -rf repo
	rm -f $(NAME).flatpak

install: $(NAME)
	install -Dm755 $(NAME) "$(DESTDIR)/bin/$(NAME)"
	install -Dm644 $(PKGNAME).desktop "$(DESTDIR)/share/applications/$(PKGNAME).desktop"
	install -Dm644 icon/icon-linux.png "$(DESTDIR)/share/icons/hicolor/64x64/apps/$(PKGNAME).png"

uninstall: 
	rm -f "$(DESTDIR)/bin/$(NAME)"
	rm -f "$(DESTDIR)/share/applications/$(PKGNAME).desktop"
	rm -f "$(DESTDIR)/share/icons/hicolor/64x64/apps/$(PKGNAME).png"

endif
endif

$(NAME): $(OFILES)
	g++ -o $@ $(ARGS) $^ $(LIBS)

$(BUILD)/%.o: %.cpp $(HFILES) $(BUILD)
	g++ -c -o $@ $(ARGS) $(INCS) $<

$(BUILD)/icon-windows.o:
	windres $(shell wx-config-static --cppflags) icon/icon-windows.rc $@

$(BUILD):
	for dir in $(SRCS); do mkdir -p $(BUILD)/$$dir; done

android-bundle:
	git apply src/android/play-store.patch
	./gradlew bundle
	git apply -R src/android/play-store.patch
	jarsigner -keystore keystore.jks -signedjar noods.aab build-android/outputs/bundle/release/android-release.aab keystore

android:
	./gradlew assembleDebug

switch:
	$(MAKE) -f Makefile.switch

vita:
	$(MAKE) -f Makefile.vita

clean:
	if [ -d "build-android" ]; then ./gradlew clean; fi
	if [ -d "build-switch" ]; then $(MAKE) -f Makefile.switch clean; fi
	if [ -d "build-vita" ]; then $(MAKE) -f Makefile.vita clean; fi
	rm -rf $(BUILD)
	rm -f $(NAME)

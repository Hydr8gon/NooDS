NAME    := noods
BUILD   := build
SOURCES := src src/common src/desktop
ARGS    := -Ofast -flto -std=c++11 #-DDEBUG
LIBS    := -lportaudio

ifeq ($(OS),Windows_NT)
ARGS += -static
LIBS += `wx-config-static --cxxflags --libs --gl-libs` -lole32 -lsetupapi -lwinmm
else
ARGS += -no-pie
LIBS += `wx-config --cxxflags --libs --gl-libs`
ifneq ($(shell uname -s),Darwin)
LIBS += -lGL
endif
endif

CPPFILES := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
HFILES   := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.h))
OFILES   := $(patsubst %.cpp,$(BUILD)/%.o,$(CPPFILES))

ifeq ($(OS),Windows_NT)
OFILES += $(BUILD)/icon.o
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

NAME    := noods
SOURCES := src src/desktop
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

$(NAME): $(CPPFILES) $(HFILES)
	g++ -o $@ $(ARGS) $(CPPFILES) $(LIBS)

clean:
	rm -f $(NAME)

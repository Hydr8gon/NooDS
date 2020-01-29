NAME	:= noods
SOURCES := src src/desktop

ifeq ($(OS),Windows_NT)
ARGS    := -Ofast -flto -std=c++11 -static #-DDEBUG
LIBS    := `wx-config-static --cxxflags --libs` -lportaudio -lole32 -lsetupapi -lwinmm
else
ARGS    := -Ofast -flto -std=c++11 -no-pie #-DDEBUG
LIBS    := `wx-config --cxxflags --libs` -lportaudio
endif

CPPFILES := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
HFILES   := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.h))

$(NAME): $(CPPFILES) $(HFILES)
	g++ -o $@ $(ARGS) $(CPPFILES) $(LIBS)

clean:
	rm -f $(NAME)

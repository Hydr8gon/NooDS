NAME	:= noods
SOURCES := src src/desktop
ARGS    := -Ofast -no-pie
LIBS    := -lpthread `wx-config --cxxflags --libs`

CPPFILES := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
HFILES   := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.h))

$(NAME): $(CPPFILES) $(HFILES)
	g++ -o $@ $(ARGS) $(LIBS) $(CPPFILES)

clean:
	rm -f $(NAME)

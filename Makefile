NAME	:= noods
SOURCES := src src/desktop
ARGS    := -Ofast -no-pie -std=c++11 #-DDEBUG
LIBS    := `wx-config --cxxflags --libs`

CPPFILES := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
HFILES   := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.h))

$(NAME): $(CPPFILES) $(HFILES)
	g++ -o $@ $(ARGS) $(CPPFILES) $(LIBS)

clean:
	rm -f $(NAME)

NAME	:= noids
SOURCES := src src/desktop
ARGS    := -O2
LIBS    := -lpthread -lglut -lGL

CPPFILES := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.cpp))
HFILES   := $(foreach dir,$(SOURCES),$(wildcard $(dir)/*.h))

$(NAME): $(CPPFILES) $(HFILES)
	g++ -o $@ $(ARGS) $(LIBS) $(CPPFILES)

clean:
	rm -f $(NAME)

CC=gcc
CFLAGS=-std=c99 -Wall -c -Wc++-compat -O3 -fextended-identifiers
LIBS=-lX11 -lGL -lpng
DEMOS=\
	Spiral1 \
	Spiral2 \
	Spiral3 \
	Spiral4 \
	Spiral5 \
	Spiral6 \
	Spiral7a Spiral7b \
	Spiral8a Spiral8b \

SHARED=pez.o bstrlib.o pez.linux.o
PREFIX=

run: Spiral7b
	./Spiral7b

all: $(DEMOS)

define DEMO_RULE
$(1): $(PREFIX)$(1).o $(PREFIX)$(1).glsl $(SHARED)
	$(CC) $(PREFIX)$(1).o $(SHARED) -o $(1) $(LIBS)
endef

$(foreach demo,$(DEMOS),$(eval $(call DEMO_RULE,$(demo))))

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o $(DEMOS)

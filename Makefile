CC=g++
CFLAGS=-Wall -c -O3
LIBS=-lX11 -lGL -lpng

MAINCPP=Fluid3d.o Utility.o
CSHARED=pez.o pez.linux.o bstrlib.o
SHADERS=Fluid.glsl Raycast.glsl Light.glsl

run: Fluid
	./Fluid

Fluid: $(MAINCPP) $(CSHARED) $(SHADERS)
	$(CC) $(MAINCPP) $(CSHARED) -o Fluid $(LIBS)

.c.o:
	$(CC) $(CFLAGS) $< -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -rf *.o Fluid

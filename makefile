OBJS = main.o FrameGen.o
DEPS = FrameGen.h
CC = g++
CFLAGS = -std=c++11
LFLAGS = -std=c++11 -lz

%.o: %.cpp $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

FrameGen: $(OBJS)
	$(CC) $(LFLAGS) $^ -o $@

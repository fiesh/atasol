TARGET=atasol

OBJECTS=main.o

CXXFLAGS = -std=c++14 -march=native -Werror -flto -pipe -fvisibility=hidden -Wall -Wextra -Wformat=2 -Winit-self -Wshadow -Wcast-align -Wunused -pedantic -Wswitch-enum -Wuninitialized -Wtrampolines -Wcast-align -Wconversion -Wlogical-op -Wmissing-declarations -Wredundant-decls -Wvector-operation-performance -Wdisabled-optimization -Wcast-qual -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual -Wuseless-cast

# Debug flags
# CXXFLAGS += -ggdb -Og -ffast-math

# Release flags
CXXFLAGS += -DNDEBUG -Ofast

# Profiling flags
# CXXFLAGS += -DNDEBUG -ggdb -pg -Ofast

all: ${TARGET}

${TARGET}: ${OBJECTS}
	${CXX} -o ${TARGET} ${CXXFLAGS} ${OBJECTS} 

clean:
	-rm -f $(OBJECTS)

main.o: main.cpp solver.hpp

CXX      := g++
CXXFLAGS := -std=c++11 -Wall -Wextra
TARGET   := final
SOURCES  := final.cpp

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
	LDLIBS := -framework OpenGL -framework GLUT
else
	LDLIBS := -lglut -lGLU -lGL -lm
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDLIBS)

clean:
	rm -f $(TARGET) *.o

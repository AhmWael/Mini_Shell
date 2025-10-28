## you have command.cc command.h tokenizer.cc tokenizer.h you need compile them and link
## them to one object file called  : "myshell" 
## so we should run make , that will create our object myshell ,
## ./myshell should run your application 

# Name of the final executable
TARGET = myshell

# Source files
SRCS = command.cc tokenizer.cc

# Object files (replace .cc with .o)
OBJS = $(SRCS:.cc=.o)

# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -g -std=c++20

# Default target
all: $(TARGET)

# Linking
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# Compilation rule
%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f $(OBJS) $(TARGET)

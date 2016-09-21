CXXFLAGS:=-DNDEBUG -O2 -g -std=c++11 -Wall -Wextra -Weffc++ -pedantic

PROGNAME:= watcher

.PHONY: all clean

all: $(PROGNAME)

$(PROGNAME): main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	@rm -f $(PROGNAME) %.o

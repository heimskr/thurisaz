EMIT	:= clang++ -S -emit-llvm -std=c++2a
SOURCES	:= $(shell find src/*.cpp)


.PHONY: clean

%.ll: %.cpp
	$(EMIT) $< -o $@

all: $(SOURCES:.cpp=.ll)

clean:
	rm -f src/*.ll src/**/*.ll

DEPFILE  = .dep
DEPTOKEN = "\# MAKEDEPENDS"
DEPFLAGS = -f $(DEPFILE) -s $(DEPTOKEN)

depend:
	@ echo $(DEPTOKEN) > $(DEPFILE)
	makedepend $(DEPFLAGS) -- $(COMPILER) $(CFLAGS) -- $(SOURCES) 2>/dev/null
	@ rm $(DEPFILE).bak

sinclude $(DEPFILE)

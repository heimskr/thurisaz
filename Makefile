EMIT	:= clang++ -S -emit-llvm -std=c++2a
SOURCES	:= $(shell find src/*.cpp)
LLVMIR	:= $(SOURCES:.cpp=.ll)
OUTPUT	:= Thurisaz.ll

.PHONY: clean

%.ll: %.cpp
	$(EMIT) -Iinclude $< -o $@

all: $(OUTPUT)

clean:
	rm -f src/*.ll src/**/*.ll

$(OUTPUT): $(SOURCES:.cpp=.ll)
	llvm-link -S -o $@ $(LLVMIR)

DEPFILE  = .dep
DEPTOKEN = "\# MAKEDEPENDS"
DEPFLAGS = -f $(DEPFILE) -s $(DEPTOKEN)

depend:
	@ echo $(DEPTOKEN) > $(DEPFILE)
	makedepend $(DEPFLAGS) -- $(COMPILER) $(CFLAGS) -- $(SOURCES) 2>/dev/null
	@ rm $(DEPFILE).bak

sinclude $(DEPFILE)

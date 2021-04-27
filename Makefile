EMIT		:= clang++ -S -emit-llvm -std=c++2a -nostdlib -fno-builtin -Iinclude -Iinclude/libcxx -fno-exceptions -fno-rtti
SOURCES		:= $(shell find src/*.cpp src/**/*.cpp)
LLVMIR		:= $(SOURCES:.cpp=.ll)
LINKED		:= Thurisaz.ll
WASM		:= Thurisaz.wasm
OUTPUT		:= Thurisaz.why
LLVMLINK	?= llvm-link

.PHONY: linked wasm clean

%.ll: %.cpp
	$(EMIT) -Iinclude $< -o $@

all: $(OUTPUT)

$(OUTPUT): $(WASM)
	wasmc $(WASM) $(OUTPUT)

$(WASM): $(LINKED)
	ll2w $(LINKED) > $(WASM)

$(LINKED): $(SOURCES:.cpp=.ll)
	$(LLVMLINK) -S -o $@ $(LLVMIR)

linked: $(LINKED)

wasm: $(WASM)

clean:
	rm -f $(LINKED) $(WASM) src/*.ll src/**/*.ll

DEPFILE  = .dep
DEPTOKEN = "\# MAKEDEPENDS"
DEPFLAGS = -f $(DEPFILE) -s $(DEPTOKEN)

depend:
	@ echo $(DEPTOKEN) > $(DEPFILE)
	makedepend $(DEPFLAGS) -- $(COMPILER) $(CFLAGS) -- $(SOURCES) 2>/dev/null
	@ rm $(DEPFILE).bak

sinclude $(DEPFILE)

EMIT		:= clang++ -S -emit-llvm -std=c++2a -nostdlib -fno-builtin -Iinclude -fno-exceptions -fno-rtti
SOURCES		:= $(shell find src/*.cpp)
LLVMIR		:= $(SOURCES:.cpp=.ll)
LINKED		:= main.ll
WASM		:= main.wasm
OUTPUT		:= $(WASM:.wasm=.why)
EXTRA		:= extra.wasm
EXTRA_WHY	:= $(EXTRA:.wasm=.why)
FINAL		:= Thurisaz.why
LLVMLINK	?= llvm-link


.PHONY: linked wasm clean

%.ll: %.cpp
	$(EMIT) -Iinclude $< -o $@

all: $(FINAL)

$(FINAL): $(OUTPUT) $(EXTRA_WHY)
	wld $^ -o $@

$(EXTRA_WHY): $(EXTRA)
	wasmc $< $@

$(OUTPUT): $(WASM)
	wasmc $< $@

$(WASM): $(LINKED)
	ll2w $< > $@

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

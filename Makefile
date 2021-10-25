EMIT      := clang++ -S -emit-llvm -target mips64el-linux-gnu -std=c++20 -O0 -g -nostdlib -fno-builtin -Iinclude \
             -Imusl/arch/aarch64 -Imusl/arch/generic -Imusl/obj/src/internal -Imusl/src/include -Imusl/src/internal \
             -Imusl/obj/include -Imusl/include -Iinclude/lib -Iinclude/lib/libcxx \
			 -fno-exceptions -fno-rtti -Drestrict=__restrict__
SOURCES   := $(shell find src -name '*.cpp')
LLVMIR    := $(SOURCES:.cpp=.ll)
LINKED    := main.ll
WASM      := main.wasm
OUTPUT    := $(WASM:.wasm=.why)
EXTRA     := extra.wasm
EXTRA_WHY := $(EXTRA:.wasm=.why)
FINAL     := Thurisaz.why
LLVMLINK  ?= llvm-link

.PHONY: linked wasm clean

all: $(FINAL)

%.ll: %.cpp
	$(EMIT) -Iinclude $< -o $@

$(FINAL): $(OUTPUT) $(EXTRA_WHY)
	wasmc -l $@ $^

$(EXTRA_WHY): $(EXTRA)
	wasmc $< $@

$(OUTPUT): $(WASM)
	wasmc $< $@

$(WASM): $(LINKED)
	ll2w $< -main > $@

$(LINKED): $(LLVMIR)
	$(LLVMLINK) -S -o $@ $(LLVMIR)

linked: $(LINKED)

wasm: $(WASM)

clean:
	rm -f $(LINKED) $(WASM) $(shell find src -name '*.ll')

DEPFILE  = .dep
DEPTOKEN = "\# MAKEDEPENDS"
DEPFLAGS = -f $(DEPFILE) -s $(DEPTOKEN)

depend:
	@ echo $(DEPTOKEN) > $(DEPFILE)
	makedepend $(DEPFLAGS) -- $(COMPILER) $(CFLAGS) -- $(SOURCES) 2>/dev/null
	@ rm $(DEPFILE).bak

sinclude $(DEPFILE)

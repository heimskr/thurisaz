EMIT        := clang++ -S -emit-llvm -Xclang -disable-O0-optnone -target mips64el-linux-gnu -std=c++20 -O1 \
               -nostdlib -fno-builtin -fno-exceptions -frtti -Drestrict=__restrict__ -Wall -Wextra \
               -Iinclude -Imusl/arch/aarch64 -Imusl/arch/generic -Imusl/obj/src/internal -Imusl/src/include \
               -Imusl/src/internal -Imusl/obj/include -Imusl/include -Iinclude/lib -Iinclude/lib/libcxx
SOURCES     := $(shell find src -name '*.cpp')
LLVMIR      := $(SOURCES:.cpp=.ll)
UNOPTIMIZED := main.unopt.ll
OPTIMIZED   := main.ll
WASM        := main.wasm
OUTPUT      := $(WASM:.wasm=.why)
EXTRA       := extra.wasm
EXTRA_WHY   := $(EXTRA:.wasm=.why)
FINAL       := Thurisaz.why
LLVMLINK    ?= llvm-link
OPT         ?= opt

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

$(WASM): $(UNOPTIMIZED)
	ll2w $< -main > "tmp.$@"
	mv "tmp.$@" $@

$(UNOPTIMIZED): $(LLVMIR)
	$(LLVMLINK) -S -o $@ $(LLVMIR)

$(OPTIMIZED): $(UNOPTIMIZED)
	@# $(OPT) -S -mem2reg -always-inline -constmerge -dce -deadargelim -dse -globaldce -globalopt -inline -instcombine -aggressive-instcombine $< -o $@
	@# $(OPT) -S -mem2reg -always-inline -constmerge -dce -deadargelim -dse -globaldce -globalopt -inline $< -o $@
	@# $(OPT) -S --passes="function(mem2reg,dce,dse),always-inline,constmerge,deadargelim,globaldce,globalopt,inline" $< -o $@
	@# $(OPT) -S --passes="function(mem2reg,sroa)" -debug-pass-manager $< -o $@
	$(OPT) -S --passes="no-op-module,cgscc(no-op-cgscc,function(mem2reg,sroa<modify-cfg>,loop(no-op-loop))),function(mem2reg,sroa<modify-cfg>,loop(no-op-loop))" -debug-pass-manager $< -o $@
	@# $(OPT) -S -passes="default<O3>" $< -o $@

linked: $(UNOPTIMIZED)

wasm: $(WASM)

clean:
	rm -f $(UNOPTIMIZED) $(OPTIMIZED) $(WASM) $(shell find src -name '*.ll')

DEPFILE  = .dep
DEPTOKEN = "\# MAKEDEPENDS"
DEPFLAGS = -f $(DEPFILE) -s $(DEPTOKEN) -o.ll

depend:
	@ echo $(DEPTOKEN) > $(DEPFILE)
	makedepend $(DEPFLAGS) -- $(COMPILER) -Iinclude -- $(SOURCES) 2>/dev/null
	@ rm $(DEPFILE).bak

sinclude $(DEPFILE)

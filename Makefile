CXX ?= clang++
# -ffp-contract=off: no fused multiply-add contraction (subspec/mountains.md, Arithmetic).
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -ffp-contract=off -pthread
BUILD := build

CORE_SRC := $(wildcard core/*.cpp)
CORE_OBJ := $(patsubst core/%.cpp,$(BUILD)/core/%.o,$(CORE_SRC))

all: $(BUILD)/gallery $(BUILD)/verify $(BUILD)/render

$(BUILD)/core/%.o: core/%.cpp core/*.hpp core/*.inc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%: tools/%.cpp $(CORE_OBJ) core/*.hpp tools/*.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -Icore $< $(CORE_OBJ) -o $@

gallery: $(BUILD)/gallery
	./$(BUILD)/gallery gallery

verify: $(BUILD)/verify
	./$(BUILD)/verify

verify-full: $(BUILD)/verify
	./$(BUILD)/verify 20000

# The WebAssembly build for the site: one module, single-threaded (no
# SharedArrayBuffer needed), same arithmetic flags as the native build.
EMXX ?= em++
wasm: site/bh.js

site/bh.js: wasm/bh.cpp $(CORE_SRC) core/*.hpp core/*.inc
	$(EMXX) -std=c++17 -O2 -ffp-contract=off -DBH_SINGLE_THREAD -Icore $(CORE_SRC) wasm/bh.cpp -o site/bh.js \
		-sMODULARIZE=1 -sEXPORT_NAME=BeautifulHash -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web,node \
		-sEXPORTED_FUNCTIONS=_bh_render -sEXPORTED_RUNTIME_METHODS=cwrap

# Golden SVG checksums for fixed hashes (wasm/hashes.txt): `make golden` records
# them from the native build; `make wasm-check` fails if the native or the
# WebAssembly build drifts from them.
golden: $(BUILD)/render
	while read h; do printf "%s %s\n" $$h $$(./$(BUILD)/render $$h | shasum -a 256 | cut -d" " -f1); done < wasm/hashes.txt > wasm/golden.txt

wasm-check: $(BUILD)/render site/bh.js
	while read h; do printf "%s %s\n" $$h $$(./$(BUILD)/render $$h | shasum -a 256 | cut -d" " -f1); done < wasm/hashes.txt | diff wasm/golden.txt -
	node wasm/compare.mjs wasm/hashes.txt | diff wasm/golden.txt -
	@echo "native and WebAssembly match the golden SVGs"

budgets: $(BUILD)/budgets
	./$(BUILD)/budgets core/budgets.inc

clean:
	rm -rf $(BUILD) gallery

.PHONY: all gallery verify verify-full budgets wasm golden wasm-check clean

# Keep object files (otherwise make deletes them as intermediates).
.SECONDARY:

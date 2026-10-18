CXX ?= clang++
# -ffp-contract=off: no fused multiply-add contraction (subspec/mountains.md, Arithmetic).
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -ffp-contract=off -pthread
BUILD := build

CORE_SRC := $(wildcard core/*.cpp)
CORE_OBJ := $(patsubst core/%.cpp,$(BUILD)/core/%.o,$(CORE_SRC))

all: $(BUILD)/gallery $(BUILD)/verify

$(BUILD)/core/%.o: core/%.cpp core/*.hpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%: tools/%.cpp $(CORE_OBJ) core/*.hpp tools/*.hpp
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -Icore $< $(CORE_OBJ) -o $@

gallery: $(BUILD)/gallery
	./$(BUILD)/gallery gallery

verify: $(BUILD)/verify
	./$(BUILD)/verify

clean:
	rm -rf $(BUILD) gallery

.PHONY: all gallery verify clean

# Keep object files (otherwise make deletes them as intermediates).
.SECONDARY:

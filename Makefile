# Define build directory
BUILD_DIR = build
# Target to run cmake and make
cmake:
	# Create build directory if it doesn't exist
	@if [ ! -d "$(BUILD_DIR)" ]; then \
		mkdir $(BUILD_DIR); \
	fi
	@cd $(BUILD_DIR) && cmake ..
	@cd $(BUILD_DIR) && make

# Clean rule
clean:
	@rm -f $(OBJS_C) $(OBJS_CPP)
	@rm -rf build
	@rm -rf CMakeFiles
	@echo "Cleanup is done"

.PHONY: clean all
# Kernel Module Makefile
# Module name
MODULE_NAME := klogger
obj-m += $(MODULE_NAME).o

# Kernel directory and current working directory
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# Device file permissions
DEVICE_PERMISSION := 666
DEVICE_PATH := /dev/$(MODULE_NAME)

# Default target
all: build

# Build the kernel module
build:
	@echo "Building $(MODULE_NAME) module..."
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	@rm -f *.o *.ko *.mod.* *.symvers *.order .*.cmd

# Load the kernel module
load:
	@if lsmod | grep -q "^$(MODULE_NAME)"; then \
		echo "Module $(MODULE_NAME) is already loaded."; \
	else \
		echo "Loading $(MODULE_NAME) module..."; \
		sudo insmod $(MODULE_NAME).ko; \
		sudo chmod $(DEVICE_PERMISSION) $(DEVICE_PATH); \
		echo "Module loaded and permissions set to $(DEVICE_PERMISSION)"; \
	fi

# Unload the kernel module
unload:
	@if lsmod | grep -q "^$(MODULE_NAME)"; then \
		echo "Unloading $(MODULE_NAME) module..."; \
		sudo rmmod $(MODULE_NAME); \
		echo "Module unloaded successfully"; \
	else \
		echo "Module $(MODULE_NAME) is not loaded."; \
	fi

# Reload the kernel module (unload if loaded, then load)
reload: unload load

# Show module status
status:
	@if lsmod | grep -q "^$(MODULE_NAME)"; then \
		echo "Module $(MODULE_NAME) is loaded:"; \
		lsmod | grep "^$(MODULE_NAME)"; \
		echo "\nDevice file:"; \
		ls -l $(DEVICE_PATH) 2>/dev/null || echo "Device file not found"; \
	else \
		echo "Module $(MODULE_NAME) is not loaded."; \
	fi

# Show recent kernel logs related to the module
logs:
	@echo "Recent kernel logs for $(MODULE_NAME):"
	@sudo dmesg | grep -i "$(MODULE_NAME)" | tail -n 20

# Test targets
test:
	@echo "Running tests..."
	@sudo chmod +x ./test.sh
	@./test.sh
	unload

# Help target
help:
	@echo "Available targets:"
	@echo "  all (default) - Build the kernel module"
	@echo "  build        - Same as 'all'"
	@echo "  clean        - Remove all build artifacts"
	@echo "  load         - Load the module and set permissions"
	@echo "  unload       - Unload the module"
	@echo "  reload       - Unload and load the module"
	@echo "  status       - Show module status"
	@echo "  logs         - Show recent kernel logs for the module"
	@echo "  test         - Run tests"
	@echo "  help         - Show this help message"

.PHONY: all build clean load unload reload status logs help test
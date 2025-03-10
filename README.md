# Kernel Logger Module

A Linux kernel module implementing a character device driver that provides a circular buffer for logging messages in kernel space. This module allows user-space applications to write log entries and read them back through a simple interface, making it useful for debugging, monitoring, and logging purposes.

## Features

- Implements a character device driver (`/dev/klogger`)
- Circular buffer implementation for efficient memory usage
- Thread-safe operations using read-write locks
- Supports concurrent access from multiple processes
- Fixed-size message buffer (256 bytes per message)
- Total buffer size of 262,144 bytes (256KB)
- Automatic overwrite of oldest messages when buffer is full
- Simple read/write interface compatible with standard Unix tools

## Requirements

- Linux kernel headers
- GCC compiler
- Make build system

## Installation

1. Clone the repository:
```bash
git clone https://github.com/yourusername/kernel-logger-module.git
cd kernel-logger-module
```

2. Build the module:
```bash
make
```

3. Load the module:
```bash
make load
```

This will create the device file `/dev/klogger` with read-write permissions (666).

## Usage

### Writing Log Messages

You can write messages to the logger using standard Unix tools:

```bash
echo "Hello from userspace!" > /dev/klogger
```

### Reading Log Messages

To read all messages from the logger:

```bash
cat /dev/klogger
```

### Module Management

The Makefile provides several useful commands:

- `make` or `make build`: Build the kernel module
- `make load`: Load the module and set permissions
- `make unload`: Unload the module
- `make reload`: Unload and reload the module
- `make status`: Show module status
- `make logs`: Show recent kernel logs for the module
- `make test`: Run the test suite
- `make clean`: Clean build artifacts

## Testing

The project includes a comprehensive test suite that verifies:

- Basic read/write operations
- Multiple message handling
- Buffer overflow behavior
- Concurrent access handling
- Device file permissions
- Module loading/unloading

Run the tests with:
```bash
make test
```

## Technical Details

- Message size: 256 bytes
- Buffer size: 262,144 bytes (256KB)
- Maximum entries: 1024 messages
- Device name: klogger
- Major number: Dynamically allocated
- Access permissions: 666 (rw-rw-rw-)

## Implementation Details

The module implements:
- Circular buffer management
- Thread-safe operations using read-write locks
- Reference counting for open handles
- Proper cleanup on module unload
- Error handling and boundary checks

## License

This project is licensed under the GPL License - see the LICENSE file for details.

## Author

Lionel Silva

## Version

0.1

## Contributing

1. Fork the repository
2. Create your feature branch
3. Commit your changes
4. Push to the branch
5. Create a new Pull Request
# Pico-RTOS v0.3.0 API Reference

This document provides a comprehensive reference for the new features introduced in Pico-RTOS v0.3.0.

## Version 0.3.0 - Advanced Features and Multi-Core Support

This API reference covers all new functions available in Pico-RTOS v0.3.0, which builds upon the solid foundation of v0.2.1 with advanced synchronization primitives, comprehensive debugging tools, multi-core support, and enhanced system extensions.

For v0.2.1 API functions, please refer to the main [API Reference](api_reference.md).

## Event Groups

Event groups provide advanced synchronization capabilities, allowing tasks to wait for multiple events simultaneously with "wait for any" or "wait for all" semantics.

### Data Structures

```c
typedef struct {
    uint32_t event_bits;                    // Current event state (32 events max)
    pico_rtos_block_object_t *block_obj;    // Blocking object for waiting tasks
    critical_section_t cs;                  // Thread safety
} pico_rtos_event_group_t;
```

### Functions

#### `bool pico_rtos_event_group_init(pico_rtos_event_group_t *event_group)`

Initialize an event group.

**Parameters:**
- `event_group`: Pointer to event group structure to initialize

**Returns:**
- `true` if initialization successful
- `false` if initialization failed (invalid parameters)

**Performance:** O(1)

**Example:**
```c
pico_rtos_event_group_t my_events;
if (pico_rtos_event_group_init(&my_events)) {
    // Event group ready to use
}
```

#### `bool pico_rtos_event_group_set_bits(pico_rtos_event_group_t *event_group, uint32_t bits)`

Set (activate) one or more event bits. This operation will unblock any tasks waiting for these events.

**Parameters:**
- `event_group`: Pointer to event group structure
- `bits`: Bitmask of events to set (bit 0 = event 0, bit 1 = event 1, etc.)

**Returns:**
- `true` if bits were set successfully
- `false` if invalid parameters

**Performance:** O(1) for setting bits, O(n) for unblocking tasks where n is the number of waiting tasks

**Example:**
```c
// Set events 0, 2, and 5
pico_rtos_event_group_set_bits(&my_events, (1 << 0) | (1 << 2) | (1 << 5));
```#### 
`bool pico_rtos_event_group_clear_bits(pico_rtos_event_group_t *event_group, uint32_t bits)`

Clear (deactivate) one or more event bits.

**Parameters:**
- `event_group`: Pointer to event group structure
- `bits`: Bitmask of events to clear

**Returns:**
- `true` if bits were cleared successfully
- `false` if invalid parameters

**Performance:** O(1)

**Example:**
```c
// Clear events 0 and 2
pico_rtos_event_group_clear_bits(&my_events, (1 << 0) | (1 << 2));
```

#### `uint32_t pico_rtos_event_group_wait_bits(pico_rtos_event_group_t *event_group, uint32_t bits_to_wait_for, bool wait_for_all, bool clear_on_exit, uint32_t timeout)`

Wait for one or more event bits to be set.

**Parameters:**
- `event_group`: Pointer to event group structure
- `bits_to_wait_for`: Bitmask of events to wait for
- `wait_for_all`: If `true`, wait for ALL specified bits; if `false`, wait for ANY specified bit
- `clear_on_exit`: If `true`, automatically clear the satisfied bits when returning
- `timeout`: Timeout in milliseconds, `PICO_RTOS_WAIT_FOREVER` to wait forever, or `PICO_RTOS_NO_WAIT` for immediate return

**Returns:**
- Bitmask of events that satisfied the wait condition
- 0 if timeout occurred or invalid parameters

**Performance:** O(1) for checking, blocks until condition met or timeout

**Example:**
```c
// Wait for either event 0 OR event 1, clear them when satisfied
uint32_t result = pico_rtos_event_group_wait_bits(&my_events, 
                                                  (1 << 0) | (1 << 1), 
                                                  false,  // wait for ANY
                                                  true,   // clear on exit
                                                  5000);  // 5 second timeout
if (result != 0) {
    if (result & (1 << 0)) {
        // Event 0 occurred
    }
    if (result & (1 << 1)) {
        // Event 1 occurred
    }
}
```

### Event Group Best Practices

1. **Event Bit Assignment**: Use meaningful constants for event bits:
   ```c
   #define EVENT_SENSOR_DATA_READY  (1 << 0)
   #define EVENT_NETWORK_CONNECTED  (1 << 1)
   #define EVENT_USER_INPUT         (1 << 2)
   ```

2. **Timeout Handling**: Always check return values when using timeouts:
   ```c
   uint32_t events = pico_rtos_event_group_wait_bits(&my_events, 
                                                     EVENT_SENSOR_DATA_READY,
                                                     false, true, 1000);
   if (events == 0) {
       // Timeout occurred - handle appropriately
   }
   ```

3. **Memory Overhead**: Each event group uses approximately 48 bytes plus blocking object overhead.

## Stream Buffers

Stream buffers provide efficient variable-length message passing between tasks with optional zero-copy semantics for large data transfers.

### Data Structures

```c
typedef struct {
    uint8_t *buffer;                       // Circular buffer storage
    uint32_t buffer_size;                  // Total buffer size
    uint32_t head;                         // Write position
    uint32_t tail;                         // Read position
    uint32_t bytes_available;              // Current data bytes
    pico_rtos_block_object_t *reader_block; // Reader blocking object
    pico_rtos_block_object_t *writer_block; // Writer blocking object
    critical_section_t cs;                 // Thread safety
    bool zero_copy_enabled;                // Zero-copy optimization flag
} pico_rtos_stream_buffer_t;
```

### Functions

#### `bool pico_rtos_stream_buffer_init(pico_rtos_stream_buffer_t *stream, uint8_t *buffer, uint32_t buffer_size)`

Initialize a stream buffer with the provided storage.

**Parameters:**
- `stream`: Pointer to stream buffer structure to initialize
- `buffer`: Pointer to buffer memory for data storage
- `buffer_size`: Size of the buffer in bytes (must be > 0)

**Returns:**
- `true` if initialization successful
- `false` if initialization failed (invalid parameters)

**Performance:** O(1)

**Example:**
```c
static uint8_t stream_storage[1024];
pico_rtos_stream_buffer_t data_stream;

if (pico_rtos_stream_buffer_init(&data_stream, stream_storage, sizeof(stream_storage))) {
    // Stream buffer ready to use
}
```

#### `uint32_t pico_rtos_stream_buffer_send(pico_rtos_stream_buffer_t *stream, const void *data, uint32_t length, uint32_t timeout)`

Send data to the stream buffer.

**Parameters:**
- `stream`: Pointer to stream buffer structure
- `data`: Pointer to data to send
- `length`: Number of bytes to send
- `timeout`: Timeout in milliseconds, `PICO_RTOS_WAIT_FOREVER` to wait forever, or `PICO_RTOS_NO_WAIT` for immediate return

**Returns:**
- Number of bytes actually sent
- 0 if timeout occurred or invalid parameters

**Performance:** O(1) for small messages, O(n) for large messages where n is message size

**Example:**
```c
const char *message = "Hello, World!";
uint32_t bytes_sent = pico_rtos_stream_buffer_send(&data_stream, 
                                                   message, 
                                                   strlen(message), 
                                                   1000);
if (bytes_sent == strlen(message)) {
    // All data sent successfully
}
```## M
emory Pools

Memory pools provide O(1) allocation and deallocation for fixed-size blocks, reducing fragmentation and improving determinism.

### Data Structures

```c
typedef struct {
    void *pool_start;                      // Start of memory pool
    uint32_t block_size;                   // Size of each block
    uint32_t total_blocks;                 // Total number of blocks
    uint32_t free_blocks;                  // Currently free blocks
    pico_rtos_memory_block_t *free_list;   // Head of free list
    critical_section_t cs;                 // Thread safety
    uint32_t allocation_count;             // Statistics
    uint32_t peak_usage;                   // Peak usage tracking
} pico_rtos_memory_pool_t;
```

### Functions

#### `bool pico_rtos_memory_pool_init(pico_rtos_memory_pool_t *pool, void *memory, uint32_t block_size, uint32_t block_count)`

Initialize a memory pool with the provided storage.

**Parameters:**
- `pool`: Pointer to memory pool structure to initialize
- `memory`: Pointer to memory for the pool storage
- `block_size`: Size of each block in bytes (must be >= sizeof(void*))
- `block_count`: Number of blocks in the pool

**Returns:**
- `true` if initialization successful
- `false` if initialization failed (invalid parameters)

**Performance:** O(n) where n is block_count (for free list setup)

**Example:**
```c
// Create pool for 64-byte blocks
static uint8_t pool_memory[64 * 32];  // 32 blocks of 64 bytes each
pico_rtos_memory_pool_t my_pool;

if (pico_rtos_memory_pool_init(&my_pool, pool_memory, 64, 32)) {
    // Memory pool ready to use
}
```

#### `void *pico_rtos_memory_pool_alloc(pico_rtos_memory_pool_t *pool, uint32_t timeout)`

Allocate a block from the memory pool.

**Parameters:**
- `pool`: Pointer to memory pool structure
- `timeout`: Timeout in milliseconds, `PICO_RTOS_WAIT_FOREVER` to wait forever, or `PICO_RTOS_NO_WAIT` for immediate return

**Returns:**
- Pointer to allocated block
- `NULL` if no blocks available within timeout or invalid parameters

**Performance:** O(1)

**Example:**
```c
void *block = pico_rtos_memory_pool_alloc(&my_pool, 1000);
if (block) {
    // Use the allocated block
    memset(block, 0, 64);  // Initialize if needed
}
```

#### `bool pico_rtos_memory_pool_free(pico_rtos_memory_pool_t *pool, void *block)`

Free a previously allocated block back to the memory pool.

**Parameters:**
- `pool`: Pointer to memory pool structure
- `block`: Pointer to block to free (must have been allocated from this pool)

**Returns:**
- `true` if block was freed successfully
- `false` if invalid parameters or block doesn't belong to this pool

**Performance:** O(1)

**Example:**
```c
if (pico_rtos_memory_pool_free(&my_pool, block)) {
    block = NULL;  // Good practice to clear pointer
}
```

## Multi-Core Support

Pico-RTOS v0.3.0 provides comprehensive multi-core support for the RP2040's dual-core architecture, including SMP scheduling, task affinity, and inter-core communication.

### Core Affinity

#### `bool pico_rtos_task_set_core_affinity(pico_rtos_task_t *task, pico_rtos_core_affinity_t affinity)`

Set the core affinity for a task.

**Parameters:**
- `task`: Pointer to task structure, or `NULL` for current task
- `affinity`: Core affinity setting

**Returns:**
- `true` if affinity was set successfully
- `false` if invalid parameters or affinity not supported

**Performance:** O(1)

**Core Affinity Values:**
```c
typedef enum {
    PICO_RTOS_CORE_AFFINITY_NONE,    // No core preference (can run anywhere)
    PICO_RTOS_CORE_AFFINITY_CORE0,   // Bind to core 0 only
    PICO_RTOS_CORE_AFFINITY_CORE1,   // Bind to core 1 only
    PICO_RTOS_CORE_AFFINITY_ANY      // Can run on any available core
} pico_rtos_core_affinity_t;
```

**Example:**
```c
// Create a task that must run on core 1
pico_rtos_task_t sensor_task;
pico_rtos_task_create(&sensor_task, "sensor", sensor_task_func, NULL, 1024, 5);
pico_rtos_task_set_core_affinity(&sensor_task, PICO_RTOS_CORE_AFFINITY_CORE1);
```

#### `uint32_t pico_rtos_get_current_core(void)`

Get the ID of the currently executing core.

**Returns:**
- 0 for core 0
- 1 for core 1

**Performance:** O(1)

**Example:**
```c
uint32_t core_id = pico_rtos_get_current_core();
printf("Running on core %u\n", core_id);
```

### Inter-Core Communication

#### `bool pico_rtos_ipc_send_message(uint32_t target_core, uint32_t message_type, const void *data, uint32_t data_length, uint32_t timeout)`

Send a message to another core.

**Parameters:**
- `target_core`: Target core ID (0 or 1)
- `message_type`: User-defined message type identifier
- `data`: Pointer to message data
- `data_length`: Length of message data in bytes
- `timeout`: Timeout in milliseconds

**Returns:**
- `true` if message sent successfully
- `false` if timeout occurred or invalid parameters

**Performance:** O(1) for small messages, O(n) for large messages

**Example:**
```c
typedef struct {
    uint32_t sensor_id;
    float temperature;
    uint32_t timestamp;
} sensor_data_t;

sensor_data_t data = {1, 23.5f, pico_rtos_get_tick_count()};
bool sent = pico_rtos_ipc_send_message(1,                    // Send to core 1
                                       MSG_TYPE_SENSOR_DATA, // Message type
                                       &data,                // Data
                                       sizeof(data),         // Data size
                                       1000);                // 1 second timeout
```

## High-Resolution Timers

High-resolution software timers provide microsecond-precision timing using the RP2040's hardware timer capabilities.

### Functions

#### `bool pico_rtos_hires_timer_init(pico_rtos_hires_timer_t *timer, const char *name, void (*callback)(void *), void *param, uint64_t period_us, bool auto_reload)`

Initialize a high-resolution timer.

**Parameters:**
- `timer`: Pointer to timer structure to initialize
- `name`: Name of the timer (for debugging)
- `callback`: Function to call when timer expires
- `param`: Parameter to pass to callback function
- `period_us`: Timer period in microseconds
- `auto_reload`: Whether timer should automatically restart after expiring

**Returns:**
- `true` if initialization successful
- `false` if initialization failed (invalid parameters)

**Performance:** O(1)

**Example:**
```c
void timer_callback(void *param) {
    uint32_t *counter = (uint32_t *)param;
    (*counter)++;
}

static uint32_t timer_count = 0;
pico_rtos_hires_timer_t precision_timer;

bool success = pico_rtos_hires_timer_init(&precision_timer,
                                          "precision",
                                          timer_callback,
                                          &timer_count,
                                          1000,    // 1000 μs = 1 ms
                                          true);   // auto-reload
```

## Debugging and Profiling Tools

Comprehensive debugging and profiling capabilities for analyzing system behavior and performance.

### Task Inspection

#### `bool pico_rtos_get_task_info(pico_rtos_task_t *task, pico_rtos_task_info_t *info)`

Get detailed information about a specific task.

**Parameters:**
- `task`: Pointer to task structure
- `info`: Pointer to structure to store task information

**Returns:**
- `true` if information retrieved successfully
- `false` if invalid parameters

**Performance:** O(1)

**Task Information Structure:**
```c
typedef struct {
    const char *name;                      // Task name
    uint32_t priority;                     // Current priority
    pico_rtos_task_state_t state;          // Current state
    uint32_t stack_usage;                  // Current stack usage
    uint32_t stack_high_water;             // Peak stack usage
    uint64_t cpu_time_us;                  // Total CPU time (microseconds)
    uint32_t context_switches;             // Number of context switches
    uint32_t last_run_time;                // Last execution timestamp
    uint32_t core_affinity;                // Core affinity setting
    uint32_t assigned_core;                // Currently assigned core
} pico_rtos_task_info_t;
```

## Constants and Macros

### Timeout Constants
```c
#define PICO_RTOS_WAIT_FOREVER    0xFFFFFFFF
#define PICO_RTOS_NO_WAIT         0
```

### Event Group Constants
```c
#define PICO_RTOS_EVENT_GROUP_MAX_EVENTS  32
#define PICO_RTOS_EVENT_ALL_BITS          0xFFFFFFFF
```

### Stream Buffer Constants
```c
#define PICO_RTOS_STREAM_BUFFER_MIN_SIZE      64
#define PICO_RTOS_STREAM_BUFFER_ZERO_COPY_THRESHOLD  256
```

### Memory Pool Constants
```c
#define PICO_RTOS_MEMORY_POOL_MIN_BLOCK_SIZE  sizeof(void*)
#define PICO_RTOS_MEMORY_POOL_MAX_BLOCKS      1024
```

### Multi-Core Constants
```c
#define PICO_RTOS_MAX_CORES                   2
#define PICO_RTOS_IPC_MAX_MESSAGE_SIZE        256
#define PICO_RTOS_CORE_ID_INVALID            0xFF
```

## Version Information

### Version Constants
```c
#define PICO_RTOS_VERSION_MAJOR   0
#define PICO_RTOS_VERSION_MINOR   3
#define PICO_RTOS_VERSION_PATCH   0
#define PICO_RTOS_VERSION_STRING  "0.3.0"
```

### Feature Availability Macros
```c
#define PICO_RTOS_HAS_EVENT_GROUPS        1
#define PICO_RTOS_HAS_STREAM_BUFFERS      1
#define PICO_RTOS_HAS_MEMORY_POOLS        1
#define PICO_RTOS_HAS_MULTI_CORE          1
#define PICO_RTOS_HAS_HIRES_TIMERS        1
#define PICO_RTOS_HAS_PROFILING           1
#define PICO_RTOS_HAS_TRACING             1
```

## Performance Summary

| Feature | Typical Latency | Memory Overhead | Notes |
|---------|----------------|-----------------|-------|
| Event Groups | 2-5 μs | 48 bytes + blocking | O(1) set/clear operations |
| Stream Buffers | 5-15 μs | Buffer size + 64 bytes | Variable based on message size |
| Memory Pools | 1-3 μs | 32 bytes + pool size | O(1) allocation/deallocation |
| Multi-Core IPC | 5-15 μs | 256 bytes per channel | Hardware FIFO based |
| High-Res Timers | 1-2 μs | 48 bytes per timer | Microsecond resolution |
| Task Profiling | 1-2 μs | 32 bytes per function | Per function entry/exit |
| Event Tracing | 0.5-1 μs | 16 bytes per event | Minimal overhead |

---

## Migration from v0.2.1

All existing v0.2.1 APIs remain unchanged and fully compatible. New v0.3.0 features are additive and optional. See the [Migration Guide](migration_guide_v0.3.0.md) for detailed upgrade instructions.
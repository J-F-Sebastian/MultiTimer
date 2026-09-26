# MultiTimer Design

## Chapter 1: Architecture

`MultiTimer` manages a collection of one-shot timer entries and a worker thread that tracks the next
timer to expire.

Each timer stores an absolute expiration time calculated from `std::chrono::steady_clock`.
The timer list is kept sorted by expiration time, with the earliest timer at the front and the latest timer at
the back. The worker therefore only needs to wait for the first non-expired entry rather than scan
the whole list for the next deadline.

The class uses a condition variable to coordinate changes to the schedule. Adding, removing, or
updating a timer refreshes the worker's next time point and notifies the worker. The worker waits
with `wait_until`, marks elapsed entries as expired, and refreshes the next time point.

All access to the timer list and scheduling state is protected by the class mutex. The public timer
operations can therefore be called while the worker thread is running.

The class owns its worker thread. The destructor waits for the worker thread to finish. Before
notifying the worker, the destructor clears the timer list while holding the mutex. This guarantees
that the worker's empty-list loop condition causes it to exit before `join()` completes.

## Chapter 2: Containers, Data Structures, and Algorithms

### 2.1 Clock and time representation

- `Clock` is an alias for `std::chrono::steady_clock`, which is appropriate for measuring durations
  because it is not affected by wall-clock adjustments.
- `TimeReference` is an alias for `Clock::time_point`.
- A timer's expiration is stored as an absolute `TimeReference`, computed as `Clock::now() +
  duration`.
- `timepoint` stores the next expiration selected by the scheduler. When there is no eligible timer,
  it is set to `TimeReference::max()`.

### 2.2 Timer entry

`TimerEntry` is a `std::tuple<TimeReference, bool>`:

1. The first element is the expiration time. Active entries contain their actual deadline; expired
   entries contain `TimeReference::max()`.
2. The second element indicates whether the timer has expired.

`createTimerEntry` returns a pointer to a heap-allocated entry initialized with
`TimeReference::max()` and an expired flag of `true`. It does not insert the entry into `timers`.
The caller owns the entry and must delete it when it is no longer needed. The timer list stores
pointers to these separately allocated entries.

### 2.3 Timer list

`TimerList` is an alias for `std::list<TimerEntry*>`. A linked list is used because:

- insertion and reordering do not invalidate pointers to entries;
- erasing a matching entry is constant-time once its iterator is known;
- `splice` can move an existing pointer to a new sorted position without reallocating it.

Registered entries are inserted before the first entry with a later expiration. Equal expiration
times retain insertion order. The list contains registered timers until they are removed or expire.

### 2.4 Scheduling algorithms

`createTimerEntry` only allocates an expired, detached entry. It does not add an entry to the list
or affect the schedule.

`removeTimerEntry` searches for the supplied pointer in the list. If found, it marks the entry
expired, removes it from the schedule, and refreshes the next expiration. It does not delete the
entry; ownership remains with the caller.

`updateTimer` calculates a new expiration and clears the expired flag. If the entry is expired, it
is treated as detached and inserted at its sorted position. Otherwise, it is found in the list and
spliced to its new sorted position. Both cases preserve the caller's pointer.

`markExpired` starts at the front of the sorted list and marks then removes every entry whose
expiration is at or before the current time. Because the list is sorted, the remaining timers stay
in expiration order. Expired entries remain allocated and can be scheduled again with `updateTimer`.

`refreshDelay` assigns `timepoint` from the first scheduled entry. If no timers remain, it assigns
`TimeReference::max()`.

### 2.5 Thread synchronization

The mutex protects the timer list and `timepoint`. The condition variable wakes the worker when the
schedule changes, allowing a newly inserted or updated earlier timer to be observed immediately. The
worker uses `condition_variable::wait_until` with `timepoint` to sleep until the next deadline or
until a scheduling operation notifies it.

The worker marks elapsed entries and refreshes `timepoint` after each wake-up. It continues until
the list is empty. The destructor explicitly clears the list under the mutex and notifies the
condition variable, providing the shutdown signal used by the worker.

## Chapter 3: Public API

### 3.1 Constructing a `MultiTimer`

The `MultiTimer` constructor is protected, so application code cannot directly write:

```cpp
MultiTimer timer;
```

Derive a class from `MultiTimer` and expose the construction policy required by the application. A
common choice is a singleton:

```cpp
class MultiTimerSingleton : public MultiTimer
{
public:
    static MultiTimerSingleton& instance()
    {
        static MultiTimerSingleton singleton;
        return singleton;
    }

private:
    MultiTimerSingleton() = default;
    ~MultiTimerSingleton() = default;

    MultiTimerSingleton(const MultiTimerSingleton&) = delete;
    MultiTimerSingleton& operator=(const MultiTimerSingleton&) = delete;
    MultiTimerSingleton(MultiTimerSingleton&&) = delete;
    MultiTimerSingleton& operator=(MultiTimerSingleton&&) = delete;
};
```

The local static object is created on first use and destroyed automatically at program shutdown.
Copying and moving are disabled in the base class and should also be disabled in the derived
singleton.

### 3.2 Using the API

#### `init`

```cpp
timer.init();
```

Starts the worker thread. If a worker already exists, `init` joins and replaces it before starting a
new one.

#### `createTimerEntry`

```cpp
MultiTimer::TimerEntry* entry =
    timer.createTimerEntry(std::chrono::seconds(5));
```

Allocates and returns a detached entry initialized with `TimeReference::max()` and an expired flag
of `true`. The duration parameter is retained by the API but does not affect this initial state.
This method does not register or schedule the entry. The caller owns the returned pointer and must
`delete` it when no longer needed.

#### `updateTimer`

```cpp
bool updated = timer.updateTimer(entry, std::chrono::seconds(10));
```

Assigns a new expiration based on the current time and `duration` and clears the expired flag. An
expired entry is treated as detached and inserted into the sorted list; a non-expired entry is
reordered in place. It returns `true` when updated and `false` if a non-expired entry is not in the
list. The `TimerEntry*` remains valid.

#### `removeTimerEntry`

```cpp
bool removed = timer.removeTimerEntry(entry);
```

Searches for the entry identified by `entry`, marks it expired, removes it from the schedule, and
wakes the worker. It returns `true` when the entry was scheduled and removed, or `false` when no
matching entry exists. The entry remains allocated and owned by the caller.

#### Destructor

The destructor stops normal object use and joins the owned worker thread before the object is
destroyed. It clears the scheduled timer pointers while holding the mutex and notifies the worker.
The caller retains ownership of each `TimerEntry*` and must delete entries when they are no longer
needed, including after the `MultiTimer` is destroyed.

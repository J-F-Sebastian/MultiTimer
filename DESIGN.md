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

The pointer returned by `addTimer` identifies the list object itself. The pointer remains stable
while the entry is in the `std::list`, including when the entry is reordered with `splice`. It
becomes invalid after `removeTimer` succeeds.

### 2.3 Timer list

`TimerList` is an alias for `std::list<TimerEntry>`. A linked list is used because:

- insertion does not invalidate pointers to existing entries;
- erasing a matching entry is constant-time once its iterator is known;
- `splice` can move an existing entry to a new sorted position without reallocating it.

New entries are inserted before the first entry with a later expiration. Equal expiration times
retain insertion order. Active entries remain at the front of the list, while expired entries are
moved to the back.

### 2.4 Scheduling algorithms

`addTimer` walks the sorted list, inserts the new entry at its ordered position, and refreshes
`timepoint`.

`removeTimer` searches for an entry by comparing the supplied pointer with the address of each list
element. If found, it erases that element and refreshes the schedule.

`updateTimer` finds the existing entry by pointer, calculates a new expiration, resets its expired
flag, and uses `std::list::splice` to move the entry to its new sorted position. This preserves the
caller's pointer.

`markExpired` starts at the front of the sorted list and processes every entry whose expiration is
at or before the current time. For each expired entry it sets the expiration to
`TimeReference::max()`, sets the expired flag to `true`, and splices the node to the end of the
list. Because the list is sorted, the remaining active entries stay at the front.

`refreshDelay` skips leading entries already marked expired and assigns `timepoint` from the first
remaining active entry. If all entries are expired, it assigns `TimeReference::max()`.

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

#### `addTimer`

```cpp
MultiTimer::TimerEntry* entry =
    timer.addTimer(std::chrono::seconds(5));
```

Creates a timer that expires after the supplied duration, inserts it in ascending expiration order,
wakes the worker, and returns a pointer to the stored entry. Keep the pointer when the timer must
later be updated or removed.

#### `updateTimer`

```cpp
bool updated = timer.updateTimer(entry, std::chrono::seconds(10));
```

Finds the entry identified by `entry`, assigns a new expiration based on the current time and
`duration`, clears its expired flag, and reorders it. It returns `true` when the entry was found and
`false` otherwise. A successful update preserves the `TimerEntry*`.

#### `removeTimer`

```cpp
bool removed = timer.removeTimer(entry);
```

Searches for the entry identified by `entry`, removes it, and wakes the worker. It returns `true`
when the entry was found and removed, or `false` when no matching entry exists. Do not use a pointer
after `removeTimer` returns `true`.

#### Destructor

The destructor stops normal object use and joins the owned worker thread before the object is
destroyed. It clears any remaining timer entries while holding the mutex and notifies the worker, so
callers may leave timers in the list when the object is destroyed. Any `TimerEntry*` retained by the
caller becomes invalid once the corresponding entry is removed or the owning `MultiTimer` is
destroyed.

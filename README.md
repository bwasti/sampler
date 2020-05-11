# sampler

simple C++ `perf_event_open` wrapper

![](https://i.imgur.com/C8WkHHX.gif)

# Test

```
make
sudo ./test
```

# Usage

The input to a Sampler is a vector of addresses and a function to profile:

```
std::vector<const void*> addresses;
Sampler s(addresses, [&]() { f(); });
```

This will kick off a separate thread and fork a child process that invokes the passed in function.
While the sampler is running, we can query for the results.

```
// does not interrupt background execution
std::vector<size_t> address_map = s.query();
```

Samples that fall in between the passed in addresses are counted.
Thus, `address_map.size() == addresses.size()` and `address_map.back() == 0`.


When you are finished with the sampler, call `s.join()` to kill
the thread and child process.

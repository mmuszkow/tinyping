tinyping
========

Simple ICMP ping implementation for Linux with kernel 3.x+. To use it, you'll need to set your allowed groups in `/proc/sys/net/ipv4/ping_group_range`. I used it in my logstash ping plugin to perform ping:

```
require "ffi"

module TinyPing
  extend FFI::Library
  ffi_lib '/usr/lib64/libtinyping.so'
  attach_function :init, [:int, :int], :int
  attach_function :ping, [:string], :long_long
  attach_function :deinit, [], :void
end

@ttl = 255
@timeout = 2
raise if TinyPing.init(@ttl, @timeout) < 0

# Some code later..
res = TinyPing.ping(host)
if res >= 0
  event["pingable"] = true
  event["ping_time"] = res / 1000.0
else
  event["pingable"] = false
  event["ping_err"] = case res
    when -4 then "Cannot resolve hostname"
    when -5 then "Sending echo request failed"
    when -6 then "Destination unreachable"
    when -7 then "Timeout"
    else         "Unknown error"
  end
end

# In the end
TinyPing.deinit
```

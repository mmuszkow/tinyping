tinyping
========

Simple ICMP ping implementation for Linux with kernel 3.x+. To use it, you'll need to allow your groups in `/proc/sys/net/ipv4/ping_group_range`. I wrote it to perform the ICMP ping in JRuby:
```
module FFIping
  extend FFI::Library
  ffi_lib "/usr/local/lib64/libtinyping.so"
  attach_function :ping, [:string, :int], :long_long
end

res = FFIping.ping(host, 5)
if res >= 0
  event["pingable"] = true
  event["ping_time"] = res / 1000.0
else
  event["pingable"] = false
  event["ping_err"] = case res
    when -1 then "Unknown host"
    when -2 then "Permission denied"
    when -3 then "Cannot set TTL"
    when -4 then "Cannot set timeout"
    when -5 then "Cannot send ICMP request packet"
    when -6 then "Destination unreachable"
    when -7 then "Timeout"
    else "Uknown error"
  end
end
```

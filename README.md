# Netlink Examples

Some low-level examples of the Linux Netlink interface.

  1. __pair__         create a virtual ethernet pair
  2. __link_address__ create a pair and add an address
  3. __refactor__     rewrite link_address in a more concise manner
  4. __namespace__    new network namespace, code courtesy iproute2
  5. __pair_ns__      move the virtual peer into the network namespace
  6. __ns_addr__      add an address to a virtual peer in a namespace
  7. __ns_gw__        add a default route in the namespace and turn on the links
  8. __masquerade__   a libiptc example to add a masquerade route
  9. __forward__      a libiptc module to add forwarding rules
  10. __final__       Combine it all into a single binary

You're going to need a C compiler, GNU Make, and `iptables-devel` or `iptables-dev`
in order to compile the final output.

The goal of this project is to recreate the following script:

```
#!/bin/bash

ip link add veth1 type veth peer name vpeer1

ip link set vpeer1 netns ns1

ip addr add 172.16.1.1/24 dev veth1
ip link set veth1 up

ip netns exec ns1 ip addr add 172.16.1.2/24 dev vpeer1
ip netns exec ns1 ip link set vpeer1 up
ip netns exec ns1 ip link set lo up

ip netns exec ns1 ip route add default via 172.16.1.1
echo 1 > /proc/sys/net/ipv4/ip_forward

iptables -t nat -A POSTROUTING -s 172.16.1.0/255.255.255.0 -o eth0 -j MASQUERADE

iptables -A FORWARD -i eth0 -o veth1 -j ACCEPT
iptables -A FORWARD -o eth0 -i veth1 -j ACCEPT
```

## Further reading

I found the following list very helpful when putting together this project

  - https://blogs.igalia.com/dpino/2016/04/10/network-namespaces/
  - https://github.com/theodor96/iptc-dev
  - https://github.com/kenshin54/crane
  - https://strace.io/

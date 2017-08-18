# Netlink Examples

Some low-level examples of using the Linux Netlink interface.

  1. pair:        create a virtual ethernet pair
  2. link_addres: create a pair and add an address
  3. refactor:    rewrite link_address in a more concise manner
  4. namespace:   new network namespace, code courtesy iproute2
  5. pair_ns:     move the virtual peer into the network namespace

The goal of this project is to recreate the following script:

```
#!/bin/bash

ip link add veth1 type veth peer name vpeer1

ip link set vpeer1 netns ns1

ip addr add 172.16.1.1/24 dev veth1

ip netns exec ns1 ip addr add 172.16.1.2/24 dev vpeer1
ip netns exec ns1 ip link set vpeer1 up
ip netns exec ns1 ip link set lo up

ip netns exec ns1 ip route add default via 172.16.1.1
echo 1 > /proc/sys/net/ipv4/ip_forward

iptables -t nat -A POSTROUTING -s 172.16.1.0/255.255.255.0 -o eth0 -j MASQUERADE

iptables -A FORWARD -i eth0 -o veth1 -j ACCEPT
iptables -A FORWARD -o eth0 -i veth1 -j ACCEPT
```

# DEPRECATED: USE https://github.com/recolic/udp-forwarder-ex

------------

# UDP forwarder

A simple **UNSAFE** tool to encrypt / hide your UDP packets. Trying to trick The Great Firewall deep learning VPN detection
model. The packet can be easily decrypted if attacker checks the source code. 

This tool is usually used with OpenVPN, to avoid being `deep-learned` by the GFW. OpenVPN already encrypted the packets,
so I needn't do it again.

## NOTE

DO NOT USE MASTER BRANCH. It's still under debugging. (the connection migration is not stable)

## Design

![explain.png](https://raw.githubusercontent.com/recolic/udp_forwarder_ng/master/res/explain.png)

## Build

```
mkdir build && cd build
cmake .. && make
./udp_forwarder_ng [args ...]
```

## Common Deployment

![solu.png](https://raw.githubusercontent.com/recolic/udp_forwarder_ng/master/res/solu.png)

## Common mistake

If you run OpenVPN and udp_forwarder on the same PC, you won't access the Internet successfully.
If you did it, please think again and you'll realize how stupid you are.

## Naive performance test

Note: the connection setup procedure maybe a little slow, but it doesn't matter.

- Latency

with proxy: (encrypted OpenVPN + encrypted&obfs udp_forwarder_ng) 70.386ms

without proxy: 0.475ms + 68.578ms = 69.053ms

overhead (OpenVPN+Forwarder): 1.333ms

## Known bug

Every UDP Forwarder, will drop the first client2server packet. (Doesn't matter).

Every UDP Forwarder, will drop 

I assume that every connection has a unique port. search for comment tag `1632` 

# UDP forwarder

A simple **UNSAFE** tool to encrypt / hide your UDP packets. Trying to trick The Great Firewall deep learning VPN detection
model. The packet can be easily decrypted if attacker checks the source code. 

This tool is usually used with OpenVPN, to avoid being `deep-learned` by the GFW. OpenVPN already encrypted the packets,
so I needn't do it again.

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


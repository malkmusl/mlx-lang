# mlx-netd

The network of MLX/Linux in place of NetworkManager, written in Mlx
without libc. `mlx-netd` follows the links through route netlink
(`std.netlink`), puts them up, and once a link has a carrier configures
it: DHCPv4 or the static settings of its configuration. It sets the
link's address and default route and writes the name servers of all
links into `resolv.conf`. It answers on the system bus (MLXIPC's or
D-Bus's), where `mlx-net`, the top bar and mlx-settings ask it, and
where apps that ask NetworkManager whether they are online get an
answer too.

```sh
mlx4 projects/network/main.mlx -o mlx-netd
mlx4 projects/network/tool.mlx -o mlx-net
tools/check_network.sh                   # the end-to-end check
```

`tools/check_network.sh` runs in network namespaces of its own
(`unshare -rn`), so the machine's network is not touched: two veth links,
one behind a test DHCP server (`tools/network-test/dhcpd.py`), one
configured statically.

## The daemon

```
mlx-netd [--config FILE] [--resolv FILE] [--bus PATH] [--verbose]
```

| Option | Default |
| --- | --- |
| `--config FILE` | `/etc/mlx/network.conf` |
| `--resolv FILE` | `/etc/resolv.conf` |
| `--bus PATH` | `$DBUS_SYSTEM_BUS_ADDRESS`, else `/run/dbus/system_bus_socket` |
| `--verbose` | says what it does on stderr |

SIGHUP reads the configuration again, SIGTERM and SIGINT end it. It needs
`CAP_NET_ADMIN` and `CAP_NET_RAW` (root). Without a bus it still runs and
tries the bus again every ten seconds.

What a link goes through:

1. Route netlink reports it (at start, from a dump, later as it comes).
   The loopback and the links the configuration ignores stay unmanaged.
2. It is put up, unless its method is `off`.
3. With a carrier it is configured. DHCP asks for an address
   (DISCOVER, OFFER, REQUEST, ACK), renews the lease at T1 and rebinds
   at T2; a NAK starts over, a lease that runs out unconfigures the link.
   A static link gets its address, gateway and name servers at once.
4. Without its carrier, or switched off, its address and route are
   removed and its name servers leave `resolv.conf` (a lease is released
   when the link is switched off).

Each link's default route carries its metric (100 on a cable, 600 on
Wi-Fi, or what the configuration says), so the kernel takes the lowest.
`resolv.conf` lists the search domains first, then the name servers of
the links in the order of their metrics.

## The configuration

`/etc/mlx/network.conf`: `key = value` lines, `#` starts a comment. A key
with a link's name before a dot is that link's.

```
hostname = laptop              # said to DHCP servers (else /etc/hostname)
ignore = docker* veth* virbr* br-* vnet* tap* tun*
                               # links left alone (this is the default)
eth0.method = dhcp             # dhcp (the default), static, or off
eth1.method = static
eth1.address = 192.168.1.5/24
eth1.gateway = 192.168.1.1
eth1.dns = 1.1.1.1 9.9.9.9     # up to three
eth1.metric = 50
```

## On the bus

`org.mlx.Network` at `/org/mlx/Network`:

| Member | What it does |
| --- | --- |
| `Devices() -> a(sssssas)` | each link: name, kind (`ethernet`, `wifi`, `other`), state (`unmanaged`, `off`, `no-carrier`, `connecting`, `connected`), address, gateway, name servers |
| `Up(s)`, `Down(s)` | switches a link on or off |
| `Reload()` | reads the configuration again |
| `Changed` | signal: a link changed |

While no NetworkManager owns the name, `mlx-netd` is also
`org.freedesktop.NetworkManager` at `/org/freedesktop/NetworkManager`
with what apps ask to know whether they are online: `state()`,
`CheckConnectivity()`, the `State`, `Connectivity` and
`NetworkingEnabled` properties and the `StateChanged(u)` signal (70 when
a link with a default route is connected).

## mlx-net

```
mlx-net [--bus PATH]                 the links
mlx-net [--bus PATH] up NAME         switches a link on
mlx-net [--bus PATH] down NAME       switches it off
mlx-net [--bus PATH] reload          the configuration again
```

```
eth0      ethernet  connected   10.7.0.50/24        via 10.7.0.1        dns 10.7.0.53 10.7.0.54
eth1      ethernet  connected   10.8.0.5/24         via 10.8.0.1        dns 10.8.0.53
```

## Files

| File | What it holds |
| --- | --- |
| `main.mlx` | the daemon: options, the poll loop, signals |
| `devices.mlx` | the links, their states, addresses, routes, `resolv.conf` |
| `dhcp.mlx` | the DHCPv4 client of one link |
| `config.mlx` | `network.conf` |
| `bus.mlx` | `org.mlx.Network` and the NetworkManager part |
| `system.mlx` | sockets, poll, signals, files, addresses |
| `tool.mlx` | `mlx-net` |

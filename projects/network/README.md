# mlx-netd

The network of MLX/Linux in place of NetworkManager, written in Mlx
without libc. `mlx-netd` follows the links through route netlink
(`std.netlink`), puts them up, and once a link has a carrier configures
it: DHCPv4 or the static settings of its configuration. Wi-Fi cards are
driven through nl80211: scans, joining open and WPA2-Personal networks
with its own supplicant (no wpa_supplicant), saved networks. It sets the
link's address and default route and writes the name servers of all
links into `resolv.conf`. It answers on the system bus (MLXIPC's or
D-Bus's), where `mlx-net`, the top bar and mlx-settings ask it, and
where apps that ask NetworkManager whether they are online get an
answer too.

```sh
mlx4 projects/network/main.mlx -o mlx-netd
mlx4 projects/network/tool.mlx -o mlx-net
tools/check_network.sh                   # the end-to-end check
tools/check_wifi.sh                      # Wi-Fi crypto and WPA2 handshakes
```

`tools/check_wifi.sh` checks the Wi-Fi crypto (`std.sha1`, `std.aes`)
against published vectors, the WPA2 handshakes (`wpa.mlx`) against an
access point's side in python over a veth link, nl80211's scan results
from a dump in the kernel's layout and the saved networks' file. Talking
to a real card (nl80211's scan, connect and keys) needs one, or
`mac80211_hwsim`, and is not part of the checks.

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
| `--state DIR` | `/var/lib/mlx/network` (the saved Wi-Fi networks) |
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

## Wi-Fi

A link nl80211 calls a station is a Wi-Fi card. Its station scans every
30 s while it is not connected (every 2 min while it is, and on `Scan`),
and joins the strongest saved network in sight by itself:

1. nl80211's CONNECT with the network's SSID and access point; for
   WPA2-Personal with our RSN element (CCMP, PSK) and the control port, so
   the card lets nothing but EAPOL through until the port opens.
2. The 4-way handshake over the link's EAPOL socket (`wpa.mlx`): the PTK
   from the PMK and both nonces, message 3's MIC, replay counter and RSN
   element checked against the beacon's (a downgrade is refused), the
   group key unwrapped from its key data.
3. The keys into the card (NEW_KEY), the port opened (SET_STATION
   authorized); then the link is configured like a cable. Group key
   handshakes later bring new group keys.

A network is saved once the handshake proved its passphrase:
`DIR/wifi` holds its SSID and PMK (never the passphrase), mode 0600 in a
folder of mode 0700. A passphrase that does not work (three messages 1
without a message 3, or the access point giving up) marks the network:
it is not joined by itself again until a new passphrase comes.

What it does not speak: WPA3 (SAE) and networks that require management
frame protection, 802.1X (Enterprise), WEP and WPA1, hidden networks.
Without cfg80211 in the kernel there is no nl80211; mlx-netd says so
and drives the cables.

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
| `Devices() -> a(ssssssas)` | each link: name, kind (`ethernet`, `wifi`, `other`), state (`unmanaged`, `off`, `no-carrier`, `connecting`, `connected`), address, gateway, Wi-Fi network, name servers |
| `Up(s)`, `Down(s)` | switches a link on or off |
| `Reload()` | reads the configuration again |
| `Networks() -> a(suussbb)` | the Wi-Fi networks in sight: SSID, signal (0-100), frequency (MHz), security (`open`, `wpa2`, `wpa3`, `enterprise`, `old`), the last problem joining it (`passphrase`, `failed` or empty), saved, connected |
| `Scan()` | the Wi-Fi cards scan now |
| `Connect(ss)` | joins a network: SSID, passphrase (8 to 63 characters, or 64 hex digits; empty for an open or saved one) |
| `Disconnect()` | leaves the Wi-Fi network |
| `Forget(s)` | forgets a saved network |
| `Changed` | signal: a link or the networks changed |

While no NetworkManager owns the name, `mlx-netd` is also
`org.freedesktop.NetworkManager` at `/org/freedesktop/NetworkManager`
with what apps ask to know whether they are online: `state()`,
`CheckConnectivity()`, the `State`, `Connectivity` and
`NetworkingEnabled` properties and the `StateChanged(u)` signal (70 when
a link with a default route is connected).

## On the desktop

The top bar's control centre (`projects/desktop/topbar`, network.mlx)
asks `mlx-netd` when it opens and every second while it is open: its
network tile shows what is connected (a cable before Wi-Fi, a network
being joined), and with a Wi-Fi card it opens the list of networks under
the tiles, the joined one first, then by signal, each with its bars, a
lock or a check. A click joins an open or saved network on the spot
(`Connect` with no passphrase; the row says it is joining, then shows the
check); one that needs a passphrase, or is not supported, opens
mlx-settings' Network page on it (`mlx-settings network SSID`).

mlx-settings' Network page (`projects/desktop/settings/network.mlx`)
shows the links with their addresses, routes and name servers and a
switch each (`Up`, `Down`), the networks in sight (`Search again`:
`Scan`), and for the one chosen its passphrase field (masked) and Join,
or Disconnect and Forget once it is joined. A passphrase refused is said
on its row and asked again.

Both are checked against `tools/network-test/fakenetd.mlx`, a stand-in
for `mlx-netd` on a system bus of the check's own (a cable and four Wi-Fi
networks; it says what it was asked, a passphrase only by its length):
`tools/check_desktop_clients.sh` joins a network from the control centre,
`tools/check_wayland_compositor.sh` joins one from the Network page with
a wrong and then the right passphrase, and forgets it.

## Installing

```sh
tools/install_network.sh --destdir /tmp/stage --enable   # look first
sudo tools/install_network.sh                             # install
sudo tools/install_network.sh --enable                    # and switch over
```

It builds `mlx-netd` and `mlx-net` into `PREFIX/bin` (`--prefix`,
default `/usr/local`) and installs `/etc/mlx/network.conf` (an example,
all commented out; an existing one stays), a systemd unit and an mlx-init
service for `mlx-netd`, D-Bus's policy for `org.mlx.Network` (root owns
the names; everyone may read the links and networks and scan; the
`netdev` group and the user at the console may change them) and
`/var/lib/mlx/network` (mode 0700, the saved networks). The examples are
in `examples/`. Nothing is switched over unless `--enable` says so: then
`mlx-netd.service` is enabled (also as `dbus-org.freedesktop.NetworkManager`)
and NetworkManager, systemd-networkd and wpa_supplicant are masked, the
links `systemctl enable` and `systemctl mask` make; `systemctl unmask`
and `systemctl enable NetworkManager` undo it. `--destdir DIR` stages it
all under DIR instead (packaging, or a look before installing); without
it the script has to run as root.

## mlx-net

```
mlx-net [--bus PATH]                 the links
mlx-net [--bus PATH] up NAME         switches a link on
mlx-net [--bus PATH] down NAME       switches it off
mlx-net [--bus PATH] reload          the configuration again
mlx-net [--bus PATH] wifi            the Wi-Fi networks in sight
mlx-net [--bus PATH] scan            the cards scan now
mlx-net [--bus PATH] connect SSID [PASSPHRASE | -]
                                     joins a network ("-": the passphrase
                                     is stdin's first line)
mlx-net [--bus PATH] disconnect      leaves the Wi-Fi network
mlx-net [--bus PATH] forget SSID     forgets a saved network
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
| `wpa.mlx` | WPA2-Personal: the 4-way and group key handshakes (CCMP, PSK) |
| `nl80211.mlx` | Wi-Fi cards: scans and their results, connect, keys, events |
| `wifi.mlx` | a card's station (scan, join, handshake, keys) and the saved networks |
| `config.mlx` | `network.conf` |
| `bus.mlx` | `org.mlx.Network` and the NetworkManager part |
| `system.mlx` | sockets, poll, signals, files, addresses |
| `tool.mlx` | `mlx-net` |

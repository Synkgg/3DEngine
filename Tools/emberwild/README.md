# Emberwild internet relay

Requires Python 3.11+ on the relay and each player's computer. No third-party
runtime packages are needed. This is a two-player relay for private games.

## Deploy once

Use a public server with a DNS name and a TLS certificate matching that name.
Allow inbound TCP 7443 on the server's firewall. The players do not open inbound
ports. Copy `relay.py` to the server and run under your service manager:

```
python relay.py serve --cert /path/to/fullchain.pem --key /path/to/privkey.pem --port 7443
```

The operator must keep the certificate renewed and restart the relay after
renewal. Run it as an unprivileged user. This repository does not provision a
server, DNS, certificates or a paid hosting subscription.

## Play

Generate a new private room code with `python relay.py room` and share it with
your friend. Use the generated random code, not a memorable public password.

On the host PC:

```
python relay.py connect --server YOUR.RELAY.DOMAIN --room YOUR_RANDOM_ROOM_CODE --role host
```

Wait for **Relay connected**, launch Emberwild, and choose **Host Camp**.
Then the guest runs:

```
python relay.py connect --server YOUR.RELAY.DOMAIN --room YOUR_RANDOM_ROOM_CODE --role guest
```

Wait for **Relay connected** and choose **Join via Relay** in the game. Keep the
terminal helpers open. Ctrl+C stops them. In a standalone package, the helper
lives in `Online/relay.py`. A convenience `Online/Play-Online.ps1` starts/stops
the helper alongside the game; Python must already be installed.

`--ca your-ca.pem` trusts a private certificate authority while still validating
the server hostname. There is intentionally no disable-verification option.
`--port` selects a non-default relay port. The guest loopback endpoint defaults
to UDP 17777; the host game listens on UDP 7777.

## Operational limits

- TLS encrypts each client-to-relay connection. The relay operator can inspect
  traffic; this is not end-to-end encryption between players.
- The room-code hash is a bearer credential inside TLS. Keep the code private.
  Anyone with it can occupy an empty slot. There is no account system.
- 32 simultaneous rooms / 64 connections; two roles per room. Duplicate roles
  are rejected. Frames are bounded to 1400 bytes, handshake lines to 2048 bytes,
  and idle connections expire after 30 seconds. Helpers send keepalives.
- Do not advertise this as an unrestricted public service: add network-level
  connection/rate limits for public hosting. No per-user quotas are implemented.
- Relay failure ends connectivity. Return to the main menu and restart helpers;
  automatic reconnection and host migration are not implemented.
- No hosting credentials or server address have been embedded in the game.

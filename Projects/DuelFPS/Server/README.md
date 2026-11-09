# DuelFPS online play

Select **Mode** beside **Play** for 1v1, 2v2 or 3v3. Play queues for the selected format. When two, four or six players are ready, the lobby starts a dedicated match on the VM and sends each participant a distinct ticket. The VM hosts up to two independent matches by default. The lobby stays reachable on UDP 7780 and assigns each match its own port, 7781 or 7782. Other public groups wait when both slots are full. Private creators receive a busy response. Ending one match frees its slot. Public matches use Freight Depot with five rounds to win and 90-second rounds. Press **Cancel search** to leave the queue.

**Custom game** defaults to **DuelFPS server**. Select **Create server game**, choose the mode, and share the eight-digit code. Other players enter it on the Custom game screen. Switch **Host on** to **LAN** to create or discover a local 1v1 instead.

Eliminate the entire enemy team to score a round. Friendly players block shots without taking damage. Eliminated players return at the next warmup. When time expires, the team with more total remaining health wins the round. Equal health is a draw. All players must vote for a rematch.

## One-time Lightsail deployment

1. Create an Ubuntu 24.04 LTS Lightsail instance with dual-stack networking in the region closest to your players. Attach a static IPv4 address to it. [Lightsail static IP instructions](https://docs.aws.amazon.com/lightsail/latest/userguide/lightsail-create-static-ip.html)
2. In the instance's **Networking → IPv4 firewall**, add a **Custom UDP** rule for destination ports **7780–7782**. Allow the same range through the Ubuntu firewall if enabled. Keep SSH access in place. [Lightsail firewall instructions](https://docs.aws.amazon.com/lightsail/latest/userguide/amazon-lightsail-editing-firewall-rules.html)
3. From PowerShell in the repository root, package and upload the server. Replace the example IP with the VM's public IPv4. If SSH uses a non-default key, add the same `-i` key path to both commands.

```powershell
$vm = 'ubuntu@203.0.113.10'
tar --exclude=__pycache__ -czf out/duelfps-server.tar.gz -C Projects/DuelFPS Server Assets/Scenes Assets/Scripts/Weapons
scp out/duelfps-server.tar.gz "${vm}:duelfps-server.tar.gz"
ssh $vm
```

4. In the VM's SSH shell, install and enable the service. It starts now, starts again on boot, and restarts after a failure. Once it reports **active**, you can close SSH.

```sh
sudo mkdir -p /opt/duelfps
sudo tar -xzf ~/duelfps-server.tar.gz -C /opt/duelfps
sudo chmod -R a+rX /opt/duelfps
sudo install -m 0644 /opt/duelfps/Server/duelfps.service /etc/systemd/system/duelfps.service
sudo systemctl daemon-reload
sudo systemctl enable duelfps
sudo systemctl restart duelfps
systemctl is-active duelfps
```

5. On your development PC, set `address` in `Assets/Scripts/CloudHost.lua` to the VM's public IPv4, leaving `port=7780`. Export and distribute that client build. Players launch it and click **Play**. Test with two clients on separate internet connections. If a connection fails, inspect `sudo journalctl -u duelfps -n 50` on the VM and confirm all three UDP ports are allowed.

The public endpoint is initially blank. Until you deploy the service and set its address, the Play button displays a configuration message. The VM runs the lobby continuously and starts match processes on demand. A match ends if a participant disconnects. When updating a deployment, install the updated server archive, restart the service and distribute a client exported from the same checkout.

To raise the match limit, increase the service's `--max-matches` argument and open the matching contiguous UDP range above 7780. Verify performance under real load first. Restarting the service ends its active matches.

## Developer verification

For an optional loopback test, run `.\Tools\Start-DuelLobby.ps1` in one terminal. In another, set `$env:DUELFPS_TEST_CLOUD='quick'` and run the release CTest suite from the repository root. This test launches two engine runtimes, clicks Play in each, fires a weapon and verifies the shared score. Set the variable to `1` to verify private cloud invites or clear it to run the custom LAN test. Only the test runtimes override the cloud endpoint to use localhost. Stop the test lobby with Ctrl+C.

Use `DUELFPS_TEST_CLOUD=multi` to test twelve clients in two simultaneous 3v3 matches. The test ends one match and verifies that the other can still score. Use `DUELFPS_TEST_CLOUD=2v2` or `3v3` for the four- and six-runtime tests. They select the mode through the menus, join the dedicated server, fire at each opposing player and verify the score on every client. Run `python Server/test_lifecycle.py` from the DuelFPS folder to check mode isolation, private invites, friendly fire, timeout scoring and rematch voting.

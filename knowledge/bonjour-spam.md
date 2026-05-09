# Bonjour Spam

Floods the LAN with fake DNS-SD service announcements over mDNS so nearby devices show phantom AirPlay speakers, Spotify Connect targets, Chromecasts, printers, and SMB workstations. Each phantom resolves to a fake hostname so connect attempts time out.

## Usage

1. Connect to a WiFi network: **WiFi > Network**
2. Open **Bonjour Spam**
3. Toggle individual categories on / off (all enabled by default)
4. Tap **Start Spam** to begin broadcasting; tap again to stop
5. The **Stats** row shows total announcements sent

Phantom devices appear in:
- macOS **Finder → Network** sidebar (Workstation phantoms)
- iOS / macOS **Spotify Connect** picker (Spotify phantoms)
- iOS **Control Center → AirPlay** + **Music app** (AirPlay phantoms)
- Chrome / Android **Cast** picker (Google Cast phantoms)
- macOS / Windows **Add Printer** dialog (Printer phantoms)

Tapping any phantom in those UIs hangs / times out — the SRV record points to a non-existent hostname.

## Categories

| Category | Service Type | Port | Visible in |
|---|---|---|---|
| Spotify Connect | `_spotify-connect._tcp` | 4070 | Spotify app device picker |
| AirPlay | `_airplay._tcp` | 7000 | iOS / macOS AirPlay menu |
| Google Cast | `_googlecast._tcp` | 8009 | Chrome / Android cast menu |
| Printer | `_ipp._tcp` | 631 | OS Add Printer dialog |
| Workstation | `_smb._tcp` | 445 | macOS Finder, Windows Network |

8 phantom names per category = 40 fake devices broadcast every 1.5 s.

## How it works

1. Opens UDP socket bound to ephemeral port
2. Every 1.5 s, sends one mDNS announcement per phantom to multicast `224.0.0.251:5353`
3. Each packet contains:
   - **PTR** record: service type → instance name (e.g. `_airplay._tcp.local` → `Apple TV Office._airplay._tcp.local`)
   - **SRV** record: instance → fake hostname + port (`fake-XXXXXX.local`, with cache-flush bit set)
   - **TXT** record: empty (single zero-length string)
   - **A** record: fake hostname → random IP in our subnet
4. TTL of 120 s on SRV/A means phantoms disappear ~2 minutes after stopping

## Why no AirDrop?

AirDrop discovery uses **Apple Wireless Direct Link (AWDL)** — a peer-to-peer Wi-Fi protocol Apple devices speak directly to each other. AWDL frames can't be sent from a Wi-Fi-only device like an ESP32. Advertising `_airdrop._tcp` over normal mDNS does nothing because the AirDrop UI only consults the AWDL channel.

## Achievements

| Internal name | Display name | Tier |
|---|---|---|
| `wifi_bonjour_spam_first` | Phantom Caller | Bronze |
| `wifi_bonjour_spam_1min`  | Network Mirage | Silver |

## Ethics

Pure annoyance, no real damage — the phantoms vanish ~2 minutes after stopping. Use only on networks where you have permission. Some corporate Wi-Fi blocks multicast or has client isolation, in which case nothing will appear.

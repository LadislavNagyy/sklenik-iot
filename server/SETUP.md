# Instalacny navod — Raspberry Pi server

Tento navod popisuje kompletne nastavenie serverovej casti projektu od cistej instalacie OS.
Server bezi na Raspberry Pi 3B s Home Assistant OS (HAOS).

## 1. Instlacia Home Assistant OS

1. Stiahnut [Raspberry Pi Imager](https://www.raspberrypi.com/software/)
2. Vybrat OS: **Other specific-purpose OS → Home assistants and home automation → Home Assistant**
3. Vybrat SD kartu, nastavit hostname, SSH
4. Naflashovat a bootovat RPi
5. Po ~5 minutach je HA dostupny na `http://homeassistant.local:8123`

## 2. Mosquitto MQTT broker (add-on)

Mosquitto bezi ako HAOS add-on — neinstauje sa manualne, spravuje ho HA supervisor.

1. Settings → Add-ons → Add-on Store → vyhladat **Mosquitto broker** → Install
2. Info tab: zapnut **Start on boot** a **Watchdog**
3. Configuration tab → Logins → Add:
   - username: `mqtt_iot`
   - password: (vlastne heslo)
4. Save → Info tab → Restart

Po pridani loginu Mosquitto automaticky zakaze anonymny pristup (`allow_anonymous false`).
Add-on sa spusta automaticky pri kazdom starte RPi — systemctl nie je potrebne.

Konfiguracia zodpovedajuca nasadeniu je zdokumentovana v `mosquitto/mosquitto.conf` a `mosquitto/acl`.

## 3. MQTT integracia v Home Assistant

1. Settings → Integrations → Add Integration → MQTT
2. HA automaticky detekuje Mosquitto add-on a ponukne internu konfiguraciu — potvrdit
3. Otvorit File Editor add-on (nainstaovat z Add-on Store)
4. Upravit `/config/configuration.yaml` — pridat obsah zo suboru `home-assistant/configuration.yaml`
5. Settings → System → Restart (Core only)

## 4. Databaza — HA Recorder

Home Assistant ma vstavany Recorder, ktory automaticky uklada vsetky entity do SQLite
databazy. Predvolene uchováva historiu 10 dni — nie je potrebna ziadna dalsie nastavenie.

Pre dlhsie uchovavanie pridat do `configuration.yaml`:

```yaml
recorder:
  purge_keep_days: 30
```

Cisenie starej/chybnej historie: Developer Tools → Actions → `recorder.purge` → `keep_days: 0, repack: true`

## 5. Historicke grafy v dashboarde

Dashboard → Edit → Add card → **History Graph**:

```yaml
type: history-graph
title: Sklenik - 24h historia
hours_to_show: 24
entities:
  - entity: sensor.sklenik_teplota
  - entity: sensor.sklenik_tlak_vzduchu
  - entity: sensor.sklenik_vlhkost_pody
  - entity: sensor.sklenik_intenzita_svetla
```

Log sprav ako Logbook karta:

```yaml
type: logbook
title: Sklenik - Log spravy
hours_to_show: 24
entities:
  - sensor.sklenik_log
```

## 6. Automaticky start sluzieb

HAOS resi auto-start automaticky:
- Mosquitto add-on: **Start on boot** toggle v add-on Info tab
- Home Assistant samotny: supervisor zabezpecuje start pri kazdom boot RPi

Overenie po reštarte: HA dashboard dostupny na `http://homeassistant.local:8123`, Mosquitto
zobrazeny ako "Running" v Settings → Add-ons.

## 7. Verejny pristup na dashboard (kritérium 5.4)

### Varianta A — Port forwarding na routeri

1. Router admin panel → Port forwarding
2. Pridat pravidlo: externi port `8123` → interna IP RPi `192.168.1.228:8123`
3. Dashboard bude dostupny na `http://<verejná-IP>:8123`
4. Verejnu IP zistit cez `http://ifconfig.me`

### Varianta B — Nabu Casa (odporucane, bezpecnejsie)

1. HA → Settings → Home Assistant Cloud → Sign up
2. Poskytuje HTTPS URL bez potreby port forwardingu

## 8. Overenie systemu

V HA: Settings → Integrations → MQTT → Mosquitto broker → **Listen to a topic**:
- Topic: `fei/#`
- Start listening

Vypis by mal zobrazovat:
```
fei/sklenik/temperature  {"value":23.45,"unit":"°C","ts":1748000000,"node":"sklenik","sensor":"BMP280"}
fei/sklenik/soil_moisture {"value":62.00,"unit":"%","ts":1748000000,"node":"sklenik","sensor":"soil_sensor"}
fei/sklenik/status  online
```

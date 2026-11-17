# BIOM / Pylontech LV CAN charger: bench commissioning

**Development branch:** `feature/pylontech-bms-psu-topology`
**PlatformIO environment:** `biom_pylontech`
**Status:** engineering prototype; do not use unattended until hardware validation is complete.

## Agreed architecture

- Deye inverter decides when to start/stop the **single-phase generator**.
- Generator AC powers Flatpack2 HE 48 V 2000/3000 W and ESP32-S3.
- BIOM 16S 314Ah LiFePO4 / JK BMS v19 shares its inverter CAN with Deye.
- ESP32-S3 **only listens** on battery CAN using TWAI RX GPIO5, TX GPIO6 with external **3.3V CAN transceiver**, 500 kbit/s.
- Flatpack CAN is on the existing separate MCP2515 interface(s), 125 kbit/s.
- `BatteryManager` retains its existing CC/CV/Float logic and current ramp, but BMS limits and permission checks apply **only** to `PYLONTECH_CAN`.
- The originally agreed default Flatpack NVRAM fallback is **43.7 V**.
- Topology `series` / `parallel` and number of PSUs (1..3) are set in the Web UI and stored in ESP NVS; settings activate on **next reboot**.
- Pylontech 16S is restricted to **one PSU in series**; one, two, or three parallel PSU outputs may be configured. **Use one independently controlled CAN bus per PSU.**
- This revision keeps the existing conservative **41.7A per PSU** limit for both 2kW and 3kW variants; 3000W-specific derating/ratings are not yet calibrated.

## First-time Flatpack installation (DC output DISCONNECTED)

1. Power ESP and one Flatpack2 from protected AC, **without the battery DC cable connected**.
2. Verify Flatpack's CAN login and PSU status, then write default voltage 43.7 V using existing UI.
3. **Power-cycle AC** and measure PSU output directly with a voltmeter **before connecting the battery**. Verify 43.7 V on each PSU; do not assume that a sent CAN write was persisted successfully.
4. Check CAN power-up, loss of ESP, and reboot behavior.
5. Do not connect multiple PSUs in parallel unless manufacturer sharing / current balancing and reverse-feed behavior are validated. Never connect series outputs without checking output isolation and grounding.

## Battery CAN commissioning (no charge output connected)

1. Verify actual BMS profile is `Pylontech LV`, **500 kbit/s** and the intended RJ45 CAN-H/L pinout with the battery manual. Verify polarity and correct two-terminator bus topology. A third stub must be short and must not add a third 120-ohm terminator.
2. Confirm Deye remains functional with ESP TWAI in **listen-only** mode.
3. Observe standard CAN IDs `0x351`, `0x355`, `0x356`, `0x359` and `0x35C`.
4. Compare decoded CVL/CCL, SOC, pack voltage/current, temperature, permission and alarms to the BIOM LCD and Deye. **Especially verify the meaning of 0x359 bits and the current sign**; JK compatibility profiles vary by firmware.
5. Stop CAN traffic: expected behavior is **0 A and 43.7 V** commands within roughly 1-4 seconds. Restore traffic and verify new full validation before enabling the ramp.
6. Clear `Charge Enable`, set BMS charge limit to zero, or introduce a simulated alarm; in each case output commands must return to the safe setpoints.
7. Test `Stop Charging` from Web UI and check that `Start Charging` still requires BMS permission.

## Controlled first charge

1. Start with one PSU, a fused connection, an appropriate DC switch/protection strategy and **limited generator budget**. The proposed DC output isolation/reverse-current scheme was intentionally deferred; physical commissioning still requires verification of reverse feed and power-up safety.
2. Configure **Parallel x1** for 16S battery. Select conservative manual current limit via UI (for example 5 A), enable an appropriate single-phase AC preset, and verify no competing charging source is active.
3. Verify battery current on the BMS independently, PSU output current, measured bus voltage, CAN CVL/CCL and generator AC power using instruments.
4. Test voltage regulation, current limit changes, full-charge transition into existing Float, restart, CAN disconnect and generator stop.
5. For two or three parallel Flatpacks, validate equal current distribution, independent PSU status, DC fuse per module, shared bus wiring and each module's thermal conditions.

## Important outstanding conditions

- **43.7 V is a voltage setpoint, not a galvanic disconnect.** A battery at or below 43.7V can receive current before ESP authorization; this design does not yet implement a guaranteed DC off switch.
- Pylontech `0x351` CCL applies to **battery total charge**, including Deye PV and any other charger. Current software limits **its own** PSU current against CCL but cannot guarantee total charge stays under CCL if Deye simultaneously charges. Before unattended use either ensure exclusive charge sources or add verified total-current accounting/coordination.
- Passive listening assumes BIOM broadcasts all required CAN frames. If `0x359` or `0x35C` is absent/variant, the controller **fails closed** and never enables charge until parser mapping is confirmed with captured traffic.
- Current control at battery voltages below approximately 47V may be inaccurate for some Flatpack2 units. Do not rely on this path for deep-recovery charging without additional testing.
- The ESP WiFi AP/REST API is not authenticated by default. Secure access before production use.
- A CAN timeout while AC remains powered is distinct from a total generator shutdown; under that condition the firmware sends zero current, but firmware software cannot enforce a guaranteed open circuit.
- Generator capacity, PSU variant (2000/3000W), output ORing, fusing, grounding, and inverter backfeed must be checked before high-current testing.

## Build

```sh
pio run -e biom_pylontech
pio run -e biom_pylontech -t buildfs
pio run -e vx1
pio run -e gree_tractor
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude tests/pylontech_decoder_test.cpp -o pylontech-test
./pylontech-test
```

**Upload both firmware and LittleFS**: `pio run -e biom_pylontech -t upload`, then `pio run -e biom_pylontech -t uploadfs`. Confirm USB board/port prior to flashing.

### API

`GET /api/topology`: active mode/count. `POST /api/topology` payload `{"mode":"parallel","count":2}` saves desired wiring; **reboot required**.

`GET /api/battery`: BMS fields `bmsDataValid`, `bmsChargeAllowed`, `bmsChargeVoltageLimitV`, `bmsChargeCurrentLimitA`.

`POST /api/charging` with `{"enabled":false}` sets a temporary (until ESP reboot) manual inhibit. `{"enabled":true}` releases inhibit **but never bypasses BMS checks**.

# Rev A schematic review

The active design is in Hot_Wheels_Electronics/Hot_Wheels_Electronics.kicad_sch on the electronics branch. The original D1 Mini reference circuit is archived separately under archive/.

## Verification

| Check | Result |
| --- | --- |
| KiCad schematic ERC | 0 errors, 0 warnings |
| Active netlist export | Pass; verified test points, PWM sinks, I2C and supplies |
| BOM/footprint audit | 55 component line items, all assigned footprints; project MiniBuck library registered |
| Assembly footprint type | All active BOM footprints are SMD |
| Legacy D1 Mini archival | Complete; 168 active pin connections unchanged at extraction |
| Test points | TP1 +BATT; TP2 +6V; TP3 +3V3; TP4 GND; TP5 bridge-A current ADC; TP6 bridge-B current ADC |
| Functional circuits | Motor driver, dual current sensing, servo power, IMU, ToF, colour, wheel speed, battery/thermistor ADC, ESC, lighting and passive buzzer |

Recheck with these commands from the repository root:

    kicad-cli sch erc Hot_Wheels_Electronics/Hot_Wheels_Electronics.kicad_sch
    kicad-cli sch export netlist --format kicadxml -o /tmp/reva.xml Hot_Wheels_Electronics/Hot_Wheels_Electronics.kicad_sch
    kicad-cli sch export bom -o /tmp/reva-bom.csv Hot_Wheels_Electronics/Hot_Wheels_Electronics.kicad_sch

Zero ERC violations verify KiCad's electrical rules, not current ratings, thermal behavior, completed PCB layout or safe fabrication.

## Hardware implementation

- ESP32-WROOM-32UE-N8 with off-board USB-UART programmer and external antenna.
- BMI160 I2C address 0x68 (CSB high, SDO low), preserving INT1 on GPIO16. Shared 3.3 V SDA/GPIO21 and SCL/GPIO22 also serve ADS1115, TOF400C/VL53L1X and AS7341.
- DRV8833 two brushed bridges, independent 0.1 ohm low-side sense resistors and ADS1115 sensing. The driver reference is 200 mV, so the nominal hardware chopping threshold is around 2 A per bridge; this does not establish motor suitability.
- 6 V servo buck with local 22 uF and 100 nF capacitors.
- Two 3.3 V WS2812B-2020-V6 underglow pixels on GPIO2. White front and red rear body lamps use distinct PWM gates (GPIO17 and GPIO18), two AO3400A low-side switches, one resistor per LED return, and a 5-pin JST ZH harness.
- Direct GPIO19 drive of a 3 V-rated, 5 mA CPT-9019S-SMT-TR piezo.
- Two-wire off-board main switch attaches to small stock SMT solder pads. Motor outputs use keyed SMT Molex Micro-Fit connectors distinct from the JST-PH battery connector.

## Unresolved acceptance checks

1. **Power protection.** There is no inline fuse, reverse-polarity protection or TVS in Rev A. Confirm correct keyed LiPo polarity before applying power. A LiPo can source dangerous fault current; absence of the fuse is not a safety feature. Never power an unverified prototype unattended.
2. **Motor and servo current.** Measure running, startup, stall and reversal current under real mechanical load. Validate motor-driver thermal margin, two sense resistor power ratings, MiniBuck headroom, off-board switch/wiring, connector current rating, PCB copper dimensions and brownout behavior. Servo bulk capacitance is provisional.
3. **Current sensing.** Check shunt/Kelvin layout, ADS1115 signal range, PWM noise, calibration and failure handling with bench measurements before relying on software telemetry or stall detection.
4. **Boot safety.** Check ESP32 strapping pins GPIO2 (LED data) and GPIO15 (ESC), programmer/boot/reset operation, motor-driver control during reset and brownout, and safe firmware failsafe. DRV8833 nSLEEP has an internal 500 kilohm pull-down (TI datasheet); no additional resistor is placed.
5. **Breakout characteristics.** Confirm the purchased TOF400C/AS7341 supply voltage, logic voltage, I2C addresses, onboard I2C pull-ups and ToF interrupt/shutdown reset behavior; test combined I2C load and timing with BMI160.
6. **LED and buzzer operation.** Select actual white/red body LEDs and measure forward drop/output. Four 470 ohm series values are provisional. Confirm PWM, power/noise, underglow V6 supply and piezo current on assembled hardware.
7. **Mechanical/procurement.** Confirm board outline, one-sided reflow, Micro-Fit mating connector clearances, strain relief on SW1 wires, PCB antenna exclusion and external antenna routing. Confirm ESP32-WROOM-32UE-N8 and module-specific mounting details.
8. **Independent review.** Obtain the schematic/connector safety design-review sign-off and resolve the measurements above before fabrication.

## Status

**Electrical schematic assembled and ERC clean. Design-review approval and hardware-dependent checks remain open.** Issue #9 should remain open pending independent review and the required current/thermal/connector validation.

References: [TI DRV8833](https://www.ti.com/lit/ds/symlink/drv8833.pdf), [AO3400A](https://aosmd.com/res/data_sheets/AO3400A.pdf), [CUI CPT-9019S](https://www.digikey.com.au/htmldatasheets/production/1917703/0/0/1/cpt-9019s-smt-tr.pdf), [Worldsemi WS2812B-2020-V6](https://www.world-semi.com/products/ws2812b-2020-v6.html).

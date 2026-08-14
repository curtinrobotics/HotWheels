# Hot Wheels RC Electronics and PCB Specification

## Document control

| Field               | Value                      |
| ------------------- | -------------------------- |
| Status              | Draft requirements capture |
| Target              | 1:43-scale car, PCB Rev A  |
| Baseline controller | ESP32 D1 Mini with USB-C   |
| Last updated        | 11 August 2026             |

## Purpose

This document records the intended features, design constraints and unresolved decisions for the car's electronics and PCB. It is the working reference for the schematic, PCB layout, firmware interfaces and later verification.

The document describes what the design needs to achieve. Component choices, dimensions and pin assignments can change as measurements and prototypes provide better information.

The terms below indicate how settled an item is:

- **Must** means it is required for the first board to be accepted.
- **Should** means it is strongly preferred unless a documented trade-off prevents it.
- **May** means it is optional or intended for a later revision.
- **TBD** means a decision or measurement is still required.

## Product intent

The vehicle is a 1:43-scale RC car intended to combine low-latency driving with telemetry and interactive track features. The PCB will contain the main electronics and form the structural centre of the chassis.

The design should remain modular. The body-mounted lights, forward distance sensor, floor colour sensor, wheel-speed sensor and motor thermistor should connect with cables so their positions can be adjusted and parts can be replaced. Front and rear mechanical sections should attach to the PCB and remain replaceable.

## Confirmed design direction

| Area                       | Decision                                                                         |
| -------------------------- | -------------------------------------------------------------------------------- |
| Vehicle scale              | 1:43                                                                             |
| Main controller            | ESP32 D1 Mini module with USB-C                                                  |
| Battery                    | 2S 600 mAh 20C LiPo, approximately 49 × 18 × 15 mm                               |
| Brushed motor driver       | DRV8833 dual H-bridge                                                            |
| Motor-current sensing      | Onboard sensing of both DRV8833 bridge currents                                  |
| Battery-voltage sensing    | Protected onboard divider connected to an ADC1 input                             |
| Inertial sensing           | Onboard BMI160 three-axis accelerometer and three-axis gyroscope                 |
| Steering supply            | Regulated 6 V                                                                    |
| Steering connection        | Three-pin connector; keyed preferred, with a controlled unkeyed DuPont exception |
| Forward ranging            | Cable-mounted TOF400C module using VL53L1X                                       |
| Floor colour sensing       | Cable-mounted AS7341 V2 module                                                   |
| Motor temperature          | Cable-mounted MF52B 10 kΩ, B3750 NTC thermistor                                  |
| Initial wheel-speed sensor | Cable-mounted 49E Hall sensor and wheel magnet                                   |
| Lighting                   | Dimmable underglow, headlights and taillights                                    |
| Underglow                  | Top-mounted WS2812-family LEDs with a reflector or 3D-printed light redirector   |
| Audio                      | Buzzer for interaction, warnings and find-me operation                           |
| Assembly                   | Reflowed parts on the top side only, suitable for hotplate assembly              |
| Chassis                    | PCB forms the structural centre, with generic front and rear extensions          |
| PCB envelope               | 88 mm long, 44 mm maximum width and 12 mm front/rear arms                        |
| PCB thickness              | 1.6 mm and 1.8 mm candidates; final choice after stiffness and cost comparison   |
| Fasteners                  | M2 thin-head screws inserted from underneath                                     |
| ESP32 mounting             | Removable through standard 2.54 mm double-row SMT socket headers                 |
| Base-station radio         | Direct peer-to-peer control and telemetry without joining the car's access point |

The ESP32 D1 Mini is the Rev A baseline. A bare ESP32 module or an RP2040/RP2350 with Radio Module 2 may be reconsidered later, but those options should not delay the first board.

## System overview

The 2S battery will feed the main input protection and power switch. Raw battery voltage will supply the DRV8833 and optional external ESC power connection. A dedicated regulator will produce 6 V for the steering servo. A separate logic supply will power the ESP32 D1 Mini and compatible sensors.

The ESP32 will handle motor and servo control, lighting, audio, sensor acquisition, Bluetooth controller support, ESP-NOW communication and telemetry. Motor-current, battery-voltage and inertial measurements will also support diagnostics, data logging and optional driver-assist functions.

The 6 V servo rail must not be connected directly to the ESP32 module's 5 V input.

## Main functional requirements

- The PCB must operate as the electronic controller and structural centre of the 1:43-scale car.
- The car must provide proportional steering and proportional forward and reverse motor control.
- Propulsion must stop after communication loss, controller disconnection, ESP32 reset or a serious motor-driver fault.
- The body shell must be removable without desoldering lights or sensors.
- External sensors should be replaceable and positionable through keyed cable connections.
- The car must send live telemetry while being driven.
- Track-marker and power-up features must not make safe manual driving dependent on the central server.
- The board should provide accessible test points for power rails and critical signals.
- The PCB must measure battery voltage, motor current, acceleration and angular velocity.
- Driver assists must be independently configurable and must fail back to normal manual control when their inputs are invalid.

## Wireless control and telemetry

### Operating modes

| Mode                   | Control                                        | Telemetry                                                            | Intended use                |
| ---------------------- | ---------------------------------------------- | -------------------------------------------------------------------- | --------------------------- |
| Base-station mode      | Direct ESP-NOW or equivalent peer-to-peer link | Bidirectional over the same link                                     | Primary Rev A mode          |
| Direct-controller mode | Bluetooth Classic through Bluepad32            | Limited, stored locally or provided through a separately tested link | Standalone and fallback use |
| Development mode       | USB-C programming and serial control tools     | USB-C serial logging                                                 | Bench testing               |

### Radio requirements

- The base station must not connect to the car's access point for control or telemetry, so the base station can retain its normal internet connection.
- ESP-NOW unicast between known peers is the first implementation choice for Rev A.
- The base station should pass telemetry to a computer over USB so the computer keeps its normal internet connection.
- The car must stop if valid control packets are not received within a defined timeout.
- Telemetry loss must not interrupt otherwise valid control.
- Packets must include a car identity, packet type, sequence number, command age and fault state.
- Each car must accept driving commands only from its paired base station during multi-car events.
- The radio channel must be documented and configurable.
- Bluetooth Classic and ESP-NOW share the ESP32 radio. Simultaneous operation must be measured before it is considered supported.

### Initial performance targets

| Measurement                      |                     Initial target |
| -------------------------------- | ---------------------------------: |
| Control update rate              |                    At least 100 Hz |
| Typical command latency          |                      20 ms or less |
| Failsafe stop after lost control |                     100 ms or less |
| Basic telemetry rate             |                     At least 20 Hz |
| Packet-loss measurement          | Continuous, using sequence numbers |

These values are starting targets for testing rather than fixed production limits.

## Battery and power

The selected battery is a 2S 600 mAh 20C LiPo measuring approximately 49 × 18 × 15 mm. Space must also be allowed for the cable, connector, bend radius and removal path.

A 2S LiPo is nominally 7.4 V and reaches 8.4 V when fully charged. All parts connected to the battery rail must tolerate the fully charged voltage.

### Planned power rails

| Rail     | Source                                   | Main loads                                 |
| -------- | ---------------------------------------- | ------------------------------------------ |
| VBAT     | 2S LiPo                                  | DRV8833 and optional ESC power pads        |
| 6V_SERVO | Dedicated buck regulator from VBAT       | Steering servo                             |
| 5V_LOGIC | QITN/Matek BEC or integrated replacement | ESP32 D1 Mini input and compatible modules |
| 3V3      | ESP32 module or dedicated regulator      | ESP32 logic and compatible sensors         |
| GND      | Common return                            | All loads                                  |

The existing QITN/Matek BEC can provide the candidate 5 V logic rail. It does not provide the required 6 V servo rail, so a separate regulator is needed unless the architecture changes.

### Power requirements

- The board must operate across the expected 2S battery range and tolerate 8.4 V at full charge.
- Battery polarity must be keyed and clearly marked.
- The battery connector must not mate with the motor or sensor connectors.
- The switch, protection, copper and connectors must be sized from measured system current.
- Battery voltage must be measurable through a protected divider connected to ADC1.
- Firmware must provide configurable low-battery warning and shutdown thresholds.
- Motor and servo current transients must not reset the ESP32 or corrupt sensor readings.
- Local ceramic and bulk decoupling must be provided at regulators, the motor driver, the servo connection and the ESP32 supply.
- USB-C and battery power must not back-power each other through an unsafe path.
- Reverse-polarity, short-circuit and input transient protection must be reviewed before schematic release.
- The battery-voltage measurement circuit must tolerate at least 8.4 V plus component tolerances without exceeding the ESP32 ADC input range.
- Battery telemetry must distinguish sustained low voltage from short motor- or servo-induced voltage sag.

The battery's 20C label implies an advertised current capability of 12 A. It does not mean the PCB needs to carry 12 A, but it means the battery may deliver destructive current into a fault. Actual motor and servo currents must be measured.

### Battery-voltage monitoring

The protected voltage divider will use an ADC1 input so it remains available while Wi-Fi is active. The input should include suitable filtering and protection, while the divider resistance must avoid unnecessary battery drain and remain compatible with the ADC input requirements.

Firmware will calibrate divider ratio and ADC error against a trusted meter. It should report raw and filtered battery voltage, detect rapid sag under load, and provide configurable warning, power-reduction and shutdown thresholds.

## Brushed motor system

The PCB will include a DRV8833 dual H-bridge.

It should support both:

- one brushed motor with the two bridges connected in parallel for additional current;
- two brushed motors using one independent bridge per motor.

The parallel and independent arrangements may use solder links, zero-ohm resistors or separate assembly variants. The selected arrangement must be obvious, and firmware must not drive the bridges independently while their outputs are tied together.

- Motor running and stall current must be measured before the driver package and copper layout are finalised.
- The DRV8833 must have hardware-defined safe input and nSLEEP states while the ESP32 is resetting.
- The nFAULT output should be routed to the ESP32 if a suitable pin is available.
- Motor connectors and wiring must be rated for measured stall current.
- Motor and battery connectors must be physically incompatible.
- Motor suppression and layout must limit brush noise and inductive transients.
- Each motor configuration must be tested separately for current, temperature, reversing and fault behaviour.
- The PCB must measure the current through both DRV8833 bridges.
- In dual-motor mode, bridge A and bridge B current must be available separately; in parallel-bridge mode, firmware must report their combined motor current.
- The current-measurement range must include normal running, acceleration and stall current with suitable transient margin.
- Current-sense failure or saturation must not be interpreted as a safe zero-current condition.
- Current warnings and software intervention thresholds must be configurable, but firmware must not replace the driver's hardware current regulation and thermal protection.

The current schematic uses the DRV8833PW package. Texas Instruments gives the PW package a lower continuous RMS rating than the thermally enhanced PWP and RTY versions. The final package choice therefore depends on measured current and thermal testing.

### Motor-current measurement

The DRV8833 AISEN and BISEN inputs support low-side sense resistors for bridge current regulation. Rev A should measure both sense paths so one circuit supports either two independent motors or one motor driven by parallel bridges.

The measurement circuitry must not load the sense nodes or change the hardware current-trip point. The schematic should provide Kelvin-connected sensing, appropriate gain and PWM-noise filtering, and test points for comparing the reported current with bench instruments. Current values must be calibrated and included in live telemetry, fault logging and stall detection.

## Optional brushless system

The PCB should retain the brushed driver while also providing an interface for a purchased external brushless ESC.

- The board should provide an external ESC signal and ground connection.
- The initial signal format is expected to be servo-style PWM, subject to the selected ESC.
- Any ESC-provided BEC output must be isolated by default.
- The ESC connection should be removable without affecting steering or sensors.

The interface may use labelled solder pads or a keyed connector, depending on available PCB area.

## Steering

The steering servo may use a 2.54 mm DuPont connector or another keyed three-pin connector. If an unkeyed DuPont connector is used, the pin order must place regulated 6 V on the centre pin, with signal and ground on the outer pins. This keeps the supply contact in the centre if the plug is reversed, but it does not eliminate signal/ground reversal risk. The unkeyed 2.54 mm DuPont connector is the only permitted unkeyed connector for Rev A.

- The 6 V regulator and connector must be sized for measured servo stall current.
- Steering direction, center trim, pulse range and endpoints must be configurable via software.
- Startup and communication-loss behaviour must not drive the linkage beyond its safe range.

## Sensors

### Sensor summary

In this table, “Onboard” means mounted directly on the PCB.

| Function                          | Device                                                   | Interface                                           | Mounting                                 |
| --------------------------------- | -------------------------------------------------------- | --------------------------------------------------- | ---------------------------------------- |
| Wheel speed                       | 49E Hall sensor and wheel magnet                         | Analog                                              | Adjustable cable-mounted sensor          |
| Motor temperature                 | MF52B 10 kΩ, B3750 NTC                                   | Resistive divider to ADC1                           | Attached to the motor                    |
| Motor current                     | DRV8833 bridge sense resistors and measurement circuitry | Analog or dedicated current IC                      | Onboard                                  |
| Forward distance                  | TOF400C with VL53L1X                                     | I2C                                                 | Forward-facing cable-mounted module      |
| Floor colour                      | AS7341 V2                                                | I2C (main sensor) + analog (LED brightness control) | Downward-facing cable-mounted module     |
| Battery voltage                   | Onboard divider                                          | Analog                                              | Onboard                                  |
| Acceleration and angular velocity | BMI160 six-axis IMU                                      | I2C or SPI, with data-ready interrupt preferred     | Top side of the main PCB near its centre |

### Wheel speed

The 49E sensor will connect through a three-pin connector carrying 3.3 V, ground and signal. Its position must be adjustable relative to a magnet recessed into a wheel.

A 49E is a linear Hall sensor rather than a digital switch. The prototype can detect each magnet pass using an analog threshold. A digital Hall switch may replace it later if it gives cleaner pulse timing.

Wheel circumference and magnets per revolution must be configurable. One magnet is acceptable for the first prototype, although low-speed resolution and missed pulses need testing.

### Motor temperature

The MF52B 10 kΩ, B3750 thermistor will attach to the motor and connect through a two-pin cable. The PCB will contain the other resistor in the divider, and the measurement must use ADC1.

Firmware should support calibration, a warning threshold, reduced-power operation and a shutdown threshold. Open and shorted sensor faults should not be interpreted as valid temperatures.

### Forward ranging

The TOF400C/VL53L1X module will be positioned independently from the main PCB. Its connector should provide power, ground, SDA, SCL and, where exposed by the selected module, shutdown and interrupt.

The sensor may provide obstacle warnings, emergency intervention and distance-to-car information. It must not be treated as the only collision-safety mechanism.

Ranging must not block the steering and motor-control loop. Invalid or timed-out readings must not be interpreted as a clear path.

The VL53L1X field of view, timing budget, ambient-light performance and mounting angle need testing on the actual car. A fast short-range mode is likely more useful than its slow maximum-range setting.

### Floor colour and track markers

The AS7341 V2 module will face the track and connect through a cable. It will not be soldered to the underside of the main PCB.

The sensor needs controlled floor clearance and protection from striking the track. A small shroud should reduce interference from underglow, headlights, sunlight and body reflections.

Track markers will use unique three-colour sequences. They may identify track sections, report progress and request random power-ups from a central server.

Marker events should include:

- marker identity;
- local timestamp;
- detection confidence;
- duplicate suppression state.

Marker width, spacing, vehicle speed, illumination and sensor sample rate must be tested together so each colour bar produces enough readings.

### Inertial measurement and driver assists

The PCB will include a BMI160 containing a three-axis accelerometer and three-axis gyroscope. It should be mounted rigidly on the top side near the vehicle centre where practical, with its X and Y axes aligned to the vehicle axes. The sensor orientation and positive-axis directions must be marked on the silkscreen and recorded in firmware.

- The BMI160 supply must be locally decoupled and its placement must minimise interference from the motor driver, regulators, buzzer and high-current traces.
- A data-ready interrupt should be routed to the ESP32; the second interrupt should be routed if GPIO availability permits.
- The initial control-oriented sample-rate target is at least 200 Hz, subject to bus loading and measured noise.
- Firmware must calibrate accelerometer and gyroscope offsets and compensate for the documented PCB mounting orientation.
- Raw acceleration, angular velocity, calibration state and sensor-health information must be available to telemetry and logging.
- Invalid, stale or uncalibrated IMU data must disable dependent assists without interrupting manual steering or throttle control.

The first intended driver-assist experiment is bounded automatic counter-steer during a detected drift. It may use yaw rate, lateral acceleration, wheel speed, driver steering and throttle state. The assist must be disabled by default until calibrated, must limit its steering contribution, must allow immediate driver override and must never command steering beyond the tested mechanical endpoints.

### Shared I2C bus

The VL53L1X, AS7341 and BMI160 may share one I2C bus after the exact breakout-board supply voltages, level shifting, addresses and onboard pull-ups are verified. SPI remains an option for the BMI160 if I2C bus loading or latency is unsuitable for the driver-assist sample rate.

If their connectors are physically compatible, they must use the same safe power, ground, SDA and SCL ordering. Swapping two I2C modules should not damage either one.

The design should use one intentional set of bus pull-ups or configurable pull-ups. Pull-ups already fitted to the modules must be included in the calculation.

## Lighting and audio

### Lighting

| Function   | Device         | Interface                 | Mounting                     |
| ---------- | -------------- | ------------------------- | ---------------------------- |
| Underglow  | WS2812B        | Digital                   | Top side of the main PCB     |
| Headlights | White THT LEDs | Digital (transistor, PWM) | Removable body shell harness |
| Taillights | Red THT LEDs   | Digital (transistor, PWM) | Removable body shell harness |

The underglow will use normal top-mounted WS2812-family LEDs. A reflective surface or simple 3D-printed light guide will redirect some light toward the ground. Uniform illumination is not required for Rev A.

The WS2812 power supply, data-line conditioning and bypass capacitors must follow the exact LED variant's requirements. If a 3.3 V ESP32 signal does not reliably meet the input-high requirement of LEDs powered from 5 V, a logic-level buffer should be included.

The headlights and taillights will be attached to the removable body shell and connect to the PCB through a keyed harness. Their brightness must be independently controllable. Each parallel LED branch needs suitable current limiting.

Lighting power and switching noise must not disturb the colour sensor, radio or ESP32 supply.

### Buzzer

The PCB will include a buzzer for event sounds, warnings and find-me operation. A passive buzzer is preferred if different tones are required.

The buzzer should use a transistor or driver when its current exceeds the safe capability of an ESP32 GPIO. Sound playback must not block motor control or packet handling.

## ESP32 and GPIO planning

The exact D1 Mini supplier and model must be locked before the footprint is released. Boards sold under this name may differ in dimensions, pin labels, regulator design and USB-to-serial circuitry.

The USB-C connector must remain accessible with the body and battery installed. The antenna end needs clearance from the battery, motor, wiring, fasteners and large copper areas.

### Removable SMT header mounting

The D1 Mini will plug into two standard 2×10, 2.54 mm-pitch double-row SMT female socket headers. This produces four rows of ten SMT pads, or 40 pads in total. The module remains removable and must be unplugged before the main PCB is placed on the hotplate for initial assembly or rework.

The current footprint uses 2.15 × 1.55 mm pad envelopes, header-row X positions of ±11.43 and ±13.97 mm, and end-pin Y positions of ±11.43 mm. Its nominal socket height is 5 mm. These dimensions must be checked against the exact purchased header before PCB release, including contact orientation, body width and lead-free reflow temperature rating. A socket that is not reflow-rated must be fitted after hotplate assembly using a suitable manual soldering process.

With the module centred at the PCB origin and USB-C facing forward, the local footprint represents a 30.48 mm-wide module body extending from Y = −19.05 to +17.78 mm. The USB envelope is 10.16 mm wide and extends to Y = +24.13 mm, leaving approximately 0.92 mm per side inside the 12 mm front arm. The printed front mount and body shell must provide additional clearance for the wider moulded body of a USB cable.

The existing rectangular socket courtyard is 32.5 mm wide and extends from Y = −20.05 to +25.15 mm. Its front corners extend beyond the narrow arm even though the physical module ends and only the narrower USB connector continues forward. Before final PCB DRC, this conservative rectangle should be replaced by a stepped body-and-USB mechanical courtyard based on measured parts.

Components below the removable module should be limited to approximately 3 mm in height. The minimum vertical stack before allowing for module components, pin protrusion, insulation or a battery tray is approximately 23.2 mm with a 1.6 mm chassis PCB, or 23.4 mm with a 1.8 mm chassis PCB, when the 15 mm battery is stacked directly above the socketed module.

### Current firmware allocation

|                   Native GPIO | Current use           | Design note                                               |
| ----------------------------: | --------------------- | --------------------------------------------------------- |
|                        GPIO32 | Steering PWM          | ADC1-capable but currently used as an output              |
|                        GPIO25 | Forward motor PWM     | ADC2; unsuitable for analog sensing while Wi-Fi is active |
|                        GPIO33 | Reverse motor PWM     | ADC1-capable but currently used as an output              |
|                        GPIO12 | Motor-driver enable   | ESP32 strapping pin; reset-state bias must be reviewed    |
|                        GPIO21 | I2C SDA candidate     | Confirm on the exact D1 Mini                              |
|                        GPIO22 | I2C SCL candidate     | Confirm on the exact D1 Mini                              |
| Exposed GPIO34, 35, 36 and 39 | ADC1 input candidates | Input-only and without internal pull resistors            |

Battery voltage, thermistor, analog Hall and any directly digitised motor-current measurements must use ADC1 because ADC2 is unavailable while the ESP32 Wi-Fi driver is active.

Battery voltage, motor temperature, wheel speed and two bridge-current signals require five analog channels. The currently identified free ADC1 inputs provide only four channels, so the final pin plan must add a suitable external converter or digital current monitor, multiplex a non-critical signal, or reassign GPIO. Driver-assist operation must not depend on ADC2 becoming available while the radio is active.

Two independently controlled brushed motors require four DRV8833 input signals. The final pin plan must reserve two additional outputs or treat independent dual-motor control as a later assembly variant.

Boot-strapping pins must not be connected to circuits that can force an invalid boot state.

## Connectors

Connectors should be easy to acquire and use, while preventing incorrect insertion, orientation or location that could cause over-voltage, reversed polarity or a short circuit.

### Connector rules

- Every removable connector must be keyed or mechanically unique, except for the steering servo, which may use the controlled unkeyed DuPont arrangement described above.
- Battery and motor connectors must never be mutually compatible.
- Incompatible voltages should use different families, pitches or pin counts.
- If two connectors can mate, their power and ground positions must be compatible.
- Pin one, voltage and function should be marked on the top silkscreen.
- Cable exits must avoid wheels, steering, gears, motor shafts and sharp PCB edges.
- High-current connectors must be selected using measured load current and wire size.

### Candidate connector plan

| Function        |   Pins | Candidate approach                                                                                 | Important constraint                      |
| --------------- | -----: | -------------------------------------------------------------------------------------------------- | ----------------------------------------- |
| Battery         |      2 | Match the pack (JST-PH) or use a dedicated keyed power family                                      | Must be unique on the car                 |
| Brushed motor 1 |      2 | Different family from battery or direct solder pads                                                | Stall current                             |
| Brushed motor 2 |      2 | Same safe motor pinout as motor 1                                                                  | Optional connection                       |
| Servo           |      3 | Keyed 2.0 mm or locking connector; unkeyed 2.54 mm DuPont only under the documented pin-order rule | Servo stall current                       |
| VL53L1X         | 4 to 6 | JST SH or equivalent                                                                               | Include shutdown and interrupt if exposed |
| AS7341          | 4 to 6 | JST SH or equivalent                                                                               | Confirm illumination and interrupt needs  |
| Hall sensor     |      3 | Small keyed connector                                                                              | 3.3 V, ground and signal                  |
| Thermistor      |      2 | Small keyed connector unlike battery and motor                                                     | Passive sensor only                       |
| Body lighting   |    TBD | One keyed multi-pin connector                                                                      | Body removal and current limiting         |
| External ESC    | 2 or 3 | Keyed signal connector or labelled pads                                                            | BEC isolated by default                   |

JST PH is rated for more current than JST SH and may suit some power connections. JST SH is smaller and may suit sensors. Final selection depends on measured current, component height, cable availability and assembly effort.

## PCB as chassis

The board will have a broad central electronics section with narrower front and rear structural extensions. Replaceable 3D-printed sections will attach to these extensions, allowing the mechanical layout or wheelbase to change without redesigning the electronics section.

### Coordinate system and Rev A outline

PCB coordinates use the board centre as the origin, positive Y toward the front and positive X toward the vehicle's right side. The Rev A outline is symmetric about both centre axes.

| Feature                   |                                     Rev A value |
| ------------------------- | ----------------------------------------------: |
| Model body length         |                                          114 mm |
| Measured model wheelbase  |                                           64 mm |
| Front track               |                                           40 mm |
| Maximum tyre envelope     |                 21.5 mm diameter × 8.7 mm width |
| PCB length                |                   88 mm, from Y = −44 to +44 mm |
| Maximum PCB width         |                   44 mm, from X = −22 to +22 mm |
| Centre rectangle          | 44 mm wide × 44 mm long, from Y = −22 to +22 mm |
| Front and rear arm width  |                                           12 mm |
| Front and rear arm length |                   22 mm, from Y = ±22 to ±44 mm |
| Candidate PCB thickness   |                                1.6 mm or 1.8 mm |

The Rev A outline is a simple square-ended shape. The 44 mm-wide centre rectangle ends at Y = ±22 mm, where it steps directly to the 12 mm-wide front and rear arms. The arm centreline remains X = 0, and the front and rear axle coordinates are Y = +32 mm and Y = −32 mm respectively.

The front and rear arms provide the structural interfaces for the replaceable 3D-printed sections. Fillets are not required for Rev A; a later mechanical revision may add them only if they preserve the hole pattern, component clearances and overall envelope.

### Mounting-hole pattern

All Rev A chassis holes are Ø2.3 mm non-plated holes for M2 fasteners. Hole coordinates denote centres.

| Group                 | X coordinates |  Y coordinates | Quantity |
| --------------------- | ------------: | -------------: | -------: |
| Front arm             |         ±3 mm | +26 and +38 mm |        4 |
| Rear arm              |         ±3 mm | −26 and −38 mm |        4 |
| Main electronics area |        ±19 mm |         ±10 mm |        4 |

The pattern contains 12 holes in total. A nominal Ø4 mm fastener envelope is reserved around each hole. The main and arm holes retain 1.85 mm of PCB material from the drill edge to the nearest side edge, and the nominal fastener envelope leaves 1.0 mm of side clearance. The main-hole envelope remains approximately 0.75 mm outside the current 32.5 mm-wide ESP32 socket courtyard. Thin-head screws enter from underneath. Standard M2 washers are unlikely to fit on the 12 mm-wide arms; actual screw heads, hex nuts and washers must be measured before the printed interfaces are finalised.

### Steering envelope

The initial steering target is 60° at the inner front wheel, with 65° used as the mechanical-clearance test. Using the 64 mm wheelbase, 40 mm front track, maximum tyre envelope and a provisional 0.5 mm inboard kingpin offset, the calculated tyre-to-arm clearances are approximately 2.27 mm at 60° and 2.13 mm at 65°. The result must be recalculated using the final kingpin and linkage geometry.

### External sensor mounting

The TOF400C/VL53L1X is nominally centred near Y = +51 mm on a vertical 3D-printed bracket attached to the front arm. The module and bracket must remain within the body envelope, avoid the steering mechanism and provide a clear forward field of view.

The AS7341 is nominally centred near Y = +8 mm on a separate downward-facing bracket beneath the chassis. It connects by cable to a top-side PCB connector and is not reflowed onto the underside. Its bracket must control floor clearance, protect the sensor from track strikes and provide an unobstructed optical opening.

- The outline must use a generic centre section with front and rear mounting extensions.
- Rev A must use simple square transitions into the narrower extensions; later fillets are optional and must preserve the defined envelope and hole pattern.
- Extension interfaces must use repeatable M2 hole patterns that can be referenced in CAD.
- M2 thin-head screws must enter from underneath to minimise lost ground clearance.
- Mounting holes should be non-plated unless an electrical connection is intentional.
- Nuts, inserts or printed bosses must support the board without crushing or flexing it.
- Rev A must use a 1.6 mm or 1.8 mm PCB after comparing stiffness, mass, availability and manufacturing cost.
- The outline must clear the battery, wheels, steering, drivetrain, body, wiring, sensors and USB cable.
- The internal shell width must clear the 44 mm PCB plus assembly and impact tolerance.
- The 12-hole pattern and coordinate origin must be shared with the 3D-printed part CAD.
- Steering clearance must be validated through the complete 65° inner-wheel test envelope before PCB release.

The exact choice between 1.6 mm and 1.8 mm remains open until a fabrication quote and chassis flex comparison are available. The selected value must then be used consistently by Onshape, KiCad and the manufacturing notes.

## Assembly constraints

- All reflowed components, connectors and PCB-mounted LEDs must be on the top side.
- The underside must remain component-free because it forms the lowest chassis surface.
- Through-hole leads and solder joints must not project into the ground-clearance envelope.
- The board must support one-pass hotplate assembly.
- Large modules and connectors need measured footprints, courtyards and height checks.
- The ESP32 antenna keep-out must be maintained even though the PCB is structural.
- The D1 Mini, battery and cable-mounted modules must be removed before hotplate assembly or rework.
- SMT socket bodies must be rated for the selected solder-paste reflow profile or be installed after hotplate reflow.

## Telemetry and event data

The initial telemetry format should be able to report:

- battery voltage and low-battery state;
- commanded and applied throttle and steering;
- wheel speed and estimated distance;
- bridge A, bridge B and combined motor current as applicable;
- motor temperature;
- three-axis acceleration, three-axis angular velocity and IMU calibration state;
- driver-assist enabled, active, requested correction and intervention-limit state;
- forward range and range-valid status;
- detected marker identity and confidence;
- radio signal or link-quality information;
- packet sequence, packet loss and command age;
- motor-driver, sensor, brownout and failsafe faults;
- lighting, buzzer and power-up state.

Track-marker events should include the car identity, marker identity, local timestamp, detection confidence and duplicate-suppression state.

The central server may assign power-ups, but the car must continue safe manual driving when the server or telemetry path is unavailable.

## Safety and failure behaviour

The design must define and test its response to:

- reversed battery connection;
- fully charged 2S input;
- low battery and sudden battery sag;
- motor stall or locked drivetrain;
- motor-current measurement saturation, disconnection or implausible readings;
- servo stall or steering jam;
- disconnected or shorted motor, servo, sensors and lights;
- ESP32 reset or firmware crash;
- Bluetooth loss and ESP-NOW packet loss;
- DRV8833 fault conditions;
- invalid or timed-out distance readings;
- invalid, stale or uncalibrated IMU readings while a driver assist is enabled;
- erroneous drift detection or counter-steer intervention;
- false, partial, duplicate and missed colour markers;
- USB-C and battery power being connected together;
- a connector being inserted into the wrong socket;
- PCB flex, impact and loose fasteners.

Propulsion must fail safe to stopped. Lighting, audio, telemetry and sensor faults must not prevent the failsafe from operating.

## Verification

### Electrical bring-up

1. Inspect polarity, footprints, solder joints, connector orientation, configuration links and the antenna keep-out.
2. Check resistance between all power rails and ground before power-up.
3. Power the board from a current-limited supply without the ESP32, servo, motor or sensors connected.
4. Verify every regulator across the expected battery range and representative load.
5. Verify USB and battery power-path behaviour before connecting both together.

### Functional testing

1. Verify safe motor states during power-up, reset, programming and firmware failure.
2. Test steering centre, endpoints, stall current and rapid direction changes.
3. Calibrate both motor-current channels against bench instruments and test running, acceleration, reversing and stall conditions.
4. Test each brushed configuration for current, reversing, fault handling and temperature.
5. Test the external ESC interface with its BEC isolated.
6. Check every connector against reverse insertion and plausible wrong-port insertion.
7. Operate lighting and the buzzer while monitoring ESP32 supply stability and radio packet loss.

### Sensor and radio testing

1. Calibrate Hall threshold, wheel circumference, missed pulses and speed accuracy.
2. Calibrate the thermistor and verify open and short fault detection.
3. Calibrate battery-voltage measurement across the expected battery range and characterise load-induced sag.
4. Calibrate BMI160 offsets, axis orientation, vibration sensitivity, sample timing and data-ready behaviour on the assembled car.
5. Validate counter-steer progressively with the wheels raised, then at low speed, while checking driver override and assist-disable behaviour.
6. Test VL53L1X behaviour with different targets, angles, sunlight and vibration.
7. Determine AS7341 illumination, floor clearance, sample rate and marker dimensions on the real track.
8. Measure ESP-NOW latency, jitter, packet loss, recovery and failsafe timing.
9. Test Bluetooth mode separately before attempting simultaneous Bluetooth and ESP-NOW operation.

### Mechanical testing

1. Fit-check the battery, body, motor, servo, wiring, USB cable and sensors.
2. Measure ground clearance below the screw heads.
3. Test PCB flex with the car supported at the front and rear mounts.
4. Inspect the narrow extension transitions and mounting holes after impacts.
5. Compare 1.6 mm and 1.8 mm PCB thicknesses before selecting the production stack-up.

## Remaining measurements and decisions

The design can continue without answering these immediately, but they are needed before the schematic and PCB are frozen:

1. Exact ESP32 D1 Mini and 2.54 mm SMT socket suppliers, dimensions, height, pinout, reflow rating and onboard power circuit.
2. Drive motor model, quantity, voltage, no-load current, loaded current and stall current.
3. Steering servo model, operating range, running current and stall current at 6 V.
4. Battery connector, wire gauge, cable exit and any built-in protection.
5. Body-shell internal width and height, USB cable access, and fit of the printed front, rear and sensor brackets around the defined PCB envelope.
6. Default brushed configuration: one parallel-bridge motor or two independent motors.
7. Exact TOF400C and AS7341 boards, including voltage range, pinout and onboard pull-ups.
8. Number and placement of WS2812 LEDs.
9. Headlight and taillight part numbers, current and wiring arrangement.
10. Final choice between 1.6 mm and 1.8 mm PCB thickness, plus copper weight and manufacturing tolerances.
11. Motor-current measurement topology, amplifier or converter, full-scale range, shunt values and calibration method.
12. BMI160 interface, interrupt allocation, exact placement and whether a newer IMU should be footprint-compatible as a procurement fallback.

## References

- [ESP32 Series datasheet](https://www.espressif.com/documentation/esp32_datasheet_en.pdf)
- [ESP32 ADC limitations](https://docs.espressif.com/projects/esp-idf/en/v4.2/esp32/api-reference/peripherals/adc.html)
- [Arduino-ESP32 ESP-NOW documentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/espnow.html)
- [Espressif ESP-NOW examples](https://github.com/espressif/esp-now)
- [Bluepad32](https://github.com/ricardoquesada/bluepad32)
- [Texas Instruments DRV8833](https://www.ti.com/product/DRV8833)
- [Bosch Sensortec BMI160 datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi160-ds000.pdf)
- [STMicroelectronics VL53L1X](https://www.st.com/en/imaging-and-photonics-solutions/vl53l1x.html)
- [Pololu VL53L1X Arduino library](https://github.com/pololu/vl53l1x-arduino)
- [ams OSRAM AS7341](https://ams-osram.com/products/sensor-solutions/ambient-light-color-spectral-proximity-sensors/ams-as7341-11-channel-spectral-color-sensor)
- [Adafruit AS7341 Arduino library](https://github.com/adafruit/Adafruit_AS7341)
- [JST PH connector specifications](https://www.jst-mfg.com/product/index.php?lang=2&series=199)
- [JST SH connector specifications](https://www.jst-mfg.com/product/index.php?series=231)

# Component Selection Note: Microphone & Analog Signal Conditioning

## Knowles SPH8878LR5H-1 (SparkFun BOB-19389 Breakout)
* Microphone Type: Omnidirectional Analog MEMS Microphone (Breakout Module)
* Signal-to-Noise Ratio (SNR): 65 dBA
* Current Consumption: ~250 µA (Active mode)
* Sensitivity: -38 dBV/Pa (Differential) / -44 dBV/Pa (Single-ended)
* Power Supply Voltage: 2.3V – 3.6V
* Local Availability: High (In-stock at domestic suppliers like Botland and Kamami)

### Justification for Selection:
1. Rapid Prototyping & BPF Integration:
   Integrated on the SparkFun BOB-19389 breakout board, this component allows immediate breadboard testing and seamless wiring into the external analog Band-Pass Filter (100 Hz – 2000 Hz) and hardware trigger circuit without requiring custom SMD soldering.
2. Ultra-Low Power Architecture Support (Hardware Wake-on-Sound):
   Provides an analog signal suitable for continuous background monitoring through an active analog filter and comparator stage. The main MCU remains in Deep Sleep (< 10 µA) and is awakened via GPIO interrupt only when vehicle audio amplitude exceeds the target threshold.
3. Acoustic Performance in Forest Conditions:
   Features a 65 dBA SNR and high acoustic overload point, allowing the sensor to detect faint low-frequency combustion engine harmonics (quads, cross bikes) at distances exceeding 100 meters while filtering out self-noise.
4. Supply Chain & Market Availability:
   Replaces legacy/discontinued components (e.g., ICS-40180) with a modern, actively produced MEMS chip fully available through domestic IoT distributors in Poland.

---

## STMicroelectronics MP23ABS1TR (Ultra-Low-Power Analog MEMS)
* Microphone Type: Omnidirectional Analog MEMS Microphone (Bottom-port SMD)
* Signal-to-Noise Ratio (SNR): 64 dBA
* Current Consumption: ~120 µA – 150 µA (Ultra-low power active mode)
* Sensitivity: -38 dBV/Pa ± 1 dB
* Power Supply Voltage: 1.52V – 3.6V
* Local Availability: High (Raw component in stock at Kamami, TME, and Farnell PL)

### Justification for Selection:
1. Exceptional Energy Efficiency for Custom PCB Production:
   With an active supply current of only ~120 µA (nearly 50% lower than standard MEMS microphones), this chip is the optimal choice for the final custom PCB revision to achieve multi-year battery lifetime.
2. Direct Analog BPF & Comparator Interfacing:
   Outputs a single-ended analog voltage directly compatible with low-power operational amplifiers (e.g., TLV8542) used in the 100 Hz – 2000 Hz analog band-pass conditioning pipeline.
3. High SNR and Acoustic Sensitivity:
   Delivers 64 dBA SNR and -38 dBV/Pa sensitivity, maintaining high detection reliability for engine noise over long acoustic paths in forest environments.
4. Compact Industrial Design & Wide Temperature Range:
   Packaged in a miniature metal-cap RHLGA enclosure guaranteed to operate from -40°C to +85°C, ensuring environmental durability inside ruggedized outdoor enclosures.
# Theoretical informations and research for microphone requirements


## Noise source profile
Internal combustion engine vehicles generate a specific acoustic profile:

- **Main Frequency Range:** Most of the acoustic energy from an internal combustion engine (piston operation, exhaust) is concentrated in the low-to-medium frequency band (80 Hz – 1200 Hz).

- **Fundamental Frequency (Exhaust/Engine):** For single-cylinder engines revving between 3000 and 8000 RPM, the fundamental frequency of cylinder explosions and exhaust pulses typically falls within 100 Hz – 400 Hz (along with harmonics up to approx. 1–1.5 kHz).

- **High Frequencies (Chain, Tire Friction, Ground Impact):** Frequencies above 2–3 kHz are present but attenuate very rapidly in a forest environment.

## Sound Propagation Parameters in Forest Environments

### Attenuation by Trunks, Branches, and Leaves (Frequency Selectivity)
Sound attenuation in forests depends on the wavelength $\lambda = \frac{c}{f}$ (where $c \approx 343\text{ m/s}$):
- **Low Frequencies ($< 500\text{ Hz}$, wavelength $> 70\text{ cm}$):** The wavelength is larger than or comparable to the diameter of most tree trunks. Diffraction allows low frequencies to bend around tree trunks and travel significant distances with minimal attenuation from vegetation.
- **High Frequencies ($> 1.5 - 2\text{ kHz}$, wavelength $< 20\text{ cm}$):** Wavelengths strike tree trunks, branches, and foliage, suffering heavy scattering and absorption. High-frequency attenuation in forests incurs an additional 10–20 dB loss per 100 meters beyond standard geometric spreading.

### Ground Effect
- Soft forest floor, moss, and soil are porous materials that absorb sound effectively.
- When both source and receiver are close to the ground, destructive interference between the direct wave and the ground-reflected wave causes severe attenuation in the 200–800 Hz range.
- Conclusion: Mounting the microphone at a height of 1.5 – 2.5 meters (e.g., on a tree trunk) bypasses ground attenuation and captures a significantly stronger signal.

### Sound Level Decay with Distance (Inverse-Square Law)
- In free space, sound pressure drops by 6 dB for every doubling of distance.
- A quad/cross bike noise at 7 meters is approx. 85–100 dBA.
- Ambient forest background noise (wind, birds) is typically 30–42 dBA.
- Accounting for geometric spreading and forest structure, vehicle noise in the 100–800 Hz band remains clearly above background noise even at distances of 80–150 meters (and up to 200 meters along forest paths and trails).

### Wind Noise Interference
- Wind blowing through the canopy produces ambient noise, but direct airflow over the microphone capsule creates strong low-frequency turbulent noise (0–100 Hz) that can clip the amplifier.
- A physical windscreen (foam or deadcat windscreen) is mandatory.

## Conclusions for Microphone Selection and Energy Efficiency
- **Frequency Response:** 100 Hz – 2000 Hz (max 4000 Hz). Higher frequencies do not travel far in forests. Restricting the bandwidth allows lowering the ADC sampling rate (e.g., to 8–11 kHz), dramatically reducing microcontroller power consumption and processing overhead.
- **Sensitivity & SNR:** SNR > 60–65 dB, high sensitivity (e.g., -26 dBFS to -38 dBV) to detect faint signals from $>100\text{ m}$.
- **Directivity:** Omnidirectional to listen in all directions around the tree.
- **Microphone Type:** Digital MEMS (I2S/PDM) or Analog MEMS. MEMS mics are tiny, moisture/vibration resistant, and draw negligible current ($< 1\text{ mA}$ active, $\mu\text{A}$ in sleep mode).
- **Power Saving Strategy:** Hardware interrupt triggering (Wake-on-Sound / Threshold). The microcontroller (e.g., ESP32-S3, nRF52, STM32L4) sleeps most of the time and only wakes up when the analog circuit or digital microphone detects a sound level threshold spike in the target band.

## Estimated Sensor Spacing
- Along forest paths/roads: Place sensors every 150 – 200 meters.
- Dense forest coverage (area monitoring): Grid spacing of 100 – 150 meters.
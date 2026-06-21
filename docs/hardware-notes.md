# Hardware notes, TC4D7 Lite Kit

Facts verified on the actual board over the on-board DAP debugger.

## Board and chip

- Kit `KIT_A3G_TC4D7_LITE`, AURIX TC4Dx
- `device_type` 0x10225083 (reads back as TC4Dx)
- Cores are TriCore 1.8. The TC4x PPU vector unit is a separate ISA and is out of scope for the TriCore toolchain.

## On-board debugger over USB

- Enumerates as USB id `058b:0043`, manufacturer IFX, product "DAS JDS AURIX LITE KIT V1.0 (TC4D7)"
- It is an FTDI FT2232-class device with an Infineon-reprogrammed vendor id, not a generic FTDI `0403` id
- Interface 0 is the DAP and JTAG channel used by `tas_server`
- Interface 1 is a UART console bound by `ftdi_sio` and exposed as `/dev/ttyUSB0`

## Memory

- LMU RAM around 0x70000000 is readable and writable over DAP, used as a scratch region in early tests

## User LEDs

LED1 and LED2 are driven directly by TC4D7 GPIO. Both are low-active, so driving the pin LOW turns the LED ON.

- LED1 is P03.9
- LED2 is P03.10

LED4, LED5, LED6 hang off the FTDI side or ESR0 and are not plain GPIO.

## Port P03 register map (from iLLD TC4Dx)

The TC4x port layout differs from TC3x. Addresses below are absolute.

| Register | Address | Notes |
| --- | --- | --- |
| `P03_ID` | 0xF003AC00 | module base |
| `P03_OUT` | 0xF003AC20 | output value, P9 is bit 9, P10 is bit 10 |
| `P03_IN` | 0xF003AC24 | pin level read-back, P9 is bit 9, P10 is bit 10 |
| `P03_OMR` | 0xF003AC3C | atomic set and clear |
| `P03_PADCFG9_GPIO` | 0xF003AF90 | per-pin state, SET bit 2, CLR bit 3, IN bit 8 |
| `P03_PADCFG9_DRVCFG` | 0xF003AF94 | DIR bit 0, OD bit 1, MODE bits 4 to 7 |
| `P03_PADCFG10_GPIO` | 0xF003AFA0 | per-pin state for pin 10 |
| `P03_PADCFG10_DRVCFG` | 0xF003AFA4 | direction and mode for pin 10 |

PADCFG registers stride by 0x10 per pin from `P03_PADCFG0_GPIO` at 0xF003AF00.

### OMR bit positions

- PS9 bit 9, drive P03.9 high (LED1 off)
- PCL9 bit 25, drive P03.9 low (LED1 on)
- PS10 bit 10, drive P03.10 high (LED2 off)
- PCL10 bit 26, drive P03.10 low (LED2 on)

### Configure a pin as output

Write DRVCFG with DIR set to 1 and OD set to 0 for push-pull general output. The value is 0x00000001. After a clean reset the port access protection is open, so DAP writes land. Read DRVCFG back and confirm DIR is 1 to verify.

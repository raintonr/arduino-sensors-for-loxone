# Arduino Sensors for Loxone

Use an Arduino to present various sensor readings on a legacy Dallas 1-Wire bus. Primarily designed to interface with Loxone, but could work with other systems too.

Currently provided:

- MQ135 presented as single DS18B20.
- SHT31 temperature & humidity with 1 minute deltas presented as single DS2438.

**Note:** [OneWireHub](https://github.com/orgua/OneWireHub/) DS2438 emulation with Loxone was fixed with PR [#97](https://github.com/orgua/OneWireHub/pull/97). Until that is merged, use this alternate branch of the OneWireHub library:

https://github.com/raintonr/OneWireHub/tree/issue/65/DS2438Loxone

## Sensors supported by Loxone & their resolution 

- DS1822: 1 x 12 bit
- DS18B20: 1 x 12 bit
- DS18S20: 1 x 9 bit
- DS2438: 1 x 13 bit (0-8191), 1 x 11 bit (0-2048), 2 x 10 bit (0-1023)

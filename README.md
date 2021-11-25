# Variable Output Power Supply 2



## Feature

- Variable regulated power supply with electronically controlled fine adjustment function.
- Variable electronic load with electronically controlled fine adjustment function.
- Controlled with seeeduino xiao.



## Target device

| Device  | I2C addr | Use for                     |
| ------- | -------- | --------------------------- |
| MCP4725 | 0x60     | 出力の基準電圧生成用 DAC    |
| INA226  | 0x40     | 出力電圧/電流の計測用       |
| SSD1306 | 0x3C     | OLED ディスプレイ           |
| MCP4725 | 0x61     | 電子負荷の基準電圧生成用    |
| INA226  | 0x44     | 電子負荷の電圧/電流の計測用 |


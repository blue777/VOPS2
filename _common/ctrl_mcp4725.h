//	g++ -Ofast -std=c++11 ina219.c -o ina219.o -lpthread `freetype-config --cflags` `freetype-config --libs`


#include "ctrl_i2c.h"



class i2c_mcp4725
{
public:
    i2c_mcp4725( int a0=0 ) :
        m_i2c(0x60 | a0)
    {
        SetValue(0);
        delay(10);

        for (int i = 0; i < 10; i++)
        {
            uint8_t  r_data[3] = { 0 };
            m_i2c.read(r_data, sizeof(r_data));
            // initial EEPROM value
            //  0xC0 0x80 0x00
            printf("MCP4725 read 0x%02x 0x%02x 0x%02x\n", r_data[0], r_data[1], r_data[2] );

            if (((r_data[0] & 0x40) != 0x40) ||
                ((r_data[0] & 0x06) != 0x00) ||
                ((r_data[1] & 0xFF) != 0x00) ||
                ((r_data[2] & 0xF0) != 0x00))
            {
                // Write DAC Register and EEPROM
                m_i2c.write({ 0x62, 0x00, 0x00 });

                printf("MCP4725 Write EPROM.\n");
                delay(10);
            }
            else
            {
                printf("MCP4725 initialize OK!\n");
                break;
            }
        }
    }

    void  SetValue(uint16_t value)
    {
        m_i2c.write({ (uint8_t)(0x0F & (value >> 12)), (uint8_t)(0xFF & (value >> 4)) });
    }

    uint16_t GetValue()
   {
            uint8_t  r_data[3] = { 0 };
            m_i2c.read(r_data, sizeof(r_data));
           return (r_data[1] << 8) | (0xF0 & r_data[2]);
    }

protected:
    ctrl_i2c    m_i2c;
};


class i2c_mcp4726 : public ctrl_i2c
{
public:
    i2c_mcp4726() : ctrl_i2c(0x60)
    {
    }

    void  SetValue(uint16_t value)
    {
        uint8_t reg = 0x40;

        // Vref
        // 0: VDD                           (6.4v max)
        // 2: Vref with internally buffer   (5.2v max)
        // 3: Vref unbuffered               (5.2v max)
        reg |= 3 << 3;

        // PD
        // 0: Normal opertion
        // 1: 1k ohm resistor to ground
        // 2: 125k
        // 3: 640k
        reg |= 0 << 1;

        // Gain
        // 0: x1
        // 1: x2
        reg |= 1 << 0;

        // 6.2 Write Volatile Memory
        // 0
        // 1
        // 0
        // VREF1
        // VREF0
        // PD1
        // PD0
        // G
        uint8_t  w_data[3] = { reg, (uint8_t)(0xFF & (value >> 8)), (uint8_t)(0xFF & value) };
        write(w_data, sizeof(w_data));
    }
};

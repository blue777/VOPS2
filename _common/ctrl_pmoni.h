//	g++ -Ofast -std=c++11 ina219.c -o ina219.o -lpthread `freetype-config --cflags` `freetype-config --libs`


#include "ctrl_i2c.h"

class ctrl_PowerMonitor
{
public:
	ctrl_PowerMonitor()
	{
		m_dShuntReg			= 0.002;
		m_dCalibExpected	= 1;
		m_dCalibMeasured	= 1;
	}

	void	SetShuntValue( double shuntreg, double expected = 1, double measured = 1)
	{
		// nomally, expected < measured
		m_dShuntReg			= shuntreg;
		m_dCalibExpected	= expected;
		m_dCalibMeasured	= measured;
	};

	virtual	double	GetA()
	{
		return	ReadShuntRaw() * GetAmpereOf1LSB();
	}

	virtual	double	GetV()
	{
		return	ReadVoltageRaw() * GetVoltageOf1LSB();
	}

	virtual	void	SetSamplingPeriod( int msec )=0;
	
	virtual	double	GetShuntOf1LSB()=0;
	virtual	double	GetVoltageOf1LSB()=0;

	double	GetAmpereOf1LSB()
	{
		const double	s_reg	= m_dShuntReg * m_dCalibMeasured / m_dCalibExpected;

		return	GetShuntOf1LSB() / s_reg;
	}

	virtual	int16_t	ReadShuntRaw()=0;
	virtual	int16_t	ReadVoltageRaw()=0;

protected:
	double		m_dShuntReg;
	double		m_dCalibExpected;
	double		m_dCalibMeasured;
};


class PMoni_INA226 : public ctrl_PowerMonitor
{
public:
	enum ALERT_FUNC
	{
		ALERT_SHUNT_OVER_VOLT		= 0x8000,
		ALERT_SHUNT_UNDER_VOLT		= 0x4000,
		ALERT_BUS_OVER_VOLT			= 0x2000,
		ALERT_BUS_UNDER_VOLT		= 0x1000,
	};
	
	PMoni_INA226( int a1=0, int a0=0 ) : m_i2c( 0x40 | (a1 << 2) | a0 )
	{
		// reg = m_dShuntReg * m_dCalibMeasured / m_dCalibExpected
		m_dShuntReg			= 0.005;
		m_dCalibMeasured	= 1;
		m_dCalibExpected	= 1;
	}
	
	void	SetAlertFunc( enum ALERT_FUNC func, int16_t value )
	{
		uint8_t  w_data6[3]	= { 0x06, (uint8_t)(0xFF & (func  >> 8)), (uint8_t)(0xFF & func) };
		uint8_t  w_data7[3]	= { 0x07, (uint8_t)(0xFF & (value >> 8)), (uint8_t)(0xFF & value) };
 
		m_i2c.write( w_data6, sizeof(w_data6) );
		m_i2c.write( w_data7, sizeof(w_data7) );
	}

	virtual	void	SetSamplingPeriod( int msec )
	{
		int		usec	= msec * 1000;
		int		avg_table[]		= { 1, 4, 16, 64, 128, 256, 512, 1024 };
		int		avg_reg			= 0; 
		int		ct_table[]		= { 140, 204, 332, 588, 1100, 2116, 4156, 8244 };
		int		ct_reg			= 0;

#if 0
		int		avg[8] = { 0 };
		for (int i = 0; i < (sizeof(avg) / sizeof(avg[0])); i++)
		{
			for (int index = (sizeof(avg) / sizeof(avg[0])) - 1; 0 <= index; index--)
			{
				if ((ct_table[i] * avg_table[index] * 2) <= usec)
				{
					avg[i] = index;
					break;
				}
			}
		}

		// Determine Convertion Time
		int	max_index = 0;
		for (int i = 1; i < (sizeof(avg) / sizeof(avg[0])); i++)
		{
			if ((ct_table[max_index] * avg_table[avg[max_index]]) <= (ct_table[i] * avg_table[avg[i]]))
			{
				max_index = i;
			}
		}

		ct_reg = max_index;
		avg_reg = avg[max_index];

#else

		while(
			(ct_reg < (sizeof(ct_table) / sizeof(ct_table[0]) - 1)) &&
			((ct_table[ct_reg +1] * avg_table[avg_reg] * 2) <= usec))
		{
			ct_reg++;
		} 

		// Determine Averaging
		while (
			(avg_reg < (sizeof(avg_table) / sizeof(avg_table[0]) - 1)) &&
			((ct_table[ct_reg] * avg_table[avg_reg+1] * 2) <= usec))
		{
			avg_reg++;
		}

#endif

		printf( "SetSamplingDuration req = %d, ConvTime = %d, Average = %d, actual =%d\n", msec, ct_table[ct_reg] , avg_table[avg_reg], ct_table[ct_reg] * avg_table[avg_reg] * 2 / 1000 );
		
		uint8_t  w_data[3]	= {
			0x00,
			(uint8_t)(0x40 | (0xe0 & (avg_reg << 1)) | (0x01 & (ct_reg >> 2)) ), 
			(uint8_t)(0x07 | (0xc0 & (ct_reg << 6)) | (0x38 & (ct_reg << 3)) ) };
		m_i2c.write( w_data, sizeof(w_data) );
	}

	virtual	int16_t	ReadShuntRaw()
	{
		uint8_t	w_data[1]	= { 0x01 };
		uint8_t	r_data[2]	= { 0x00, 0x00 };

		// read shunt
		w_data[0]   = 0x01;
		m_i2c.write( w_data, sizeof(w_data) );
		m_i2c.read( r_data, sizeof(r_data) );

		return	(r_data[0] << 8) | r_data[1];
	}
	
	virtual	int16_t	ReadVoltageRaw()
	{
		uint8_t  w_data[1]	= { 0x01 };
		uint8_t  r_data[2]	= { 0x00, 0x00 };

		// read vbus
		w_data[0]   = 0x02;
		m_i2c.write( w_data, sizeof(w_data) );
		m_i2c.read( r_data, sizeof(r_data) );

		return	(r_data[0] << 8) | r_data[1];
	}

	virtual	double	GetShuntOf1LSB()
	{
		return	0.0000025;
	}

	virtual	double	GetVoltageOf1LSB()
	{
		return	0.00125;
	}

protected:
	ctrl_i2c	m_i2c;
};


class PMoni_INA219 : public ctrl_PowerMonitor
{
public:
	PMoni_INA219( int slave_addr = 0x45 ) : m_i2c( slave_addr )
	{
		// reg = m_dShuntReg * m_dCalibMeasured / m_dCalibExpected
		m_dShuntReg			= 0.005;
		m_dCalibMeasured	= 1.14;
		m_dCalibExpected	= 1.00;

		uint8_t  w_data[3]	= { 0x00, 0x07, 0xFF };
		if( !m_i2c.write( w_data, sizeof(w_data) ) )
		{
//			throw	std::runtime_error("PMoni_INA219 i2c write failed");
		}
	}

	virtual	void	SetSamplingPeriod( int msec )
	{
		int		avg_reg			= 0;
		int		avg_table[]		= { 1, 2, 4, 8, 17, 34, 68, 0x7FFFFFFF };
		
		while( avg_table[avg_reg] <= msec )
		{
			avg_reg++;
		}
		printf( "SetSamplingDuration req = %d, Average reg=%d, actual =%d\n", msec, avg_reg, avg_table[avg_reg] );

		uint8_t  w_data[3]	= { 0x00, 0x07, (uint8_t)(0x80 + avg_reg) };
		m_i2c.write( w_data, sizeof(w_data) );
	}

	double	GetShuntOf1LSB()
	{
		return 0.00001;
	}

	double	GetVoltageOf1LSB()
	{
		return	0.004;
	}

	virtual	int16_t	ReadShuntRaw()
	{
		uint8_t  		w_data[1]	= { 0x01 };
		uint8_t  		r_data[2]	= { 0x00, 0x00 };

		// read shunt
		w_data[0]   = 0x01;
		m_i2c.write( w_data, sizeof(w_data) );
		m_i2c.read( r_data, sizeof(r_data) );

		return	(r_data[0] << 8) | r_data[1];
	}

	virtual	int16_t	ReadVoltageRaw()
	{
		uint8_t 		w_data[1]	= { 0x02 };
		uint8_t 		r_data[2]	= { 0x00, 0x00 };

		// read vbus
		m_i2c.write( w_data, sizeof(w_data) );
		m_i2c.read( r_data, sizeof(r_data) );

		return	((r_data[0] << 8) | r_data[1]) >> 3;
	}

protected:
	ctrl_i2c	m_i2c;
};

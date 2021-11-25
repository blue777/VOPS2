
#include <string.h>
#include "display_if.h"
#include "ctrl_i2c.h"

#include "bitmap_font_render.h"

#ifndef FEATURE_NO_FRAMEBUFFER	
#define FEATURE_NO_FRAMEBUFFER	0
#endif


class Display_SSD1306_i2c : public DisplayIF
{
public:
	Display_SSD1306_i2c( int nRotate = 0, int x_offset = 0) :
		m_i2c( 0x3C )
	{
		m_nRotate	= nRotate;
		m_nXoffset	= x_offset;
		
		switch( nRotate )
		{
		case 0:		break;
		case 180:	break;

		case 90:	break;
		case 270:	break;

		default:
			printf( "ERROR: Display_SSD1306_i2c() Invalid rotate %d.\n", nRotate );
//			throw	"Display_SSD1306_i2c() INVALID rotate";
		}
	}

	virtual int Init()
	{
		printf( "Display_SSD1306_i2c::Init()\n");
		
		///////////////////////////////////////////////////////
		// 1. Fundamental Command Table
		///////////////////////////////////////////////////////

		// Set Contrast Control
//		WriteCmd(0x81);
//		WriteCmd(0xFF);

		// Normal display (RESET)
		WriteCmd(0xA6);

		///////////////////////////////////////////////////////
		// 3. Addressing Setting Command Table
		///////////////////////////////////////////////////////

		// Set Memory Addressing Mode
		WriteCmd(0x20);
		WriteCmd(0x02); //Page Addressing Mode (RESET)

		///////////////////////////////////////////////////////
		// 4. Hardware Configuration (Panel resolution & layout related) Command Table
		///////////////////////////////////////////////////////

		// Set Display Start Line (0x40 + startLine)
		WriteCmd(0x40);

		// Set Segment Re-map
		// 0xA0: column address 0 is mapped to SEG0 (RESET)
		// 0xA1: column address 127 is mapped to SEG0

		// Set COM Output Scan Direction
		// 0xC0: normal mode (RESET) Scan from COM0 to COM[N –1]
		// 0xC8: remapped mode. Scan from COM[N-1] to COM0

		if( m_nRotate < 180 )
		{
			WriteCmd(0xA0);
			WriteCmd(0xC0);
		}
		else
		{
			WriteCmd(0xA1); // #set segment remap
			WriteCmd(0xC8);
		}

		// Set MUX ratio to N+1 MUX
//		WriteCmd(0xA8);
//		WriteCmd(0x3F); // ResetValue=0x3F

		// Set Display Offset
		WriteCmd(0xD3);
		WriteCmd(0x00);

		// Set COM Pins Hardware Configuration
//		WriteCmd(0xDA);
//		WriteCmd(0x12); // ResetValue=0x12

		///////////////////////////////////////////////////////
		// 5. Timing & Driving Scheme Setting Command Table
		///////////////////////////////////////////////////////

		// Set Display Clock Divide Ratio/Oscillator Frequency
//		WriteCmd(0xD5);
//		WriteCmd(0x80); // ResetValue=0x80

		// Set Pre-charge Period
//		WriteCmd(0xD9);
//		WriteCmd(0xF1); // ResetValue=0x22

		// Set VCOMH Deselect Level
//		WriteCmd(0xDB);
//		WriteCmd(0x40); // ReserValue=0x20

		m_tDispSize.width	= 128;
		m_tDispSize.height	= 64;
		
		return	0;
	}

	virtual int DispClear()
	{
#if !FEATURE_NO_FRAMEBUFFER
		memset( m_iFrameBuf, 0, sizeof(m_iFrameBuf) );
#endif

		for( int p = 0; p < 16; p++ )
		{
			uint8_t			addr[1+3];
			uint8_t			data[1+132]	={0};

			addr[0] = 0x00;		// Command Mode
			addr[1] = 0xB0 | p;	// Set Page Address
			addr[2] = 0x10;		// #set higher column address
			addr[3] = 0x00;		// #set lower column address

			data[0] = 0x40; 	// Data Mode

			m_i2c.write( addr, sizeof(addr) );
			m_i2c.write( data, sizeof(data) );
		}

		return	0;
	}

	template<class T>
	int		DrawText( BitmapFontIF* piFont, int xstart, int ystart, const T * text)
	{
		if( text != NULL )
		{
			if ((m_nRotate == 0) || (m_nRotate == 180))
			{
				int		pages = piFont->GetHeight() / 8;

				for (int p = 0; p < pages; p++)
				{
					uint8_t			addr[1 + 3];
					uint8_t			data[1 + 132];

					int				width = 0;
					const T*		pStr = text;

					while (*pStr)
					{
						uint8_t	nFontWidth	= 0;
						uint8_t	nFontHeight = 0;
						const uint8_t*	nFontBits	= piFont->GetBits( *pStr++, nFontWidth, nFontHeight );

						if( nFontBits != NULL )
						{
							int		tw = nFontWidth;
							for (int x = 0; x < tw; x++)
							{
								if (xstart + width < m_tDispSize.width)
								{
									data[1 + width] = nFontBits[nFontWidth * p + x];
									width++;
								}
							}
						}
					}

					addr[0] = 0x00;								// Command Mode
					addr[1] = 0xB0 | (ystart / 8 + p);			// Set Page Address
					addr[2] = 0x10 | (0x0F & (xstart >> 4));	// #set higher column address
					addr[3] = 0x00 | (0x0F & xstart);			// #set lower column address

					data[0] = 0x40; 	// Data Mode

					m_i2c.write(addr, sizeof(addr));
					m_i2c.write(data, 1 + width);
				}
			}
			else
			{
				// 8dot ずつ描いていく
				const T*		pStr = text;
				uint8_t			addr[1 + 3];
				uint8_t			data[1 + 64] = { 0 };
				
				uint8_t	nFontWidth	= 0;
				uint8_t	nFontHeight = 0;
				const uint8_t*	nFontBits	= piFont->GetBits( pStr[0], nFontWidth, nFontHeight );

				int					char_offset = 0;
				int					pages = piFont->GetHeight() / 8;

				ystart += m_nXoffset;

				while( nFontBits != NULL )
				{
					int	page = m_tDispSize.width - 8 - xstart;
					if (page < 0)
						break;

					addr[0] = 0x00;								// Command Mode
					addr[1] = 0xB0 | (page / 8);			// Set Page Address
					addr[2] = 0x10 | (0x0F & (ystart >> 4));	// #set higher column address
					addr[3] = 0x00 | (0x0F & ystart);			// #set lower column address

					data[0] = 0x40; 	// Data Mode
					for (int y = 0; y < nFontHeight; y++)
					{
						data[1+y] = 0;
					}

					for (int bit_offset = 7; 0 <= bit_offset; bit_offset--)
					{
						while (nFontWidth <= char_offset)
						{
							pStr++;
							char_offset = 0;

							if (*pStr == '\0')
							{
								nFontBits = NULL;
								break;
							}

							nFontBits	= piFont->GetBits( pStr[0], nFontWidth, nFontHeight );
						}
						if (nFontBits == NULL)
							break;

						for (int y = 0; y < nFontHeight; y += 8 )
						{
							uint8_t	font = nFontBits[nFontWidth * (y/8) + char_offset];

							data[1 + y + 0] |= (1 & (font >> 0)) << bit_offset;
							data[1 + y + 1] |= (1 & (font >> 1)) << bit_offset;
							data[1 + y + 2] |= (1 & (font >> 2)) << bit_offset;
							data[1 + y + 3] |= (1 & (font >> 3)) << bit_offset;
							data[1 + y + 4] |= (1 & (font >> 4)) << bit_offset;
							data[1 + y + 5] |= (1 & (font >> 5)) << bit_offset;
							data[1 + y + 6] |= (1 & (font >> 6)) << bit_offset;
							data[1 + y + 7] |= (1 & (font >> 7)) << bit_offset;
						}
						char_offset++;
					}

					m_i2c.write(addr, sizeof(addr));
					m_i2c.write(data, 1 + nFontHeight);
					xstart += 8;
				}
			}
		}

		return	0;
	}

	virtual int DispOn()
	{
		printf( "Display_SSD1306_i2c::DispOn()\n");

		// Charge Pump Setting
		WriteCmd(0x8D);
		WriteCmd(0x14);

		// Display ON in normal mode
		WriteCmd(0xAF);

		return	0;
	}

	virtual int DispOff()
	{
		printf( "Display_SSD1306_i2c::DispOff()\n");

		WriteCmd(0xAE);		// #display off

		return	0;
	}

	virtual int Quit()
	{
		printf( "Display_SSD1306_i2c::Quit()\n");

		m_tDispSize.width	= 0;
		m_tDispSize.height	= 0;
		return	0;
	}

	virtual	int WriteImageBGRA( int x, int y, const uint8_t* image, int stride, int cx, int cy )
	{
		return	-1;
	}


	virtual	int	WriteImageGRAY( int x, int y, const uint8_t* image, int stride, int cx, int cy )
	{
#if !FEATURE_NO_FRAMEBUFFER
		if( _CalcTransArea( x, y, image, stride, 1, cx, cy ) )
		{
			const uint8_t*	src = image;
			uint8_t*		dst	= &m_iFrameBuf[ m_tDispSize.width * y + x ];

			for( int r = 0; r < cy; r++ )
			{
				memcpy( dst, src, cx );
				src	+= stride;
				dst	+= m_tDispSize.width;
			}
			
			TransferImage( x, y, cx, cy );
		}
#endif
		return	-1;
	}
	
	virtual	int GetBPP()
	{
		return	1;
	}
	
	void	SetContrast( uint8_t value )
	{
		// Set Contrast Control
		WriteCmd(0x81);
		WriteCmd(value);
	}

protected:
	bool    WriteCmd( unsigned char cmd )
	{
		unsigned char data[2];

		data[0] = 0x00; // Command Mode
		data[1] = cmd;

		return  m_i2c.write( data, 2 );
	}



#if !FEATURE_NO_FRAMEBUFFER	
	void	TransferImage( int x, int y, int cx, int cy )
	{
		int	ps	= y / 8;
		int	pe	= (y+cy-1) / 8;
		int	xs	= m_nXoffset + x;

		for( int p = ps; p <= pe; p++ )
		{
			uint8_t			addr[1+3];
			uint8_t			data[1+128];

			addr[0] = 0x00;						// Command Mode
			addr[1] = 0xB0 | p;					// Set Page Address
			addr[2] = 0x10 | (0x0F & (xs >> 4));	// #set higher column address
			addr[3] = 0x00 | (0x0F & xs);		// #set lower column address

			data[0] = 0x40; 	// Data Mode

			CreateTransferImage(
				&data[1],
				&m_iFrameBuf[ (m_tDispSize.width * p * 8) + x ],
				m_tDispSize.width,
				cx );

			m_i2c.write( addr, sizeof(addr) );
			m_i2c.write( data, 1 + cx );
		}
	}
	
	static	void	CreateTransferImage( uint8_t * dst, const uint8_t * src, int stride, int cx )
	{
		int		x	= 0;

		for( ; (x+4) <= cx; x += 4 )
		{
			const uint8_t *	s	= &src[x];

			dst[x+0] =	((s[ 0 * stride + 0 ] >> 7) << 0) |
						((s[ 1 * stride + 0 ] >> 7) << 1) |
						((s[ 2 * stride + 0 ] >> 7) << 2) |
						((s[ 3 * stride + 0 ] >> 7) << 3) |
						((s[ 4 * stride + 0 ] >> 7) << 4) |
						((s[ 5 * stride + 0 ] >> 7) << 5) |
						((s[ 6 * stride + 0 ] >> 7) << 6) |
						((s[ 7 * stride + 0 ] >> 7) << 7);

			dst[x+1] =	((s[ 0 * stride + 1 ] >> 7) << 0) |
						((s[ 1 * stride + 1 ] >> 7) << 1) |
						((s[ 2 * stride + 1 ] >> 7) << 2) |
						((s[ 3 * stride + 1 ] >> 7) << 3) |
						((s[ 4 * stride + 1 ] >> 7) << 4) |
						((s[ 5 * stride + 1 ] >> 7) << 5) |
						((s[ 6 * stride + 1 ] >> 7) << 6) |
						((s[ 7 * stride + 1 ] >> 7) << 7);

			dst[x+2] =	((s[ 0 * stride + 2 ] >> 7) << 0) |
						((s[ 1 * stride + 2 ] >> 7) << 1) |
						((s[ 2 * stride + 2 ] >> 7) << 2) |
						((s[ 3 * stride + 2 ] >> 7) << 3) |
						((s[ 4 * stride + 2 ] >> 7) << 4) |
						((s[ 5 * stride + 2 ] >> 7) << 5) |
						((s[ 6 * stride + 2 ] >> 7) << 6) |
						((s[ 7 * stride + 2 ] >> 7) << 7);

			dst[x+3] =	((s[ 0 * stride + 3 ] >> 7) << 0) |
						((s[ 1 * stride + 3 ] >> 7) << 1) |
						((s[ 2 * stride + 3 ] >> 7) << 2) |
						((s[ 3 * stride + 3 ] >> 7) << 3) |
						((s[ 4 * stride + 3 ] >> 7) << 4) |
						((s[ 5 * stride + 3 ] >> 7) << 5) |
						((s[ 6 * stride + 3 ] >> 7) << 6) |
						((s[ 7 * stride + 3 ] >> 7) << 7);
		}

		for( ; x < cx; x++ )
		{
			const uint8_t *	s	= &src[x];

			dst[x] =	((s[ 0 * stride + 0 ] >> 7) << 0) |
						((s[ 1 * stride + 0 ] >> 7) << 1) |
						((s[ 2 * stride + 0 ] >> 7) << 2) |
						((s[ 3 * stride + 0 ] >> 7) << 3) |
						((s[ 4 * stride + 0 ] >> 7) << 4) |
						((s[ 5 * stride + 0 ] >> 7) << 5) |
						((s[ 6 * stride + 0 ] >> 7) << 6) |
						((s[ 7 * stride + 0 ] >> 7) << 7);
		}		
	}
#endif

protected:
	ctrl_i2c    m_i2c;
	int16_t		m_nRotate;
	int16_t		m_nXoffset;

#if !FEATURE_NO_FRAMEBUFFER
	uint8_t		m_iFrameBuf[128*64];
#endif
};


class Display_SH1107_i2c : public Display_SSD1306_i2c
{
public:
	Display_SH1107_i2c(int rotate) :
		Display_SSD1306_i2c(rotate, 0)
	{
	}

	virtual int Init()
	{
		Display_SSD1306_i2c::Init();

		m_tDispSize.width = 128;
		m_tDispSize.height = 128;

		return	0;
	}
};

class Display_SH1107_64x128_i2c : public Display_SSD1306_i2c
{
public:
	Display_SH1107_64x128_i2c(int rotate) :
		Display_SSD1306_i2c(rotate, 0)
	{
	}

	virtual int Init()
	{
		Display_SSD1306_i2c::Init();

		m_tDispSize.width = 128;
		m_tDispSize.height = 128;
		m_nXoffset = 32;

		return	0;
	}
};

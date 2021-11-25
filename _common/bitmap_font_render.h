#pragma once

#include <stdint.h>

class BitmapFontIF
{
public:
	BitmapFontIF(){};
	virtual	int GetHeight() = 0;
	virtual	const uint8_t*	GetBits( int c, uint8_t & nFontWidth, uint8_t & nFontHeight ) = 0;

	void	CalcRect( const char * pszString, int &width, int &height)
	{
		int		pos_x = 0;
		int		pos_y = 0;

		width = 0;
		height = 0;

		for (;*pszString != '\0'; pszString++)
		{
			uint8_t	nFontWidth = 0;
			uint8_t	nFontHeight = 0;
			GetBits( *pszString, nFontWidth, nFontHeight );

			pos_x += nFontWidth;

			if (width < pos_x)
			{
				width = pos_x;
			}
			if (height < (pos_y + nFontHeight))
			{
				height = pos_y + nFontHeight;
			}

			switch (*pszString)
			{
			case '\n':
				pos_y += GetHeight();
				pos_x = 0;
				break;
			}
		}
	}

	template<class PIXEL>
	void	DrawText( PIXEL* image, int stride, int width, int height, int pos_x, int pos_y, const char *pszString, PIXEL color=-1 )
	{
		int	start_x = pos_x;

		for(;*pszString != '\0'; pszString++)
		{
			switch (*pszString)
			{
			case '\n':
				pos_x = start_x;
				pos_y += this->GetHeight();
				break;

			default:
				{
					uint8_t		nFontWidth = 0;
					uint8_t		nFontHeight = 0;
					const uint8_t*	nFontBits	= this->GetBits( *pszString, nFontWidth, nFontHeight );

					int	tx = 0;
					int	ty = 0;
					int	tw = nFontWidth;
					int	th = nFontHeight;

					if (pos_x < 0)
					{
						tw += pos_x;
						tx -= pos_x;
					}

					if (width < (pos_x + nFontWidth) )
					{
						tw -= pos_x + nFontWidth - width;
					}

					if (pos_y < 0)
					{
						th += pos_y;
						ty -= pos_y;
					}

					if (height < (pos_y + nFontHeight))
					{
						th -= pos_y + nFontHeight - height;
					}

					if ((0 < tw) && (0 < th))
					{
						for (int y = 0; y < th; y++)
						{
							PIXEL*	dst = (PIXEL*)( ((uint8_t*)image) + stride * (pos_y + ty + y) );

							for (int x = 0; x < tw; x++)
							{
								uint8_t	bit = (nFontBits[nFontWidth * ((ty+y) / 8) + tx + x] >> ((ty+y) & 7)) & 1;

								if (bit)
								{
									dst[pos_x+tx+x] = color;
								}
							}
						}
					}

					pos_x += nFontWidth;
				}
				break;
			}
		}
	}

};




typedef struct tagCHAR_INFO
{
	int	nFontWidth;
	int	nFontHeight;
	const uint8_t * data;
} tagCHAR_INFO;

typedef struct tagBITMAP_FONT
{
	int	nFontHeight;
	tagCHAR_INFO	tInfo[128];
} tagBITMAP_FONT;


class BitmapFont : public BitmapFontIF
{
public:
	BitmapFont( const tagBITMAP_FONT& tFont ) : 
		m_tFont( tFont )
	{
	}

	virtual	int GetHeight()
	{
		return	m_tFont.nFontHeight;
	}

	virtual	const uint8_t*	GetBits( int c, uint8_t & nFontWidth, uint8_t & nFontHeight )
	{
		if( 0 == (c >> 7) )
		{
			nFontWidth	= m_tFont.tInfo[c].nFontWidth;
			nFontHeight	= m_tFont.tInfo[c].nFontHeight;
			return	m_tFont.tInfo[c].data;
		}

		c	= '?';
		nFontWidth	= m_tFont.tInfo[c].nFontWidth;
		nFontHeight	= m_tFont.tInfo[c].nFontHeight;
		return	m_tFont.tInfo[c].data;
	}

protected:
	const tagBITMAP_FONT& m_tFont; 
};




#include <TimerTC3.h>
#include <Wire.h>

#include "_common/ctrl_i2c.h"
#include "_common/ctrl_pmoni.h"
#include "_common/ctrl_mcp4725.h"
#include "_common/display_ssd1306_i2c.h"

#include "_common/bitmap_font_render.h"
#include "_common/bitmap_font16.h"
#include "_common/bitmap_font32.h"
#include "_common/bitmap_font48.h"

#include "_common/bitmap_font24.h"
#include "_common/bitmap_font40.h"
#include "_common/gpio_button.h"


#define GPIO_LED_R       7
#define GPIO_LED_G       8
#define GPIO_LED_B       9
#define GPIO_ROTARY_SW   1
#define GPIO_ROTARY_A    2
#define GPIO_ROTARY_B    3
#define GPIO_ALERT       6

#define OVER_CURRENT_PROTECT     10 // [A], 0 means unlimited.


typedef enum MODE
{
  MODE_INIT  = 0,
  MODE_SUPPLY_ONLY,
  MODE_SUPPLY_ONLY_AUTO_DEC,
  MODE_SUPPLY,
  MODE_SUPPLY_AUTO_DEC,
  MODE_LOAD,
  MODE_LOAD_AUTO_INC,
} MODE;




int   g_nRotaryA = 0;
int   g_nCtrlFine = 0;
int   g_isUpdateDac = 0;
bool  g_bRotarySwIgnore  = false;
uint32_t  g_iLastButtonUp = 0;
uint32_t  g_iModeChanged = 0;

double    g_dLoadAutoIncStopVal = 0;

MODE                g_eCurrentMode;
MODE                g_eNextMode;

Display_SSD1306_i2c g_iSSD1306;
PMoni_INA226        g_iSupplyMon;
i2c_mcp4725         g_iSupplyDac;
int                 g_nSupplyDacOut = 0;
PMoni_INA226        g_iLoadMon(1, 0);
i2c_mcp4725         g_iLoadDac(1);
int                 g_nLoadDacOut = 0;


GPIO_BUTTON_FUNC_DECLARE(GPIO_ROTARY_SW);





void  UpdateLED( int value4095 )
{
  //  int color_table[] = { 0x000000, 0xFF0000, 0xFFFF00, 0x00FF00, 0x00FFFF, 0x0000FF, 0xFF00FF, 0xFFFFFF };
  int color_table[] = { 0xFF0000, 0xFFFF00, 0x00FF00, 0x00FFFF, 0x0000FF, 0xFF00FF, 0xFFFFFF };
  //int color_table[] = { 0x0000FF, 0x00FF00, 0xFF0000, 0xFFFFFF };

  int color_table_cnt = sizeof(color_table) / sizeof(color_table[0]);
  int color_index = value4095 * (color_table_cnt - 1) * 256 / 4095;
  int index = color_index >> 8;
  int ratio = color_index & 0xFF;

  if ( index == color_table_cnt - 1 )
  {
    index = color_table_cnt - 2;
    ratio = 256;
  }

  int R = (((color_table[index] >> 16) & 0xFF) * (256 - ratio) + ((color_table[index + 1] >> 16) & 0xFF) * ratio) >> 8;
  int G = (((color_table[index] >>  8) & 0xFF) * (256 - ratio) + ((color_table[index + 1] >>  8) & 0xFF) * ratio) >> 8;
  int B = (((color_table[index] >>  0) & 0xFF) * (256 - ratio) + ((color_table[index + 1] >>  0) & 0xFF) * ratio) >> 8;

  analogWrite(GPIO_LED_R, R);
  analogWrite(GPIO_LED_G, G);
  analogWrite(GPIO_LED_B, B);
}



void OnGpioButton_Down(size_t pin)
{
  g_bRotarySwIgnore = false;
}


void OnGpioButton_Up(size_t pin)
{
  if ( !g_bRotarySwIgnore )
  {
    int interval  = millis() - g_iLastButtonUp;
    g_bRotarySwIgnore = true;

    if ( interval < 1000 )
    {
      switch ( g_eCurrentMode )
      {
        case MODE_SUPPLY_ONLY:
        case MODE_SUPPLY_ONLY_AUTO_DEC:
          g_eNextMode = MODE_SUPPLY;
          break;

        case MODE_SUPPLY:
        case MODE_SUPPLY_AUTO_DEC:
          g_eNextMode = MODE_LOAD;
          break;

        case MODE_LOAD:
        case MODE_LOAD_AUTO_INC:
          g_eNextMode = MODE_SUPPLY_ONLY;
          break;
      }
    }
    else
    {
      switch ( g_eCurrentMode )
      {
        case MODE_SUPPLY_ONLY_AUTO_DEC:
          g_eNextMode = MODE_SUPPLY_ONLY;
          break;

        case MODE_LOAD_AUTO_INC:
          g_eNextMode = MODE_LOAD;
          break;

        case MODE_SUPPLY_AUTO_DEC:
          g_eNextMode = MODE_SUPPLY;
          break;

        case MODE_SUPPLY_ONLY:
        case MODE_LOAD:
        case MODE_SUPPLY:
          g_nCtrlFine = !g_nCtrlFine;
          break;
      }
    }

    g_iLastButtonUp = millis();
  }
}


void OnGpioButton_LongPress(size_t pin)
{
  if ( !g_bRotarySwIgnore )
  {
    switch ( g_eCurrentMode )
    {
      case MODE_SUPPLY_ONLY:  g_eNextMode = MODE_SUPPLY_ONLY_AUTO_DEC;  break;
      case MODE_SUPPLY:         g_eNextMode = MODE_SUPPLY_AUTO_DEC;  break;
      case MODE_LOAD:        g_eNextMode = MODE_LOAD_AUTO_INC;  break;
    }

    g_bRotarySwIgnore = true;
  }
}



void  OnRotary( int dir )
{
  if ( dir != 0 )
  {
    switch ( g_eCurrentMode )
    {
      case MODE_SUPPLY_ONLY:
      case MODE_SUPPLY:
        if ( g_nCtrlFine )
        {
          g_nSupplyDacOut += 0 < dir ? 1 : -1;
        }
        else
        {
          int R12 = 10000;
          int R13 = 15000;
          int stp = 100 * R13 / (R12 + R13); // 100mV

          g_nSupplyDacOut += 0 < dir ? stp : - stp;
        }
        break;

      case MODE_LOAD:
        if ( g_nCtrlFine )
        {
          g_nLoadDacOut += 0 < dir ? 1 : -1;
        }
        else
        {
          int stp = 50;
          g_nLoadDacOut += 0 < dir ? stp : - stp;
        }
        break;

      case MODE_SUPPLY_ONLY_AUTO_DEC:
        g_eNextMode = MODE_SUPPLY_ONLY;
        break;

      case MODE_SUPPLY_AUTO_DEC:
        g_eNextMode = MODE_SUPPLY;
        break;

      case MODE_LOAD_AUTO_INC:
        g_eNextMode = MODE_LOAD;
        break;
    }

    g_nSupplyDacOut = 0 <= g_nSupplyDacOut ? g_nSupplyDacOut < 4096 ?  g_nSupplyDacOut : 4095 : 0;
    g_nLoadDacOut = 0 <= g_nLoadDacOut ? g_nLoadDacOut < 4096 ?  g_nLoadDacOut : 4095 : 0;
    g_isUpdateDac = 1;
    UpdateLED(g_nSupplyDacOut);
  }
}

void OnRotaryA()
{
  g_nRotaryA  = digitalRead(GPIO_ROTARY_A) ? -1 : 1;
}

void OnRotaryB()
{
  OnRotary( g_nRotaryA );
  g_nRotaryA  = 0;
}

void OnAlert()
{
  g_nSupplyDacOut = 0;
  g_nLoadDacOut = 0;
  g_isUpdateDac = 1;

  switch ( g_eCurrentMode )
  {
    case MODE_SUPPLY_ONLY_AUTO_DEC:
      g_eNextMode = MODE_SUPPLY_ONLY;
      break;

    case MODE_SUPPLY_AUTO_DEC:
      g_eNextMode = MODE_SUPPLY;
      break;

    case MODE_LOAD_AUTO_INC:
      g_eNextMode = MODE_LOAD;
      break;
  }

  analogWrite(GPIO_LED_R, 0);
  analogWrite(GPIO_LED_G, 0);
  analogWrite(GPIO_LED_B, 0);
}


void setup()
{
  // GPIO
  pinMode(GPIO_LED_R, OUTPUT);
  pinMode(GPIO_LED_G, OUTPUT);
  pinMode(GPIO_LED_B, OUTPUT);

  pinMode(GPIO_ROTARY_SW, INPUT);
  pinMode(GPIO_ROTARY_A, INPUT);
  pinMode(GPIO_ROTARY_B, INPUT);

  // Enable interrupt
  GPIO_BUTTON_FUNC_START(GPIO_ROTARY_SW);

  attachInterrupt(GPIO_ROTARY_A , OnRotaryA, CHANGE);
  attachInterrupt(GPIO_ROTARY_B , OnRotaryB, FALLING);
  Wire.begin();

  ChangeMode( MODE_INIT );

  // INA226 - Over current alert
#if 0 < OVER_CURRENT_PROTECT
  double alert_ampare = OVER_CURRENT_PROTECT;
  g_iSupplyMon.SetAlertFunc( PMoni_INA226::ALERT_SHUNT_OVER_VOLT, (int)(alert_ampare / g_iSupplyMon.GetAmpereOf1LSB()) );
  pinMode(GPIO_ALERT, INPUT);
  attachInterrupt(GPIO_ALERT , OnAlert, FALLING);
#endif

  g_iLoadMon.SetAlertFunc( PMoni_INA226::ALERT_SHUNT_OVER_VOLT, (int)(1 / g_iSupplyMon.GetAmpereOf1LSB()) );

  // OLED
  g_iSSD1306.Init();
  g_iSSD1306.DispClear();
  g_iSSD1306.DispOn();
}


void ChangeMode( MODE mode)
{
  switch ( mode )
  {
    default:
    case MODE_INIT:
      g_nSupplyDacOut = 0;
      g_nLoadDacOut = 0;
      UpdateLED(g_nSupplyDacOut);
      g_iSupplyDac.SetValue( g_nSupplyDacOut << 4 );
      g_iLoadDac.SetValue( g_nLoadDacOut << 4 );
    /* Don't break */

    case MODE_SUPPLY_ONLY:
      g_iSupplyMon.SetSamplingPeriod( 100 );
      g_iLoadMon.SetSamplingPeriod(100);
      g_eCurrentMode = MODE_SUPPLY_ONLY;
      Serial.println( "***** POWER SUPPLY ONLY MODE *****" );
      break;

    case MODE_SUPPLY_ONLY_AUTO_DEC:
      g_iLoadMon.SetSamplingPeriod(33);
      g_iSupplyMon.SetSamplingPeriod(33);
      g_eCurrentMode = MODE_SUPPLY_ONLY_AUTO_DEC;
      Serial.println( "***** POWER SUPPLY ONLY, ADTO DECREMENT MODE *****" );
      break;

    case MODE_SUPPLY:
      g_iSupplyMon.SetSamplingPeriod( 100 );
      g_iLoadMon.SetSamplingPeriod(100);
      g_eCurrentMode = MODE_SUPPLY;
      Serial.println( "***** POWER SUPPLY MODE *****" );
      break;

    case MODE_SUPPLY_AUTO_DEC:
      g_iLoadMon.SetSamplingPeriod(33);
      g_iSupplyMon.SetSamplingPeriod(33);
      g_eCurrentMode = MODE_SUPPLY_AUTO_DEC;
      Serial.println( "***** POWER SUPPLY, ADTO DECREMENT MODE *****" );
      break;

    case MODE_LOAD_AUTO_INC:
      g_nLoadDacOut = 0;
      g_iLoadDac.SetValue(0 );

      g_iLoadMon.SetSamplingPeriod(500);
      delay(1000);
      g_dLoadAutoIncStopVal = g_iLoadMon.GetV() * 0.8;
      if ( 0.5 <= g_dLoadAutoIncStopVal )
      {
        g_iLoadMon.SetSamplingPeriod(33);
        g_iSupplyMon.SetSamplingPeriod(33);
        g_eCurrentMode = MODE_LOAD_AUTO_INC;
        Serial.println( "***** LAOD MODE, AUTO INCREMENT *****" );
        break;
      }
    /* Don't break */

    case MODE_LOAD:
      g_iLoadMon.SetSamplingPeriod(100);
      g_iSupplyMon.SetSamplingPeriod(100);
      g_eCurrentMode = MODE_LOAD;
      Serial.println( "***** LAOD MODE *****" );
      break;
  }

  Serial.println( "Timepoint,Supply-DAC,  Supply-A,  Supply-V,  Load-DAC,    Load-A,    Load-V" );
  g_eNextMode = g_eCurrentMode;
  g_nCtrlFine = false;
  g_iModeChanged  = millis();
}

void loop()
{
  double  supV = g_iSupplyMon.GetV();
  double  supA = g_iSupplyMon.GetA();
  double  loadV = g_iLoadMon.GetV();
  double  loadA = g_iLoadMon.GetA();

  // Console
  {
    char    szBuf[64];
    char    szSupV[16];
    char    szSupA[16];
    char    szLoadV[16];
    char    szLoadA[16];
    dtostrf( supV, 9, 6, szSupV );
    dtostrf( supA, 9, 6, szSupA );
    dtostrf( loadV, 9, 6, szLoadV );
    dtostrf( loadA, 9, 6, szLoadA );
    sprintf( szBuf, "%9d, %9d, %s, %s, %9d, %s, %s", millis() - g_iModeChanged, g_iSupplyDac.GetValue() >> 4, szSupA, szSupV, g_iLoadDac.GetValue() >> 4, szLoadA, szLoadV  );
    Serial.println( szBuf );
  }

  // Setup for next sample
  if ( g_isUpdateDac )
  {
    g_isUpdateDac = 0;
    g_iSupplyDac.SetValue( g_nSupplyDacOut << 4 );
    g_iLoadDac.SetValue( g_nLoadDacOut << 4 );
  }

  switch ( g_eCurrentMode )
  {
    case MODE_SUPPLY_ONLY_AUTO_DEC:
    case MODE_SUPPLY_AUTO_DEC:
      if ( 0 < g_nSupplyDacOut )
      {
        g_nSupplyDacOut--;
        g_iSupplyDac.SetValue( g_nSupplyDacOut << 4 );
        UpdateLED( g_nSupplyDacOut );
      }
      else
      {
        switch ( g_eCurrentMode )
        {
          case MODE_SUPPLY_ONLY_AUTO_DEC:
            g_eNextMode = MODE_SUPPLY_ONLY;
            break;
          case MODE_SUPPLY_AUTO_DEC:
            g_eNextMode = MODE_SUPPLY;
            break;
        }
      }
      break;

    case MODE_LOAD_AUTO_INC:
      if ( (loadV < g_dLoadAutoIncStopVal) ||
           ( 4 < loadA) )
      {
        g_nLoadDacOut = 0;
        g_iLoadDac.SetValue( 0 );
        g_eNextMode = MODE_LOAD;
      }
      else
      {
        g_nLoadDacOut++;
        g_iLoadDac.SetValue( g_nLoadDacOut << 4 );
      }
      break;
  }

  if ( g_eNextMode != g_eCurrentMode )
  {
    ChangeMode( g_eNextMode );
  }

  // OLED
  {
    uint8_t image[128 * 64];
    char    szBuf[64], szVal[16];
    int     w, h;

    BitmapFont  iFont16( g_tBitmapFont16 );
    BitmapFont  iFont24( g_tBitmapFont24 );
    BitmapFont  iFont48( g_tBitmapFont48 );

    memset( image, 0, sizeof(image) );

    if ( (g_eCurrentMode == MODE_SUPPLY_ONLY) ||
         (g_eCurrentMode == MODE_SUPPLY_ONLY_AUTO_DEC) )
    {
      // Draw Voltage
      {
        int left = 128;
        sprintf( szBuf, "A" );

        iFont16.CalcRect( szBuf, w, h );
        left  -= w;
        iFont16.DrawText( image, 128, 128, 64, 128 - w, 18, "v" );
        left  -= 2;

        dtostrf( supV, 0, 3, szBuf );
        iFont48.CalcRect( szBuf, w, h );
        left  -= w;
        iFont48.DrawText( image, 128, 128, 64, 0, 0, szBuf );
      }

      // Draw Ampare
      {
        int left = 128;
        sprintf( szBuf, "A" );


        iFont16.CalcRect( szBuf, w, h );
        left -= w;
        iFont16.DrawText( image, 128, 128, 64, left, 46, szBuf );
        left -= 2;

        dtostrf( supA, 0, 3, szBuf );
        iFont24.CalcRect( szBuf, w, h );
        left -= w;
        iFont24.DrawText( image, 128, 128, 64, left, 40, szBuf );
      }

      if ( g_eCurrentMode == MODE_SUPPLY_ONLY_AUTO_DEC )
      {
        iFont16.DrawText( image, 128, 128, 64, 0, 48, "A-DEC" );
      }
      else if ( g_nCtrlFine )
      {
        iFont16.DrawText( image, 128, 128, 64, 0, 48, "FINE" );
      }
    }
    else
    {
      const int left = 128;

      dtostrf( supV, 0, 3, szVal );
      sprintf( szBuf, "%s V", szVal );
      iFont16.CalcRect( szBuf, w, h );
      iFont16.DrawText( image, 128, 128, 64, left / 2 - w, 16, szBuf );

      dtostrf( supA, 0, 3, szVal );
      sprintf( szBuf, "%s A", szVal );
      iFont16.CalcRect( szBuf, w, h );
      iFont16.DrawText( image, 128, 128, 64, left - w, 16, szBuf );

      dtostrf( loadV, 0, 3, szVal );
      sprintf( szBuf, "%s V", szVal );
      iFont16.CalcRect( szBuf, w, h );
      iFont16.DrawText( image, 128, 128, 64, left / 2 - w, 48, szBuf );

      dtostrf( loadA, 0, 3, szVal );
      sprintf( szBuf, "%s A", szVal );
      iFont16.CalcRect( szBuf, w, h );
      iFont16.DrawText( image, 128, 128, 64, left - w, 48, szBuf );

      switch ( g_eCurrentMode )
      {
        case MODE_SUPPLY:
        case MODE_SUPPLY_AUTO_DEC:
          iFont16.DrawText( image, 128, 128, 64, 0, 0,  "** Supply **" );
          iFont16.DrawText( image, 128, 128, 64, 0, 32, "Load" );
          break;

        case MODE_LOAD:
        case MODE_LOAD_AUTO_INC:
          iFont16.DrawText( image, 128, 128, 64, 0, 0,  "Supply" );
          iFont16.DrawText( image, 128, 128, 64, 0, 32, "** Load **" );
          break;
      }

      switch ( g_eCurrentMode )
      {
        case MODE_SUPPLY:
          if ( g_nCtrlFine )
          {
            iFont16.DrawText( image, 128, 128, 64, 90, 0, "FINE" );
          }
          break;

        case MODE_SUPPLY_AUTO_DEC:
          iFont16.DrawText( image, 128, 128, 64, 84, 0, "A-DEC" );
          break;

        case MODE_LOAD:
          if ( g_nCtrlFine )
          {
            iFont16.DrawText( image, 128, 128, 64, 90, 32, "FINE" );
          }
          break;

        case MODE_LOAD_AUTO_INC:
          iFont16.DrawText( image, 128, 128, 64, 84, 32, "A-INC" );
          break;
      }
    }

    g_iSSD1306.WriteImageGRAY( 0, 0, image, 128, 128, 64 );
  }

  switch ( g_eCurrentMode )
  {
    case MODE_SUPPLY_ONLY:
    case MODE_SUPPLY:
    case MODE_LOAD:
      delay(100);
      break;
  }
}

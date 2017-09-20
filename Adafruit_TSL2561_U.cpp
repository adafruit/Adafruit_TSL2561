/**************************************************************************/
/*!
    @file     Adafruit_TSL2561.cpp
    @author   K.Townsend (Adafruit Industries), T.Jacobs (CegekaLabs)
    @license  BSD (see license.txt)

    Driver for the TSL2561 digital luminosity (light) sensors.

    Pick one up at http://www.adafruit.com/products/439

    Adafruit invests time and resources providing this open source code,
    please support Adafruit and open-source hardware by purchasing
    products from Adafruit!

    @section  HISTORY

    v3.0 - Added interrupt support
    v2.0 - Rewrote driver for Adafruit_Sensor and Auto-Gain support, and
           added lux clipping check (returns 0 lux on sensor saturation)
    v1.0 - First release (previously TSL2561)
*/
/**************************************************************************/
#if defined(__AVR__)
#include <avr/pgmspace.h>
#include <util/delay.h>
#elif !defined(TEENSYDUINO)
#include "pgmspace.h"
#endif
#include <stdlib.h>

#include "Adafruit_TSL2561_U.h"

#define TSL2561_DELAY_INTTIME_13MS    (15)
#define TSL2561_DELAY_INTTIME_101MS   (120)
#define TSL2561_DELAY_INTTIME_402MS   (450)

/*========================================================================*/
/*                          PRIVATE FUNCTIONS                             */
/*========================================================================*/

/**************************************************************************/
/*!
    @brief  Writes a register and an 8 bit value over I2C
*/
/**************************************************************************/
void Adafruit_TSL2561_Unified::write8 (uint8_t reg, uint32_t value)
{
  DEBUGOUT.print(" *** TSL2561: write8, register = 0x"); DEBUGOUT.print(reg, HEX); DEBUGOUT.print(" / data: 0x"); DEBUGOUT.println(value, HEX);
  wire -> beginTransmission(_addr);
  #if ARDUINO >= 100
  wire -> write(reg);
  wire -> write(value & 0xFF);
  #else
  wire -> send(reg);
  wire -> send(value & 0xFF);
  #endif
  wire -> endTransmission();
}

/**************************************************************************/
/*!
    @brief  Writes a register over I2C
*/
/**************************************************************************/
void Adafruit_TSL2561_Unified::writereg8 (uint8_t reg)
{
  DEBUGOUT.print(" *** TSL2561: writereg8, register = 0x"); DEBUGOUT.println(reg, HEX);
  wire -> beginTransmission(_addr);
  #if ARDUINO >= 100
  wire -> write(reg);
  #else
  wire -> send(reg);
  #endif
  wire -> endTransmission();
}

/**************************************************************************/
/*!
    @brief  Writes a register and an 16 bit value over I2C
*/
/**************************************************************************/
void Adafruit_TSL2561_Unified::write16 (uint8_t reg, uint32_t value)
{
  // Only accept at highest TSL2561_REGISTER_CHAN1_LOW = 0x0E as input
  if((reg & 0x0F) < 0x0F) {
    write8(reg, lowByte(value));
    write8(reg+1, highByte(value));
  }
}

/**************************************************************************/
/*!
    @brief  Reads an 8 bit value over I2C
*/
/**************************************************************************/
uint8_t Adafruit_TSL2561_Unified::read8(uint8_t reg)
{
  wire -> beginTransmission(_addr);
  #if ARDUINO >= 100
  wire -> write(reg);
  #else
  wire -> send(reg);
  #endif
  wire -> endTransmission();

  wire -> requestFrom(_addr, 1);
  #if ARDUINO >= 100
  return wire -> read();
  #else
  return wire -> receive();
  #endif
}

/**************************************************************************/
/*!
    @brief  Reads a 16 bit values over I2C
*/
/**************************************************************************/
uint16_t Adafruit_TSL2561_Unified::read16(uint8_t reg)
{
  uint16_t x; uint16_t t;

  wire -> beginTransmission(_addr);
  #if ARDUINO >= 100
  wire -> write(reg);
  #else
  wire -> send(reg);
  #endif
  wire -> endTransmission();

  wire -> requestFrom(_addr, 2);
  #if ARDUINO >= 100
  t = wire -> read();
  x = wire -> read();
  #else
  t = wire -> receive();
  x = wire -> receive();
  #endif
  x <<= 8;
  x |= t;
  return x;
}

/**************************************************************************/
/*!
    Enables the device
*/
/**************************************************************************/
void Adafruit_TSL2561_Unified::enable(void)
{
  /* Enable the device by setting the control bit to 0x03 */
  write8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_CONTROL, TSL2561_CONTROL_POWERON);
}

/**************************************************************************/
/*!
    Disables the device (putting it in lower power sleep mode)
*/
/**************************************************************************/
void Adafruit_TSL2561_Unified::disable(void)
{
  /* Turn the device off to save power */
  write8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_CONTROL, TSL2561_CONTROL_POWEROFF);
}

/**************************************************************************/
/*!
    Private function to read luminosity on both channels
*/
/**************************************************************************/
void Adafruit_TSL2561_Unified::getData (uint16_t *broadband, uint16_t *ir)
{
  /* Enable the device by setting the control bit to 0x03 */
  enable();

  /* Wait x ms for ADC to complete */
  switch (_tsl2561IntegrationTime)
  {
    case TSL2561_INTEGRATIONTIME_13MS:
      delay(TSL2561_DELAY_INTTIME_13MS);  // KTOWN: Was 14ms
      break;
    case TSL2561_INTEGRATIONTIME_101MS:
      delay(TSL2561_DELAY_INTTIME_101MS); // KTOWN: Was 102ms
      break;
    default:
      delay(TSL2561_DELAY_INTTIME_402MS); // KTOWN: Was 403ms
      break;
  }

  /* Reads a two byte value from channel 0 (visible + infrared) */
  *broadband = read16(TSL2561_COMMAND_BIT | TSL2561_WORD_BIT | TSL2561_REGISTER_CHAN0_LOW);

  /* Reads a two byte value from channel 1 (infrared) */
  *ir = read16(TSL2561_COMMAND_BIT | TSL2561_WORD_BIT | TSL2561_REGISTER_CHAN1_LOW);

  // OUTPUT RAW MEASURED VALUES
  DEBUGOUT.print(" *** TSL: Raw measurement data: chan0 = 0x"); DEBUGOUT.print(*broadband,HEX); DEBUGOUT.print(" / chan1 = 0x"); DEBUGOUT.println(*ir, HEX);

  /* Turn the device off to save power */
  if(_allowSleep) disable();
}

/*========================================================================*/
/*                            CONSTRUCTORS                                */
/*========================================================================*/

/**************************************************************************/
/*!
    Constructor

    allowSleep      When true, puts the device to sleep after every operation,
                    to conserve power. Do not put the device to sleep if
                    you are using interrupts, i.e. supply false here.
*/
/**************************************************************************/
Adafruit_TSL2561_Unified::Adafruit_TSL2561_Unified(uint8_t addr, int32_t sensorID, bool allowSleep)
{
  _addr = addr;
  _tsl2561Initialised = false;
  _tsl2561AutoGain = false;
  _tsl2561IntegrationTime = TSL2561_INTEGRATIONTIME_13MS;
  _tsl2561Gain = TSL2561_GAIN_1X;
  _tsl2561SensorID = sensorID;
  _allowSleep = allowSleep;
}

/*========================================================================*/
/*                           PUBLIC FUNCTIONS                             */
/*========================================================================*/

/**************************************************************************/
/*!
    Initializes I2C and configures the sensor (call this function before
    doing anything else)
*/
/**************************************************************************/
boolean Adafruit_TSL2561_Unified::begin()
{
  wire = &Wire;
  wire -> begin();
  return init();
}

boolean Adafruit_TSL2561_Unified::begin(TwoWire *theWire)
{
  wire = theWire;
  wire -> begin();
  return init();
}

boolean Adafruit_TSL2561_Unified::init()
{
  /* Make sure we're actually connected */
  uint8_t x = read8(TSL2561_REGISTER_ID);
  if (!(x & 0x0A))
  {
    return false;
  }
  _tsl2561Initialised = true;

  /* Set default integration time and gain */
  setIntegrationTime(_tsl2561IntegrationTime);
  setGain(_tsl2561Gain);

  /* Note: by default, the device is in power down mode on bootup */
  if(_allowSleep) disable();

  return true;
}

/**************************************************************************/
/*!
    @brief  Enables or disables the auto-gain settings when reading
            data from the sensor
*/
/**************************************************************************/
void Adafruit_TSL2561_Unified::enableAutoRange(bool enable)
{
   _tsl2561AutoGain = enable ? true : false;
}

/**************************************************************************/
/*!
    Sets the integration time for the TSL2561
*/
/**************************************************************************/
void Adafruit_TSL2561_Unified::setIntegrationTime(tsl2561IntegrationTime_t time)
{
  if (!_tsl2561Initialised) begin();

  /* Enable the device by setting the control bit to 0x03 */
  enable();

  /* Update the timing register */
  write8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_TIMING, time | _tsl2561Gain);

  /* Update value placeholders */
  _tsl2561IntegrationTime = time;

  /* Turn the device off to save power */
  if(_allowSleep) disable();
}

/**************************************************************************/
/*!
    Adjusts the gain on the TSL2561 (adjusts the sensitivity to light)
*/
/**************************************************************************/
void Adafruit_TSL2561_Unified::setGain(tsl2561Gain_t gain)
{
  if (!_tsl2561Initialised) begin();

  /* Enable the device by setting the control bit to 0x03 */
  enable();

  /* Update the timing register */
  write8(TSL2561_COMMAND_BIT | TSL2561_REGISTER_TIMING, _tsl2561IntegrationTime | gain);

  /* Update value placeholders */
  _tsl2561Gain = gain;

  /* Turn the device off to save power */
  if(_allowSleep) disable();
}

/**************************************************************************/
/*!
    @brief  Gets the broadband (mixed lighting) and IR only values from
            the TSL2561, adjusting gain if auto-gain is enabled
*/
/**************************************************************************/
void Adafruit_TSL2561_Unified::getLuminosity (uint16_t *broadband, uint16_t *ir)
{
  bool valid = false;

  if (!_tsl2561Initialised) begin();

  /* If Auto gain disabled get a single reading and continue */
  if(!_tsl2561AutoGain)
  {
    getData (broadband, ir);
    return;
  }

  /* Read data until we find a valid range */
  bool _agcCheck = false;
  do
  {
    uint16_t _b, _ir;
    uint16_t _hi, _lo;
    tsl2561IntegrationTime_t _it = _tsl2561IntegrationTime;

    /* Get the hi/low threshold for the current integration time */
    switch(_it)
    {
      case TSL2561_INTEGRATIONTIME_13MS:
        _hi = TSL2561_AGC_THI_13MS;
        _lo = TSL2561_AGC_TLO_13MS;
        break;
      case TSL2561_INTEGRATIONTIME_101MS:
        _hi = TSL2561_AGC_THI_101MS;
        _lo = TSL2561_AGC_TLO_101MS;
        break;
      default:
        _hi = TSL2561_AGC_THI_402MS;
        _lo = TSL2561_AGC_TLO_402MS;
        break;
    }

    getData(&_b, &_ir);

    /* Run an auto-gain check if we haven't already done so ... */
    if (!_agcCheck)
    {
      if ((_b < _lo) && (_tsl2561Gain == TSL2561_GAIN_1X))
      {
        /* Increase the gain and try again */
        setGain(TSL2561_GAIN_16X);
        /* Drop the previous conversion results */
        getData(&_b, &_ir);
        /* Set a flag to indicate we've adjusted the gain */
        _agcCheck = true;
      }
      else if ((_b > _hi) && (_tsl2561Gain == TSL2561_GAIN_16X))
      {
        /* Drop gain to 1x and try again */
        setGain(TSL2561_GAIN_1X);
        /* Drop the previous conversion results */
        getData(&_b, &_ir);
        /* Set a flag to indicate we've adjusted the gain */
        _agcCheck = true;
      }
      else
      {
        /* Nothing to look at here, keep moving ....
           Reading is either valid, or we're already at the chips limits */
        *broadband = _b;
        *ir = _ir;
        valid = true;
      }
    }
    else
    {
      /* If we've already adjusted the gain once, just return the new results.
         This avoids endless loops where a value is at one extreme pre-gain,
         and the the other extreme post-gain */
      *broadband = _b;
      *ir = _ir;
      valid = true;
    }
  } while (!valid);
}

/**************************************************************************/
/*!
    Converts the raw sensor values to the standard SI lux equivalent.
    Returns 0 if the sensor is saturated and the values are unreliable.
*/
/**************************************************************************/
uint32_t Adafruit_TSL2561_Unified::calculateLux(uint16_t broadband, uint16_t ir)
{
  unsigned long chScale;
  unsigned long channel1;
  unsigned long channel0;

  /* Make sure the sensor isn't saturated! */
  uint16_t clipThreshold;
  switch (_tsl2561IntegrationTime)
  {
    case TSL2561_INTEGRATIONTIME_13MS:
      clipThreshold = TSL2561_CLIPPING_13MS;
      break;
    case TSL2561_INTEGRATIONTIME_101MS:
      clipThreshold = TSL2561_CLIPPING_101MS;
      break;
    default:
      clipThreshold = TSL2561_CLIPPING_402MS;
      break;
  }

  /* Return 65536 lux if the sensor is saturated */
  if ((broadband > clipThreshold) || (ir > clipThreshold))
  {
    return 65536;
  }

  /* Get the correct scale depending on the intergration time */
  switch (_tsl2561IntegrationTime)
  {
    case TSL2561_INTEGRATIONTIME_13MS:
      chScale = TSL2561_LUX_CHSCALE_TINT0;
      break;
    case TSL2561_INTEGRATIONTIME_101MS:
      chScale = TSL2561_LUX_CHSCALE_TINT1;
      break;
    default: /* No scaling ... integration time = 402ms */
      chScale = (1 << TSL2561_LUX_CHSCALE);
      break;
  }

  /* Scale for gain (1x or 16x) */
  if (!_tsl2561Gain) chScale = chScale << 4;

  /* Scale the channel values */
  channel0 = (broadband * chScale) >> TSL2561_LUX_CHSCALE;
  channel1 = (ir * chScale) >> TSL2561_LUX_CHSCALE;

  /* Find the ratio of the channel values (Channel1/Channel0) */
  unsigned long ratio1 = 0;
  if (channel0 != 0) ratio1 = (channel1 << (TSL2561_LUX_RATIOSCALE+1)) / channel0;

  /* round the ratio value */
  unsigned long ratio = (ratio1 + 1) >> 1;

  unsigned int b, m;

#ifdef TSL2561_PACKAGE_CS
  if ((ratio >= 0) && (ratio <= TSL2561_LUX_K1C))
    {b=TSL2561_LUX_B1C; m=TSL2561_LUX_M1C;}
  else if (ratio <= TSL2561_LUX_K2C)
    {b=TSL2561_LUX_B2C; m=TSL2561_LUX_M2C;}
  else if (ratio <= TSL2561_LUX_K3C)
    {b=TSL2561_LUX_B3C; m=TSL2561_LUX_M3C;}
  else if (ratio <= TSL2561_LUX_K4C)
    {b=TSL2561_LUX_B4C; m=TSL2561_LUX_M4C;}
  else if (ratio <= TSL2561_LUX_K5C)
    {b=TSL2561_LUX_B5C; m=TSL2561_LUX_M5C;}
  else if (ratio <= TSL2561_LUX_K6C)
    {b=TSL2561_LUX_B6C; m=TSL2561_LUX_M6C;}
  else if (ratio <= TSL2561_LUX_K7C)
    {b=TSL2561_LUX_B7C; m=TSL2561_LUX_M7C;}
  else if (ratio > TSL2561_LUX_K8C)
    {b=TSL2561_LUX_B8C; m=TSL2561_LUX_M8C;}
#else
  if ((ratio >= 0) && (ratio <= TSL2561_LUX_K1T))
    {b=TSL2561_LUX_B1T; m=TSL2561_LUX_M1T;}
  else if (ratio <= TSL2561_LUX_K2T)
    {b=TSL2561_LUX_B2T; m=TSL2561_LUX_M2T;}
  else if (ratio <= TSL2561_LUX_K3T)
    {b=TSL2561_LUX_B3T; m=TSL2561_LUX_M3T;}
  else if (ratio <= TSL2561_LUX_K4T)
    {b=TSL2561_LUX_B4T; m=TSL2561_LUX_M4T;}
  else if (ratio <= TSL2561_LUX_K5T)
    {b=TSL2561_LUX_B5T; m=TSL2561_LUX_M5T;}
  else if (ratio <= TSL2561_LUX_K6T)
    {b=TSL2561_LUX_B6T; m=TSL2561_LUX_M6T;}
  else if (ratio <= TSL2561_LUX_K7T)
    {b=TSL2561_LUX_B7T; m=TSL2561_LUX_M7T;}
  else if (ratio > TSL2561_LUX_K8T)
    {b=TSL2561_LUX_B8T; m=TSL2561_LUX_M8T;}
#endif

  unsigned long temp;
  temp = ((channel0 * b) - (channel1 * m));

  /* Do not allow negative lux value */
  if (temp < 0) temp = 0;

  /* Round lsb (2^(LUX_SCALE-1)) */
  temp += (1 << (TSL2561_LUX_LUXSCALE-1));

  /* Strip off fractional portion */
  uint32_t lux = temp >> TSL2561_LUX_LUXSCALE;

  /* Signal I2C had no errors */
  return lux;
}

/**************************************************************************/
/*!
    @brief  Gets the most recent sensor event
    returns true if sensor reading is between 0 and 65535 lux
    returns false if sensor is saturated
*/
/**************************************************************************/
bool Adafruit_TSL2561_Unified::getEvent(sensors_event_t *event)
{
  uint16_t broadband, ir;

  /* Clear the event */
  memset(event, 0, sizeof(sensors_event_t));

  event->version   = sizeof(sensors_event_t);
  event->sensor_id = _tsl2561SensorID;
  event->type      = SENSOR_TYPE_LIGHT;
  event->timestamp = millis();

  /* Calculate the actual lux value */
  getLuminosity(&broadband, &ir);
  event->light = calculateLux(broadband, ir);

  if (event->light == 65536) {
    return false;
  }
  return true;
}

/**************************************************************************/
/*!
    @brief  Gets the sensor_t data
*/
/**************************************************************************/
void Adafruit_TSL2561_Unified::getSensor(sensor_t *sensor)
{
  /* Clear the sensor_t object */
  memset(sensor, 0, sizeof(sensor_t));

  /* Insert the sensor name in the fixed length char array */
  strncpy (sensor->name, "TSL2561", sizeof(sensor->name) - 1);
  sensor->name[sizeof(sensor->name)- 1] = 0;
  sensor->version     = 1;
  sensor->sensor_id   = _tsl2561SensorID;
  sensor->type        = SENSOR_TYPE_LIGHT;
  sensor->min_delay   = 0;
  sensor->max_value   = 17000.0;  /* Based on trial and error ... confirm! */
  sensor->min_value   = 0.0;
  sensor->resolution  = 1.0;
}


/**************************************************************************/
/*!
    @brief  Sets up interrupt control on the TSL2561

    Values for control & persist:
      TSL2561_INTERRUPTCTL_DISABLE: interrupt output disabled
      TSL2561_INTERRUPTCTL_LEVEL: use level interrupt, see setInterruptThreshold(),
          datasheet calls this "traditional interrupt".
      TSL2561_INTERRUPTCTL_SMBALERT: if you need this, you'll know what this means :).
      TSL2561_INTERRUPTCTL_TEST: Sets interrupt and functions like SMBALERT mode.

      intpersist = 0, every integration cycle generates an interrupt
      intpersist = 1, any value outside of threshold generates an interrupt
      intpersist = 2 to 15, value must be outside of threshold for 2 to 15 integration cycles

      Note: A persist value of 0 causes an interrupt to occur after every integration cycle regardless
      of the threshold settings. A value of 1 results in an interrupt after one integration
      time period outside the threshold window. A value of N (where N is 2 through 15) results
      in an interrupt only if the value remains outside the threshold window for N consecutive
      integration cycles. For example, if N is equal to 10 and the integration time is 402 ms,
      then the total time is approximately 4 seconds.

*/
/**************************************************************************/

void Adafruit_TSL2561_Unified::setInterruptControl(tsl2561InterruptControl_t intcontrol, uint8_t intpersist) {
  // Are we initialized?
  if (!_tsl2561Initialised) begin();
  /* Enable the device by setting the control bit to 0x03 */
  enable();

  uint8_t cmd = TSL2561_COMMAND_BIT | TSL2561_REGISTER_INTERRUPT;
  uint8_t data = ((intcontrol & 0B00000011) << 4) | (intpersist & 0B00001111);

  DEBUGOUT.print(" *** TSL2561: setInterruptControl, register = 0x"); DEBUGOUT.print(cmd, HEX); DEBUGOUT.print(" / data: 0x"); DEBUGOUT.println(data, HEX);

  /* Update the interrupt control register */
  write8(cmd, data);

  /* Turn the device off to save power */
  // NOTE: This disables interrupts so no good idea if you need them :)
  if(_allowSleep) disable();
}





//
// low, high: 16-bit threshold values
// Returns true (1) if successful, false (0) if there was an I2C error
// (Also see getError() below)

/**************************************************************************/
/*!
    @brief  Set interrupt thresholds (TSL2561 supports only interrupts
    generated by thresholds on channel 0)
*/
/**************************************************************************/

void Adafruit_TSL2561_Unified::setInterruptThreshold(uint16_t lowThreshold, uint16_t highThreshold) {
	// Write low and high threshold values
	write16(TSL2561_COMMAND_BIT | TSL2561_REGISTER_THRESHHOLDL_LOW , lowThreshold);
  write16(TSL2561_COMMAND_BIT | TSL2561_REGISTER_THRESHHOLDH_LOW , highThreshold);
}

/**************************************************************************/
/*!
    @brief  Clears an active interrupt
*/
/**************************************************************************/

void Adafruit_TSL2561_Unified::clearLevelInterrupt(void) {
	// Send command byte for interrupt clear
  writereg8(TSL2561_COMMAND_BIT | TSL2561_CLEAR_BIT);
}

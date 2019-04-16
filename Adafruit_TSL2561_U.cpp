/*!
 * @file Adafruit_TSL2561_U.cpp
 * @author   K.Townsend (Adafruit Industries), T.Jacobs (CegekaLabs)
 *
 * @mainpage Adafruit TSL2561 Light/Lux sensor driver
 *
 * @section intro_sec Introduction
 *
 * This is the documentation for Adafruit's TSL2561 driver for the
 * Arduino platform.  It is designed specifically to work with the
 * Adafruit TSL2561 breakout: http://www.adafruit.com/products/439
 *
 * These sensors use I2C to communicate, 2 pins (SCL+SDA) are required
 * to interface with the breakout.
 *
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing
 * products from Adafruit!
 *
 * @section dependencies Dependencies
 *
 * This library depends on <a href="https://github.com/adafruit/Adafruit_Sensor">
 * Adafruit_Sensor</a> being present on your system. Please make sure you have
 * installed the latest version before using this library.
 *
 * @section author Author
 *
 * Written by Kevin "KTOWN" Townsend for Adafruit Industries.
 * Edited by Tim "CegekaLabs" Jacobs
 *
 * @section license License
 *
 * BSD license, all text here must be included in any redistribution.
 *
 *   @section  HISTORY
 *
 *   v3.0 - Added interrupt support
 *
 *   v2.0 - Rewrote driver for Adafruit_Sensor and Auto-Gain support, and
 *          added lux clipping check (returns 0 lux on sensor saturation)
 *   v1.0 - First release (previously TSL2561)*/
/**************************************************************************/

#include "Adafruit_TSL2561_U.h"

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
  _i2c-> beginTransmission(_addr);
  #if ARDUINO >= 100
  _i2c-> write(reg);
  _i2c-> write(value & 0xFF);
  #else
  _i2c-> send(reg);
  _i2c-> send(value & 0xFF);
  #endif
  _i2c-> endTransmission();
}

/**************************************************************************/
/*!
    @brief  Writes a register over I2C
*/
/**************************************************************************/
void Adafruit_TSL2561_Unified::writereg8 (uint8_t reg)
{
  _i2c-> beginTransmission(_addr);
  #if ARDUINO >= 100
  _i2c-> write(reg);
  #else
  _i2c-> send(reg);
  #endif
  _i2c-> endTransmission();
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
  _i2c-> beginTransmission(_addr);
  #if ARDUINO >= 100
  _i2c-> write(reg);
  #else
  _i2c-> send(reg);
  #endif
  _i2c-> endTransmission();

  _i2c-> requestFrom(_addr, 1);
  #if ARDUINO >= 100
  return _i2c-> read();
  #else
  return _i2c-> receive();
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

  _i2c-> beginTransmission(_addr);
  #if ARDUINO >= 100
  _i2c-> write(reg);
  #else
  _i2c-> send(reg);
  #endif
  _i2c-> endTransmission();

  _i2c-> requestFrom(_addr, 2);
  #if ARDUINO >= 100
  t = _i2c-> read();
  x = _i2c-> read();
  #else
  t = _i2c-> receive();
  x = _i2c-> receive();
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

  /* Turn the device off to save power */
  if(_allowSleep) disable();
}

/*========================================================================*/
/*                            CONSTRUCTORS                                */
/*========================================================================*/

/**************************************************************************/
/*!
    @brief Constructor
    @param addr The   I2C address this chip can be found on, 0x29, 0x39 or 0x49
    @param sensorID   An optional ID that will be placed in sensor events to help
                      keep track if you have many sensors in use
    @param allowSleep When true, puts the device to sleep after every operation,
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
    @brief Initializes I2C and configures the sensor with default Wire I2C
           (call this function before doing anything else)
    @returns True if sensor is found and initialized, false otherwise.
*/
/**************************************************************************/
boolean Adafruit_TSL2561_Unified::begin()
{
  _i2c = &Wire;
  _i2c->begin();
  return init();
}

/**************************************************************************/
/*!
    @brief Initializes I2C and configures the sensor with provided I2C device
           (call this function before doing anything else)
    @param theWire A pointer to any I2C interface (e.g. &Wire1)
    @returns True if sensor is found and initialized, false otherwise.
*/
/**************************************************************************/
boolean Adafruit_TSL2561_Unified::begin(TwoWire *theWire)
{
  _i2c = theWire;
  _i2c-> begin();
  return init();
}

/**************************************************************************/
/*!
    @brief  Initializes I2C connection and settings.
    Attempts to determine if the sensor is contactable, then sets up a default
    integration time and gain. Then powers down the chip.
    @returns True if sensor is found and initialized, false otherwise.
*/
/**************************************************************************/
boolean Adafruit_TSL2561_Unified::init()
{
  /* Make sure we're actually connected */
  uint8_t x = read8(TSL2561_REGISTER_ID);
  if (x & 0x05) { // ID code for TSL2561
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
    @param enable Set to true to enable, False to disable
*/
/**************************************************************************/
void Adafruit_TSL2561_Unified::enableAutoRange(bool enable)
{
   _tsl2561AutoGain = enable;
}

/**************************************************************************/
/*!
    @brief      Sets the integration time for the TSL2561. Higher time means
                more light captured (better for low light conditions) but will
		take longer to run readings.
    @param time The amount of time we'd like to add up values
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
    @brief  Adjusts the gain on the TSL2561 (adjusts the sensitivity to light)
    @param gain The value we'd like to set the gain to
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
    @param  broadband Pointer to a uint16_t we will fill with a sensor
                      reading from the IR+visible light diode.
    @param  ir Pointer to a uint16_t we will fill with a sensor the
               IR-only light diode.
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
    @brief  Converts the raw sensor values to the standard SI lux equivalent.
    @param  broadband The 16-bit sensor reading from the IR+visible light diode.
    @param  ir The 16-bit sensor reading from the IR-only light diode.
    @returns The integer Lux value we calcuated.
             Returns 0 if the sensor is saturated and the values are
             unreliable, or 65536 if the sensor is saturated.
*/
/**************************************************************************/
/**************************************************************************/
/*!

    Returns
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
    Converts a standard SI lux value to a raw sensor value (for channel 0),
    taking into account the configured gain for the sensor. Required for
    SI lux consistency, and needed for interrupt threshold configuration.

    Important:
     * first configure the integration time and gain.
     * in case of autogain, the gain factor can be adjusted depending on the
       measurement; you might want to perform a measurement first to autoconfigure
       it, or avoid autogain alltogether with interrupts.
     * changing the integration value requires changing the interrupt thresholds!

    -----------------------------------------------------------------------

    TIJA: So, funny story: the raw values (ch0/ch1) to lux conversion takes
    both values into account. For interrupts, we can only configure a
    threshold for channel 0. So the problem is: given a lux value, can
    we make an educated guess for what the channel 0 value would be, leaving
    channel 1 as an undetermined parameter?

    If you look at the datasheet p23, you'll see the ratio of both is
    determining what formula to use.
    (https://cdn-shop.adafruit.com/datasheets/TSL2561.pdf)

    Here, we take the naieve assumption that the expected ratio is that as
    covered by the sensitivity curves of Figure 4, p8. The ratio of the
    area under both curves is roughly:
      * area under curve channel0: 24 squares
      * area under curve channel1: 7.5 squares
      * ratio of areas under curve: 7.5 / 24 = 0.3125

    So the most unbiased estimation for the ration is somewhere around 0.31 - 0.32.
    For conditions under sunlight, this was experimentally verified and more or
    less holds. Under artificial light (e.g. LED light), the IR component is
    (not very surprisingly) much smaller and leans more towards ratio 0.11 !!!
    Very roughly speaking the (black body) light spectrum of the sun is, again
    very roughly speaking, ""uniform"" (*cough* cutting corners *cough*) across
    the sensitivity range of the sensor (300-1100nm wavelength).

    It is possible to supply different estimations for the ratio, depending
    on your specific lighting conditions where you need thresholds (because that
    would be the only situation where I would recommend using this function).

      TSL2561_APPROXCHRATIO_SUN = 0.325
      TSL2561_APPROXCHRATIO_LED = 0.100

    The formula's, I refer to p23 of the datasheet to see where they come from
    (which seems to be an empiric formula from TAOS).

    Note: below is a mixture of fixed point & floating point math; sorry, did
    not have the time to rewrite everything in fixed point math.

*/
/**************************************************************************/
uint32_t Adafruit_TSL2561_Unified::calculateRawCH0(uint16_t lux, float approxChRatio)
{
  unsigned long luxScale;
  unsigned long ch0Scale = 0;
  unsigned long chScale;

  /* First, go to fixed point math scale -- reference scale is 2^TSL2561_LUX_CHSCALE = 2^10 */
  luxScale = (lux << TSL2561_LUX_CHSCALE);

  /* Next, calculate from lux to (scaled) channel0 values */
#ifdef TSL2561_PACKAGE_CS
  // CS Package
  // ----------------------
  // For calculating CH1/CH0 to Lux (p23 datasheet):
  //    For 0 < CH1/CH0 < 0.52      Lux = 0.0315 * CH0 - 0.0593 * CH0 * ((CH1/CH0)^1.4)
  //    For 0.52 < CH1/CH0 < 0.65   Lux = 0.0229 * CH0 - 0.0291 * CH1
  //    For 0.65 < CH1/CH0 < 0.80   Lux = 0.0157 * CH0 - 0.0180 * CH1
  //    For 0.80 < CH1/CH0 < 1.30   Lux = 0.00338 * CH0 - 0.00260 * CH1
  //    For CH1/CH0 > 1.30          Lux = 0
  //
  // Replacing CH1 = (CH1/CH0) * CH0 = approxChRatio * CH0, we can solve all
  // of these for CH0:
  //
  //    For 0 < CH1/CH0 < 0.52      Lux = (0.0315 - 0.0593 * ((approxChRatio)^1.4) )* CH0
  //    For 0.52 < CH1/CH0 < 0.65   Lux = (0.0229 - 0.0291 * approxChRatio) * CH0
  //    For 0.65 < CH1/CH0 < 0.80   Lux = (0.0157 - 0.0180 * approxChRatio) * CH0
  //    For 0.80 < CH1/CH0 < 1.30   Lux = (0.00338 - 0.00260 * approxChRatio) * CH0
  //    For CH1/CH0 > 1.30          Lux = 0
  //

  float divisor = 1;
  if(approxChRatio > 1.30) {
    return 0xFFFF;
  } else if(approxChRatio > 0.80) {
    divisor = (0.00338 - 0.00260 * approxChRatio);
  } else if(approxChRatio > 0.65) {
    divisor = (0.0157 - 0.0180 * approxChRatio);
  } else if(approxChRatio > 0.52) {
    divisor = (0.0229 - 0.0291 * approxChRatio);
  } else {
    // 0 < chRatio < 0.50
    divisor = (0.0315 - 0.0593 * pow(approxChRatio, 1.4));
  }
#else
  // T, FN, and CL Package
  // ----------------------
  // For calculating CH1/CH0 to Lux (p23 datasheet):
  //    For 0 < CH1/CH0 < 0.50     Lux = 0.0304 * CH0 - 0.062 * CH0 * ((CH1/CH0)^1.4)
  //    For 0.50 < CH1/CH0 < 0.61  Lux = 0.0224 * CH0 - 0.031 * CH1
  //    For 0.61 < CH1/CH0 < 0.80  Lux = 0.0128  * CH0 - 0.0153 * CH1
  //    For 0.80 < CH1/CH0 < 1.30  Lux = 0.00146 * CH0 - 0.00112 * CH1
  //    For CH1/CH0 > 1.30         Lux = 0
  //
  // Replacing CH1 = (CH1/CH0) * CH0 = approxChRatio * CH0, we can solve all
  // of these for CH0:
  //
  //    For 0 < CH1/CH0 < 0.50     Lux = ( 0.0304 - 0.062 * ((approxChRatio)^1.4)) * CH0
  //    For 0.50 < CH1/CH0 < 0.61  Lux = (0.0224 - 0.031 * approxChRatio) * CH0
  //    For 0.61 < CH1/CH0 < 0.80  Lux = (0.0128 - 0.0153 * approxChRatio) * CH0
  //    For 0.80 < CH1/CH0 < 1.30  Lux = (0.00146 - 0.00112 * approxChRatio) * CH0
  //    For CH1/CH0 > 1.30         Lux = 0 --> we return the highest possible value 0xFFFF
  //

  float divisor = 1;
  if(approxChRatio > 1.30) {
    // Easy one :)
    return 0xFFFF;
  } else if(approxChRatio > 0.80) {
    divisor = (0.00146 - 0.00112 * approxChRatio);
  } else if(approxChRatio > 0.61) {
    divisor = (0.0128 - 0.0153 * approxChRatio);
  } else if(approxChRatio > 0.50) {
    divisor = (0.0224 - 0.031 * approxChRatio);
  } else {
    // 0 < chRatio < 0.50
    divisor = (0.0304 - 0.062 * pow(approxChRatio, 1.4));
  }

  // Calculate CH0 value (fixed point scaled)
  if (divisor > 0) {
    ch0Scale = (unsigned long)(luxScale / divisor);
  } else {
    return 0xFFFF;
  }
#endif

  /* Now, we need to scale back down... the correct scale depends on the
     integration time and the gain */

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
  if (!_tsl2561Gain) {
    chScale = chScale << 4;
  }

  /* Scale the channel value back */
  return (ch0Scale / chScale);
}

/**************************************************************************/
/*!
    @brief  Gets the most recent sensor event
    @param  event Pointer to a sensor_event_t type that will be filled
                  with the lux value, timestamp, data type and sensor ID.
    @returns True if sensor reading is between 0 and 65535 lux,
             false if sensor is saturated
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
    @param  sensor A pointer to a sensor_t structure that we will fill with
                   details about the TSL2561 and its capabilities
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
  sensor->min_value   = 1.0;
  sensor->resolution  = 1.0;
}


/**************************************************************************/
/*!
    @brief  Sets up interrupt control on the TSL2561

    @param intcontrol Interrupt Control setting, explained below
    @param intpersist The amount of integration cycles the value must be outside
                      of the threshold to trigger an interrupt

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

  /* Update the interrupt control register */
  write8(cmd, data);

  /* Turn the device off to save power */
  // NOTE: This disables interrupts so no good idea if you need them :)
  if(_allowSleep) disable();
}

/**************************************************************************/
/*!
    @brief  Set interrupt thresholds (TSL2561 supports only interrupts
    generated by thresholds on channel 0).

    @param lowThreshold   The lower interrupt threshold
    @param highThreshold  The higher interrupt threshold

    Important: values supplied as thresholds are raw sensor values, and
    NOT values in the SI lux unit. In order to get an estimation of the
    raw value to pass here when you want a lux value as a threshold, use
    the calculateRawCH0() function with the right approximation parameter.
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

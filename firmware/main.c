/*
*   ALM014 'FIZZLE GUTS' firmware 
*   
*   Copyright (c) 2014 ALM Co Ltd
*   
*   Permission is hereby granted, free of charge, to any person
*   obtaining a copy of this software and associated documentation files
*   (the "Software"), to deal in the Software without restriction,
*   including without limitation the rights to use, copy, modify, merge,
*   publish, distribute, sublicense, and/or sell copies of the Software,
*   and to permit persons to whom the Software is furnished to do so,
*   subject to the following conditions:
*   
*   The above copyright notice and this permission notice shall be included in all
*   copies or substantial portions of the Software.
*   
*   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*   SOFTWARE.
*   
*/
#include "uu.h"
#include <avr/pgmspace.h>
#include <util/delay.h>


#define PIN_FZ_D0 PIN_B0
#define PIN_FZ_D1 PIN_B1
#define PIN_FZ_D2 PIN_B2
#define PIN_FZ_D3 PIN_B3
#define PIN_FZ_D4 PIN_D2
#define PIN_FZ_D5 PIN_D3
#define PIN_FZ_D6 PIN_D4
#define PIN_FZ_D7 PIN_D5

#define PIN_FZ_CHS PIN_C5
#define PIN_FZ_FA  PIN_C4
#define PIN_FZ_FCQ PIN_C3
#define PIN_FZ_RES PIN_C2
#define PIN_FZ_RW  PIN_C1
#define PIN_FZ_CE  PIN_C0

#define ADC_CHAN_FILTER_0 0
#define ADC_CHAN_FILTER_1 1
#define ADC_CHAN_RES_0 2
#define ADC_CHAN_RES_1 3
#define ADC_CHAN_DCA_0 4
#define ADC_CHAN_DCA_1 5


void cycle ();

int analog_read(uint8_t chan)
{
  uint8_t low, high;
  
  /* switch to ADMUX for (1<<6) - AVcc with external capacitor on AREF pin  */
  ADMUX = chan; // AREF, Internal Vref turned off  & chan 0 

  _delay_us(100); /* wait for mux to settle */
  
  ADCSRA = (1<<ADEN) | (1<<ADPS2) | (1<<ADPS1);
  /* Start conversion */
  ADCSRA |= (1<<ADSC);
  /* Wait.. */
  while (uu_bit_is_set(ADCSRA, ADSC));
  /* Collect */
  low  = ADCL;
  high = ADCH;

  return (high << 8) | low;
}

void FZ_poke (uint8_t data) 
{
  uu_pin_digital_write (PIN_FZ_RW, LOW); /* chip write enable */
  
  uu_pin_digital_write( PIN_FZ_D0, (data & 0x1));
  uu_pin_digital_write( PIN_FZ_D1, ((data>>1) & 0x1));
  uu_pin_digital_write( PIN_FZ_D2, ((data>>2) & 0x1));
  uu_pin_digital_write( PIN_FZ_D3, ((data>>3) & 0x1));
  uu_pin_digital_write( PIN_FZ_D4, ((data>>4) & 0x1));
  uu_pin_digital_write( PIN_FZ_D5, ((data>>5) & 0x1));
  uu_pin_digital_write( PIN_FZ_D6, ((data>>6) & 0x1));
  uu_pin_digital_write( PIN_FZ_D7, ((data>>7) & 0x1));

  _delay_us(10);

  uu_pin_digital_write (PIN_FZ_RW, HIGH); /* chip write disable */
}  


void setup () 
{
  int c, i = 0;

  uu_init(0);

  /* ADC's */
  uu_pin_mode( PIN_A0, INPUT);
  uu_pin_mode( PIN_A1, INPUT);
  uu_pin_mode( PIN_A2, INPUT);
  uu_pin_mode( PIN_A3, INPUT);
  uu_pin_mode( PIN_A4, INPUT);
  uu_pin_mode( PIN_A5, INPUT);
  
  uu_pin_mode( PIN_FZ_D0, OUTPUT);
  uu_pin_mode( PIN_FZ_D1, OUTPUT);
  uu_pin_mode( PIN_FZ_D2, OUTPUT);
  uu_pin_mode( PIN_FZ_D3, OUTPUT);
  uu_pin_mode( PIN_FZ_D4, OUTPUT);
  uu_pin_mode( PIN_FZ_D5, OUTPUT);
  uu_pin_mode( PIN_FZ_D6, OUTPUT);
  uu_pin_mode( PIN_FZ_D7, OUTPUT);
  
  uu_pin_mode( PIN_FZ_CHS, OUTPUT);
  uu_pin_mode( PIN_FZ_FA, OUTPUT);
  uu_pin_mode( PIN_FZ_FCQ, OUTPUT);
  uu_pin_mode( PIN_FZ_RES, OUTPUT);
  uu_pin_mode( PIN_FZ_RW, OUTPUT);
  uu_pin_mode( PIN_FZ_CE, OUTPUT);
  
  uu_pin_digital_write (PIN_FZ_CE, LOW); /* chip enable */

  uu_pin_digital_write (PIN_FZ_RW, HIGH); /* chip write disable */
  
  uu_pin_digital_write (PIN_FZ_RES, LOW); /* Reset - low active */
  uu_delay_ms(100);
  uu_pin_digital_write (PIN_FZ_RES, HIGH); /* Reset - low active */
  

  uu_pin_digital_write (PIN_FZ_CHS, HIGH); /* channel 0 - channel select */

  uu_pin_digital_write (PIN_FZ_FA, LOW); /* gain - 10 bit? */
  uu_pin_digital_write (PIN_FZ_FCQ, LOW); /* ??? */

  FZ_poke (0xff);  		/* Gain - all 8 bits */

  uu_pin_digital_write (PIN_FZ_FCQ, HIGH); /* gain */

  FZ_poke (0xff);  		/* Gain - just upper 2 bits */
  
}

void cycle () 
{
  static int prev_dca[2], prev_cutoff[2], prev_rez[2], hyst_dca[2], hyst_cutoff[2], hyst_rez[2];;

  int dca, cutoff = 0, rez = 0, i = 0;

  /* Note 6 over 4 as seems to be much more noise on ADC */
#define DO_HYST(c,p) if ( ((c) > (p) + 4) || ((c) < (p) - 4) ) { (p) = (c); } else { (c) = (p); }

  for (i=0; i<2; i++)
    {
      uu_pin_digital_write (PIN_FZ_CHS, !i); /* channel select */
     
      dca = 0x3ff - analog_read(ADC_CHAN_DCA_0+i);

      DO_HYST(dca, hyst_dca[i]);
  
      if (dca != prev_dca[i])
	{
	  uu_pin_digital_write (PIN_FZ_FA, LOW); /* gain - 10 bit? */
	  uu_pin_digital_write (PIN_FZ_FCQ, HIGH); /* gain */

	  /* 3 for high gain not to zero*/
	  /* 1 for for low gain */
	  
	  FZ_poke (dca >> 8);  		/* Gain - just upper 2 bits - do first*/
	  uu_pin_digital_write (PIN_FZ_FA, LOW); /* gain - 10 bit? */
	  uu_pin_digital_write (PIN_FZ_FCQ, LOW); /* ??? */
	  FZ_poke (dca & 0xff);
      
	  prev_dca[i] = dca;
	}

      //rez = 0xf - (analog_read(ADC_CHAN_RES_0) >> 6); /* 4 bit? */
      rez = (0x3ff - (analog_read(ADC_CHAN_RES_0+i))); /* 4 bit? */

      DO_HYST(rez, hyst_rez[i]);
      
      if (rez != prev_rez[i])
	{
	  uu_pin_digital_write (PIN_FZ_FA, HIGH); /* High DCF control */
	  uu_pin_digital_write (PIN_FZ_FCQ, LOW); /* ??? */

	  FZ_poke (rez >> 6);  		/* resonance - MSB? just 4 bits */
	  
	  prev_rez[i] = rez;
	}

      cutoff = analog_read(ADC_CHAN_FILTER_0+i); /* 7 bit? */

      DO_HYST(cutoff, hyst_cutoff[i]);
      
      if (cutoff != prev_cutoff[i])
	{
	  uu_pin_digital_write (PIN_FZ_FA, HIGH);
	  uu_pin_digital_write (PIN_FZ_FCQ, HIGH);

	  FZ_poke (cutoff >> 3);  		/* filter cut off ? */

	  prev_cutoff[i] = cutoff;
	}
    }

}


int main(void)
{
  setup();
  while (TRUE)
    cycle();
}

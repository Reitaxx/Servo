#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/pio.h"
#include "MECHws2812.h"
#include "MECHws2812.pio.h"


#define ADC_Pin 26
uint16_t ADC_Var = 0;
#define Servo_Pin 15
#define WS2812_PIN 16


uint32_t pwm_set_freq_duty(uint slice_num, uint chan, uint32_t f, int d)
{
    uint32_t clock = 125000000;
    uint32_t divider16 = clock / f / 4096 + (clock % (f * 4096) != 0);
    if (divider16 / 16 == 0)
        divider16 = 16;
    uint32_t wrap = clock * 16 / divider16 / f - 1;
    pwm_set_clkdiv_int_frac(slice_num, divider16 / 16, divider16 & 0xF);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, chan, wrap * d / 100);
    return wrap;
}

uint32_t pwm_get_wrap(uint slice_num)
{
    // valid_params_if(PWM, slice_num >= 0 && slice_num < NUM_PWM_SLICES);
    return pwm_hw->slice[slice_num].top;
}

void pwm_set_duty(uint slice_num, uint chan, int d)
{
    pwm_set_chan_level(slice_num, chan, pwm_get_wrap(slice_num) * d / 100);
}

void pwm_set_dutyH(uint slice_num, uint chan, int d)
{
    pwm_set_chan_level(slice_num, chan, pwm_get_wrap(slice_num) * d / 10000);
}

typedef struct
{
    uint gpio;
    uint slice;
    uint chan;
    uint speed;
    uint resolution;
    bool on;
    bool invert;
} Servo;

void ServoInit(Servo *s, uint gpio, bool invert)
{
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    s->gpio = gpio;
    s->slice = pwm_gpio_to_slice_num(gpio);
    s->chan = pwm_gpio_to_channel(gpio);

    pwm_set_enabled(s->slice, false);
    s->on = false;
    s->speed = 0;
    s->resolution = pwm_set_freq_duty(s->slice, s->chan, 50, 0);
    pwm_set_dutyH(s->slice, s->chan, 250);
    if (s->chan)
    {
        pwm_set_output_polarity(s->slice, false, invert);
    }
    else
    {
        pwm_set_output_polarity(s->slice, invert, false);
    }
    s->invert = invert;
}

void ServoOn(Servo *s)
{
    pwm_set_enabled(s->slice, true);
    s->on = true;
}

void ServoOff(Servo *s)
{
    pwm_set_enabled(s->slice, false);
    s->on = false;
}
void ServoPosition(Servo *s, uint p)
{
    pwm_set_dutyH(s->slice, s->chan, p * 10 + 250);
}

// global variables for ws2812
PIO ws2812_pio;
uint ws2812_sm;
uint offset;


//map position servo position to rainbow colors
uint32_t map_position_to_color(uint position)
{
    // Implementation for mapping position to color
    uint8_t r, g, b;
    uint16_t hue = (position * 360) / 100; // Map position to hue (0-360)
    
    if(hue < 60) {
        r = 255;
        g = (hue * 255) / 60;
        b = 0;
    } else if (hue < 120) {
        r = 255 - ((hue - 60) * 255) / 60;
        g = 255;
        b = 0;
    } else if (hue < 180) {
        r = 0;
        g = 255;
        b = ((hue - 120) * 255) / 60;
    } else if (hue < 240) {
        r = 0;
        g = ((hue - 180) * 255) / 60;
        b = 255;
    } else if (hue < 300) {
        r = ((hue - 240) * 255) / 60;
        g = 0;
        b = 255;
    } else {
        r = 255;
        g = 0;
        b = 255 - ((hue - 300) * 255) / 60;
    }
    return (r << 16) | (g << 8) | b;
}

int main()
{

    sleep_ms(2000); // Wait for 2 seconds to ensure everything is set up

    uint position = 0;
    uint offset = 0;

//initialize ws2812
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&ws2812_program, &ws2812_pio, &ws2812_sm, &offset, WS2812_PIN, 1, true);
    hard_assert(success);

// Initialize the WS2812 program with the specified parameters
    ws2812_program_init(ws2812_pio, ws2812_sm, offset, WS2812_PIN, 800000, IS_RGBW);

//initialize servo and adc
    adc_init();
    adc_gpio_init(ADC_Pin);
    adc_select_input(0);
    Servo s1;
    ServoInit(&s1, Servo_Pin, false);
    ServoOn(&s1);

    while (true)
    {
        ADC_Var = adc_read();
        position = (ADC_Var * 100) / 4095;

        ServoPosition(&s1, position);
        uint32_t color = map_position_to_color(position);
        put_pixel(ws2812_pio, ws2812_sm, color);

        sleep_ms(20);
    }

    return 0;
}
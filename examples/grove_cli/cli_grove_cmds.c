/**
 * Copyright (c) 2018, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "app_timer.h"
#include "nrf_drv_gpiote.h"
#include "nrf_cli.h"
#include "nrf_drv_saadc.h"
#include "nrf_delay.h"
#include "nrf_log.h"


/*lint -save -e689 */ /* Apparent end of comment ignored */
#include "arm_const_structs.h"
/*lint -restore */
#include "arm_math.h"


#if defined( __GNUC__ ) && (__LINT__ == 0)
   // This is required if one wants to use floating-point values in ‘printf’
   // (by default this feature is not linked together with newlib-nano).
   // Please note, however, that this adds about 13 kB code footprint...
   __ASM(".global _printf_float");
#endif


#define LIGHT_SAMPLE_COUNT 10
#define TEMP_SAMPLE_COUNT  10
#define ANGLE_SAMPLE_COUNT 10
#define SOUND_SAMPLE_COUNT 128

#define DETECTION_DELAY_TICKS APP_TIMER_TICKS(20)

const int THERMISTOR_B = 4275;               // B value of the thermistor
const int THERMISTOR_R0 = 100000;            // R0 = 100k
const int ANGLE_FULL_RANGE = 300;            // full value of the rotary angle is 300 degrees
const int GROVE_PWR_VCC = 3300;               // VCC of the grove interface is 3300mV

uint32_t GROVE_PORT_SIG_TO_PIN[] = { 27, 29, 31, 3 };

static bool m_detection_delay_timer_created = false;

APP_TIMER_DEF(m_detection_delay_timer_id);  /**< Polling timer id. */

static uint64_t m_pin_state = 0;
static uint64_t m_pin_transition = 0;
static uint64_t m_button_pin_enabled = 0;
static uint64_t m_touch_pin_enabled = 0;

nrf_cli_t const * p_current_cli = NULL;

static void detection_delay_timeout_handler(void * p_context)
{
    uint8_t i;

    for (i = 0; i < 4; i++)
    {
        uint64_t pin_mask = 1ULL << GROVE_PORT_SIG_TO_PIN[i];
        if (pin_mask & m_pin_transition)
        {
            m_pin_transition &= ~pin_mask;
            bool pin_is_set = nrf_drv_gpiote_in_is_set(GROVE_PORT_SIG_TO_PIN[i]);
            if ((m_pin_state & pin_mask) == (((uint64_t)pin_is_set) << GROVE_PORT_SIG_TO_PIN[i]))
            {
                if(p_current_cli != NULL)
                {
                    if(m_button_pin_enabled & pin_mask)
                    {
                        nrf_cli_fprintf(p_current_cli, NRF_CLI_NORMAL, "\r\nButton on Port%d: %s", i+1, pin_is_set? "Pressed":"Released");
                    }
                    else if(m_touch_pin_enabled & pin_mask)
                    {
                        nrf_cli_fprintf(p_current_cli, NRF_CLI_NORMAL, "\r\nTouch on Port%d: %s", i+1, pin_is_set? "Touched":"Released");
                    }
                }
                
            }
        }
    }
}

static void gpiote_event_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    uint32_t err_code;
    uint64_t pin_mask = 1ULL << pin;

    // Start detection timer. If timer is already running, the detection period is restarted.
    // NOTE: Using the p_context parameter of app_timer_start() to transfer the pin states to the
    //       timeout handler (by casting event_pins_mask into the equally sized void * p_context
    //       parameter).
    err_code = app_timer_stop(m_detection_delay_timer_id);
    if (err_code != NRF_SUCCESS)
    {
        // The impact in app_button of the app_timer queue running full is losing a button press.
        // The current implementation ensures that the system will continue working as normal.
        return;
    }

    if (!(m_pin_transition & pin_mask))
    {
        if (nrf_drv_gpiote_in_is_set(pin))
        {
            m_pin_state |= pin_mask;
        }
        else
        {
            m_pin_state &= ~(pin_mask);
        }
        m_pin_transition |= (pin_mask);

        err_code = app_timer_start(m_detection_delay_timer_id, DETECTION_DELAY_TICKS, NULL);
        if (err_code != NRF_SUCCESS)
        {
            // The impact in app_button of the app_timer queue running full is losing a button press.
            // The current implementation ensures that the system will continue working as normal.
        }
    }
    else
    {
        m_pin_transition &= ~pin_mask;
    }
}

static void saadc_callback(nrf_drv_saadc_evt_t const * p_event)
{
    if (p_event->type == NRF_DRV_SAADC_EVT_DONE)
    {
        //
    }
}

/**
 * @brief Function for `grove` command.
 */
static void cmd_grove(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    ASSERT(p_cli);
    ASSERT(p_cli->p_ctx && p_cli->p_iface && p_cli->p_name);

    if ((argc == 1) || nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, NULL, 0);
        return;
    }

    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s %s: command not found\r\n", argv[0], argv[1]);
}


/**
 * @brief Function for Grove Rotary Angle sensor control.
 */
static void cmd_grove_angle(nrf_cli_t const * p_cli, size_t argc, char **argv)
{

    ret_code_t err_code;
    nrf_drv_saadc_config_t saadc_config = NRF_DRV_SAADC_DEFAULT_CONFIG;
    nrf_saadc_channel_config_t channel_config =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_DISABLED);

    nrf_saadc_value_t adc_value;

    /* Extra defined port option */
    static const nrf_cli_getopt_option_t opt[] = {
        NRF_CLI_OPT(
            "--port",
            "-p",
            "Port number to which the sensor is connected. Choose from: 2|3|4"
        )
    };

    if ((argc == 1) || nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, opt, ARRAY_SIZE(opt));
        return;
    }

    if (!strcmp(argv[1], "-p") || !strcmp(argv[1], "--port"))
    {
        if(argc == 2)
        {
            nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "Choose a port number: 2, 3 or 4.\r\n");
            return;
        }
        else if(argc == 3)
        {
            switch (atoi(argv[2]))
            {
                case 2:
                    channel_config.pin_p = NRF_SAADC_INPUT_AIN5;
                    break;

                case 3:
                    channel_config.pin_p = NRF_SAADC_INPUT_AIN7;
                    break;

                case 4:
                    channel_config.pin_p = NRF_SAADC_INPUT_AIN1;
                    break;

                default:
                    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: port not available.\r\n", argv[1]);
                    return;
            }

            /* Burst enabled to oversample the SAADC. */
            channel_config.burst    = NRF_SAADC_BURST_ENABLED;
            channel_config.acq_time = NRF_SAADC_ACQTIME_40US;

            /* Burst enabled to oversample the SAADC. */
            channel_config.burst    = NRF_SAADC_BURST_ENABLED;
            channel_config.acq_time = NRF_SAADC_ACQTIME_40US;

            err_code = nrf_drv_saadc_init(&saadc_config, saadc_callback);
            APP_ERROR_CHECK(err_code);

            err_code = nrf_drv_saadc_channel_init(0, &channel_config);
            APP_ERROR_CHECK(err_code);

            int32_t sum = 0;
            int32_t voltage = 0;
            uint16_t angle_degree = 0;

            for (int i = 0; i < ANGLE_SAMPLE_COUNT; ++i)
            {
                err_code = nrf_drv_saadc_sample_convert(0, &adc_value);
                APP_ERROR_CHECK(err_code);

                sum += adc_value;

                nrf_delay_ms(10);
            }

            adc_value = sum / ANGLE_SAMPLE_COUNT;

            voltage = adc_value*3600/1024;
            angle_degree = voltage * ANGLE_FULL_RANGE / GROVE_PWR_VCC;

            nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "ADC RAW: %d\r\nAngle: %d[degree]\r\n", adc_value, angle_degree);
            
            nrf_drv_saadc_uninit();
        }
        else
        {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s %s: port not available\r\n", argv[2], argv[3]);
            return;
        }
    }
    else
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: command not found\r\n", argv[1]);
    }
}


/**
 * @brief Function for Grove Button control.
 */
static void cmd_grove_button(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    if ((argc == 1) || nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, NULL, 0);
        return;
    }

    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s %s: command not found\r\n", argv[0], argv[1]);
}

static void cmd_grove_button_enable(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    ret_code_t err_code;

    /* Extra defined port option */
    static const nrf_cli_getopt_option_t opt[] = {
        NRF_CLI_OPT(
            "--port",
            "-p",
            "Port number to which the button is connected. Choose from: 1~4"
        )
    };

    if ((argc == 1) || nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, opt, ARRAY_SIZE(opt));
        return;
    }

    if (!strcmp(argv[1], "-p") || !strcmp(argv[1], "--port"))
    {
        if(argc == 2)
        {
            nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "Choose a port number: 1~4.\r\n");
            return;
        }
        else if(argc == 3)
        {
            nrf_drv_gpiote_in_config_t config = GPIOTE_CONFIG_IN_SENSE_TOGGLE(false);
            config.pull = NRF_GPIO_PIN_PULLDOWN;

            uint32_t pin_no;

            switch (atoi(argv[2]))
            {
                case 1:
                case 2:
                case 3:
                case 4:
                    pin_no = GROVE_PORT_SIG_TO_PIN[atoi(argv[2])-1];
                    break;
                default:
                    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: port not available.\r\n", argv[2]);
                    return;
            }

            uint64_t pin_mask = 1ULL << pin_no;

            if (!nrf_drv_gpiote_is_init())
            {
                err_code = nrf_drv_gpiote_init();
                APP_ERROR_CHECK(err_code);
            }

            // Create polling timer.
            if(!m_detection_delay_timer_created)
            {
                err_code = app_timer_create(&m_detection_delay_timer_id,
                                            APP_TIMER_MODE_SINGLE_SHOT,
                                            detection_delay_timeout_handler);
                APP_ERROR_CHECK(err_code);
                m_detection_delay_timer_created = true;
            }

            if(m_touch_pin_enabled & pin_mask)
            {
                nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "Port%s is already enabled for Grove Touch\r\n", argv[2]);
                return;
            }

            if(!(m_button_pin_enabled & pin_mask))
            {
                err_code = nrf_drv_gpiote_in_init(pin_no, &config, gpiote_event_handler);
                APP_ERROR_CHECK(err_code);
                nrf_drv_gpiote_in_event_enable(pin_no, true);

                m_button_pin_enabled |= pin_mask;
            }

            p_current_cli = p_cli;

            nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "Grove Button on Port%s Enabled\r\n", argv[2]);

        }
    }
    else
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: command not found\r\n", argv[1]);
    }
}

static void cmd_grove_button_disable(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    /* Extra defined port option */
    static const nrf_cli_getopt_option_t opt[] = {
        NRF_CLI_OPT(
            "--port",
            "-p",
            "Port number to which the button is connected. Choose from: 1~4"
        )
    };

    if ((argc == 1) || nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, opt, ARRAY_SIZE(opt));
        return;
    }

    if (!strcmp(argv[1], "-p") || !strcmp(argv[1], "--port"))
    {
        if(argc == 2)
        {
            nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "Choose a port number: 1~4.\r\n");
            return;
        }
        else if(argc == 3)
        {
            uint32_t pin_no;

            switch (atoi(argv[2]))
            {
                case 1:
                case 2:
                case 3:
                case 4:
                    pin_no = GROVE_PORT_SIG_TO_PIN[atoi(argv[2])-1];
                    break;
                default:
                    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: port not available.\r\n", argv[2]);
                    return;
            }

            uint64_t pin_mask = 1ULL << pin_no;

            if(m_button_pin_enabled & pin_mask)
            {
                nrf_drv_gpiote_in_event_disable(pin_no);
                nrfx_gpiote_in_uninit(pin_no);
                m_button_pin_enabled &= ~pin_mask;
            }

            nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "Grove Button on Port%s Disabled\r\n", argv[2]);
        }
    }
    else
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: command not found\r\n", argv[1]);
    }
}

/**
 * @brief Function for Grove Light sensor control.
 */
static void cmd_grove_light(nrf_cli_t const * p_cli, size_t argc, char **argv)
{

	ret_code_t err_code;
	nrf_drv_saadc_config_t saadc_config = NRF_DRV_SAADC_DEFAULT_CONFIG;
    nrf_saadc_channel_config_t channel_config =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_DISABLED);

    nrf_saadc_value_t adc_value;

    /* Extra defined port option */
    static const nrf_cli_getopt_option_t opt[] = {
        NRF_CLI_OPT(
            "--port",
            "-p",
            "Port number to which the sensor is connected. Choose from: 2|3|4"
        )
    };

    if ((argc == 1) || nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, opt, ARRAY_SIZE(opt));
        return;
    }

    if (!strcmp(argv[1], "-p") || !strcmp(argv[1], "--port"))
    {
    	if(argc == 2)
    	{
    		nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "Choose a port number: 2, 3 or 4.\r\n");
    		return;
    	}
    	else if(argc == 3)
    	{
    		switch (atoi(argv[2]))
    		{
    			case 2:
    			    channel_config.pin_p = NRF_SAADC_INPUT_AIN5;
    			    break;

    			case 3:
    			    channel_config.pin_p = NRF_SAADC_INPUT_AIN7;
    			    break;

    			case 4:
    			    channel_config.pin_p = NRF_SAADC_INPUT_AIN1;
    			    break;

    			default:
    			    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: port not available.\r\n", argv[2]);
    			    return;
    		}

    		/* Burst enabled to oversample the SAADC. */
            channel_config.burst    = NRF_SAADC_BURST_ENABLED;
            channel_config.acq_time = NRF_SAADC_ACQTIME_40US;

		    /* Burst enabled to oversample the SAADC. */
		    channel_config.burst    = NRF_SAADC_BURST_ENABLED;
		    channel_config.acq_time = NRF_SAADC_ACQTIME_40US;

		    err_code = nrf_drv_saadc_init(&saadc_config, saadc_callback);
		    APP_ERROR_CHECK(err_code);

		    err_code = nrf_drv_saadc_channel_init(0, &channel_config);
		    APP_ERROR_CHECK(err_code);

		    int32_t sum = 0;

		    for (int i = 0; i < LIGHT_SAMPLE_COUNT; ++i)
		    {
				err_code = nrf_drv_saadc_sample_convert(0, &adc_value);
				APP_ERROR_CHECK(err_code);

				sum += adc_value;

				nrf_delay_ms(10);
		    }

		    adc_value = sum / LIGHT_SAMPLE_COUNT;

		    nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "ADC RAW: %d\r\nVoltage: %d[mV]\r\n", adc_value, adc_value*3600/1024);
		    nrf_drv_saadc_uninit();
    	}
    	else
    	{
    		nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s %s: port not available\r\n", argv[2], argv[3]);
    		return;
    	}
    }
    else
    {
    	nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: command not found\r\n", argv[1]);
    }
}


/**
 * @brief Function for Grove Sound sensor control.
 */
static void cmd_grove_sound(nrf_cli_t const * p_cli, size_t argc, char **argv)
{

    ret_code_t err_code;
    nrf_drv_saadc_config_t saadc_config = NRF_DRV_SAADC_DEFAULT_CONFIG;
    nrf_saadc_channel_config_t channel_config =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_DISABLED);

    nrf_saadc_value_t adc_value;

    /* Extra defined port option */
    static const nrf_cli_getopt_option_t opt[] = {
        NRF_CLI_OPT(
            "--port",
            "-p",
            "Port number to which the sensor is connected. Choose from: 2|3|4"
        )
    };

    if ((argc == 1) || nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, opt, ARRAY_SIZE(opt));
        return;
    }

    if (!strcmp(argv[1], "-p") || !strcmp(argv[1], "--port"))
    {
        if(argc == 2)
        {
            nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "Choose a port number: 2, 3 or 4.\r\n");
            return;
        }
        else if(argc == 3)
        {
            switch (atoi(argv[2]))
            {
                case 2:
                    channel_config.pin_p = NRF_SAADC_INPUT_AIN5;
                    break;

                case 3:
                    channel_config.pin_p = NRF_SAADC_INPUT_AIN7;
                    break;

                case 4:
                    channel_config.pin_p = NRF_SAADC_INPUT_AIN1;
                    break;

                default:
                    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: port not available.\r\n", argv[1]);
                    return;
            }

            /* Burst enabled to oversample the SAADC. */
            channel_config.burst    = NRF_SAADC_BURST_ENABLED;
            channel_config.acq_time = NRF_SAADC_ACQTIME_40US;

            /* Burst enabled to oversample the SAADC. */
            channel_config.burst    = NRF_SAADC_BURST_ENABLED;
            channel_config.acq_time = NRF_SAADC_ACQTIME_40US;

            err_code = nrf_drv_saadc_init(&saadc_config, saadc_callback);
            APP_ERROR_CHECK(err_code);

            err_code = nrf_drv_saadc_channel_init(0, &channel_config);
            APP_ERROR_CHECK(err_code);

            float32_t sampleBuffer[SOUND_SAMPLE_COUNT];
            float32_t var = 0.f;

            for (int i = 0; i < SOUND_SAMPLE_COUNT; ++i)
            {
                err_code = nrf_drv_saadc_sample_convert(0, &adc_value);
                APP_ERROR_CHECK(err_code);

                sampleBuffer[i] = 1.0f * adc_value * 3600 / 1024;
                nrf_delay_us(125); // 8kHz
            }

            /* Estimate variance */
            arm_var_f32(sampleBuffer, SOUND_SAMPLE_COUNT, &var);

            nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "Sound Level: %d[dB]\r\n", (int)(10.0f * log10f(var)));
            
            nrf_drv_saadc_uninit();
        }
        else
        {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s %s: port not available\r\n", argv[2], argv[3]);
            return;
        }
    }
    else
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: command not found\r\n", argv[1]);
    }
}

/**
 * @brief Function for Grove Temperature sensor control.
 */
static void cmd_grove_temp(nrf_cli_t const * p_cli, size_t argc, char **argv)
{

    ret_code_t err_code;
    nrf_drv_saadc_config_t saadc_config = NRF_DRV_SAADC_DEFAULT_CONFIG;
    nrf_saadc_channel_config_t channel_config =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_DISABLED);

    nrf_saadc_value_t adc_value;

    /* Extra defined port option */
    static const nrf_cli_getopt_option_t opt[] = {
        NRF_CLI_OPT(
            "--port",
            "-p",
            "Port number to which the sensor is connected. Choose from: 2,3,4"
        )
    };

    if ((argc == 1) || nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, opt, ARRAY_SIZE(opt));
        return;
    }

    if (!strcmp(argv[1], "-p") || !strcmp(argv[1], "--port"))
    {
        if(argc == 2)
        {
            nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "Choose a port number: 2, 3 or 4.\r\n");
            return;
        }
        else if(argc == 3)
        {
            switch (atoi(argv[2]))
            {
                case 2:
                    channel_config.pin_p = NRF_SAADC_INPUT_AIN5;
                    break;

                case 3:
                    channel_config.pin_p = NRF_SAADC_INPUT_AIN7;
                    break;

                case 4:
                    channel_config.pin_p = NRF_SAADC_INPUT_AIN1;
                    break;

                default:
                    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: port not available.\r\n", argv[1]);
                    return;
            }

            /* Burst enabled to oversample the SAADC. */
            channel_config.burst    = NRF_SAADC_BURST_ENABLED;
            channel_config.acq_time = NRF_SAADC_ACQTIME_40US;

            /* Burst enabled to oversample the SAADC. */
            channel_config.burst    = NRF_SAADC_BURST_ENABLED;
            channel_config.acq_time = NRF_SAADC_ACQTIME_40US;

            err_code = nrf_drv_saadc_init(&saadc_config, saadc_callback);
            APP_ERROR_CHECK(err_code);

            err_code = nrf_drv_saadc_channel_init(0, &channel_config);
            APP_ERROR_CHECK(err_code);

            int32_t sum = 0;

            for (int i = 0; i < TEMP_SAMPLE_COUNT; ++i)
            {
                err_code = nrf_drv_saadc_sample_convert(0, &adc_value);
                APP_ERROR_CHECK(err_code);

                sum += adc_value;

                nrf_delay_ms(10);
            }

            adc_value = sum / TEMP_SAMPLE_COUNT;

            /* Follow http://wiki.seeedstudio.com/Grove-Temperature_Sensor_V1.2/ 
               to calculate the temperature */
            float R = 1023.0/adc_value-1.0;
            R = THERMISTOR_R0 * R;

            float temp = 1.0/(log(R/THERMISTOR_R0)/THERMISTOR_B+1/298.15)-273.15; // convert to temperature via datasheet

            char f_str[10];
            sprintf(f_str, "%.2f", temp);
            nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "ADC RAW: %d\r\nTemperature: %s[degreeC]\r\n", adc_value, f_str);
            
            nrf_drv_saadc_uninit();
        }
        else
        {
            nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s %s: port not available\r\n", argv[2], argv[3]);
            return;
        }
    }
    else
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: command not found\r\n", argv[1]);
    }
}

/**
 * @brief Function for Grove Touch control.
 */
static void cmd_grove_touch(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    if ((argc == 1) || nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, NULL, 0);
        return;
    }

    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s %s: command not found\r\n", argv[0], argv[1]);
}

static void cmd_grove_touch_enable(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    ret_code_t err_code;

    /* Extra defined port option */
    static const nrf_cli_getopt_option_t opt[] = {
        NRF_CLI_OPT(
            "--port",
            "-p",
            "Port number to which the sensor is connected. Choose from: 1~4"
        )
    };

    if ((argc == 1) || nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, opt, ARRAY_SIZE(opt));
        return;
    }

    if (!strcmp(argv[1], "-p") || !strcmp(argv[1], "--port"))
    {
        if(argc == 2)
        {
            nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "Choose a port number: 1~4.\r\n");
            return;
        }
        else if(argc == 3)
        {
            nrf_drv_gpiote_in_config_t config = GPIOTE_CONFIG_IN_SENSE_TOGGLE(false);
            config.pull = NRF_GPIO_PIN_PULLDOWN;

            uint32_t pin_no;

            switch (atoi(argv[2]))
            {
                case 1:
                case 2:
                case 3:
                case 4:
                    pin_no = GROVE_PORT_SIG_TO_PIN[atoi(argv[2])-1];
                    break;
                default:
                    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: port not available.\r\n", argv[2]);
                    return;
            }

            uint64_t pin_mask = 1ULL << pin_no;

            if (!nrf_drv_gpiote_is_init())
            {
                err_code = nrf_drv_gpiote_init();
                APP_ERROR_CHECK(err_code);
            }

            // Create polling timer.
            if(!m_detection_delay_timer_created)
            {
                err_code = app_timer_create(&m_detection_delay_timer_id,
                                            APP_TIMER_MODE_SINGLE_SHOT,
                                            detection_delay_timeout_handler);
                APP_ERROR_CHECK(err_code);
                m_detection_delay_timer_created = true;
            }

            if(m_button_pin_enabled & pin_mask)
            {
                nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "Port%s is already enabled for Grove Button\r\n", argv[2]);
                return;
            }

            if(!(m_touch_pin_enabled & pin_mask))
            {
                err_code = nrf_drv_gpiote_in_init(pin_no, &config, gpiote_event_handler);
                APP_ERROR_CHECK(err_code);
                nrf_drv_gpiote_in_event_enable(pin_no, true);

                m_touch_pin_enabled |= pin_mask;
            }

            p_current_cli = p_cli;

            nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "Grove Touch on Port%s Enabled\r\n", argv[2]);

        }
    }
    else
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: command not found\r\n", argv[1]);
    }
}

static void cmd_grove_touch_disable(nrf_cli_t const * p_cli, size_t argc, char **argv)
{
    /* Extra defined port option */
    static const nrf_cli_getopt_option_t opt[] = {
        NRF_CLI_OPT(
            "--port",
            "-p",
            "Port number to which the sensor is connected. Choose from: 1~4"
        )
    };

    if ((argc == 1) || nrf_cli_help_requested(p_cli))
    {
        nrf_cli_help_print(p_cli, opt, ARRAY_SIZE(opt));
        return;
    }

    if (!strcmp(argv[1], "-p") || !strcmp(argv[1], "--port"))
    {
        if(argc == 2)
        {
            nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "Choose a port number: 1~4.\r\n");
            return;
        }
        else if(argc == 3)
        {
            uint32_t pin_no;

            switch (atoi(argv[2]))
            {
                case 1:
                case 2:
                case 3:
                case 4:
                    pin_no = GROVE_PORT_SIG_TO_PIN[atoi(argv[2])-1];
                    break;
                default:
                    nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: port not available.\r\n", argv[2]);
                    return;
            }

            uint64_t pin_mask = 1ULL << pin_no;

            if(m_touch_pin_enabled & pin_mask)
            {
                nrf_drv_gpiote_in_event_disable(pin_no);
                nrfx_gpiote_in_uninit(pin_no);
                m_touch_pin_enabled &= ~pin_mask;
            }

            nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "Grove Touch on Port%s Disabled\r\n", argv[2]);
        }
    }
    else
    {
        nrf_cli_fprintf(p_cli, NRF_CLI_ERROR, "%s: command not found\r\n", argv[1]);
    }
}



NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_grove_button)
{
    NRF_CLI_CMD(enable, NULL, "'grove button enable -p {1~4}' Enable Grove Button on a port", cmd_grove_button_enable),
    NRF_CLI_CMD(disable, NULL, "'grove button disable -p {1~4}' Disable Grove Button on the port", cmd_grove_button_disable),
    NRF_CLI_SUBCMD_SET_END
};

NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_grove_touch)
{
    NRF_CLI_CMD(enable, NULL, "'grove touch enable -p {1~4}' Enable Grove Touch on a port", cmd_grove_touch_enable),
    NRF_CLI_CMD(disable, NULL, "'grove touch disable -p {1~4}' Disable Grove Touch on the port", cmd_grove_touch_disable),
    NRF_CLI_SUBCMD_SET_END
};


NRF_CLI_CREATE_STATIC_SUBCMD_SET(m_sub_grove)
{
    NRF_CLI_CMD(angle, NULL, "'grove angle -p {2,3,4}' Read Grove Rotary Angle sensor data", cmd_grove_angle),
    NRF_CLI_CMD(light, NULL, "'grove light -p {2,3,4}' Read Grove Light sensor data", cmd_grove_light),
    NRF_CLI_CMD(sound, NULL, "'grove sound -p {2,3,4}' Read Grove Sound sensor data", cmd_grove_sound),
    NRF_CLI_CMD(temp, NULL, "'grove temp -p {2,3,4}' Read Grove Temperature sensor data", cmd_grove_temp),
    NRF_CLI_CMD(button, &m_sub_grove_button, "Commands for Grove Button control", cmd_grove_button),
    NRF_CLI_CMD(touch, &m_sub_grove_touch, "Commands for Grove Touch control", cmd_grove_touch),
    NRF_CLI_SUBCMD_SET_END
};

NRF_CLI_CMD_REGISTER(grove, &m_sub_grove, "Commands for Grove modules control", cmd_grove);


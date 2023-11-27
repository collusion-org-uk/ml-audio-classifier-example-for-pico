/*
 * Copyright (c) 2021 Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 */

#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/uart.h"

extern "C" {
#include "pico/pdm_microphone.h"
#include "pico/analog_microphone.h"
}

#include "tflite_model.h"

#include "dsp_pipeline.h"
#include "ml_model.h"

using namespace std;

// constants
#define SAMPLE_RATE       16000
#define FFT_SIZE          256
#define SPECTRUM_SHIFT    4
#define INPUT_BUFFER_SIZE ((FFT_SIZE / 2) * SPECTRUM_SHIFT)
#define INPUT_SHIFT       0


#define UART_ID uart0
#define BAUD_RATE 9600
#define UART_TX_PIN 0
#define UART_RX_PIN 1

//// microphone configuration
//const struct analog_microphone_config config = {
//  .gpio = 26,
//  .bias_voltage = 1.65,
//  .sample_rate = SAMPLE_RATE,
//  .sample_buffer_size = INPUT_BUFFER_SIZE,
//};


const struct pdm_microphone_config pdm_config = {
    // GPIO pin for the PDM DAT signal
    .gpio_data = 2,

    // GPIO pin for the PDM CLK signal
    .gpio_clk = 3,

    // PIO instance to use
    .pio = pio0,

    // PIO State Machine instance to use
    .pio_sm = 0,

    // sample rate in Hz
    .sample_rate = SAMPLE_RATE,

    // number of samples to buffer
    .sample_buffer_size = INPUT_BUFFER_SIZE,
};

string resultKeyLess1 = "";
string resultKeyLess2 = "";
string resultKeyLess3 = "";

q15_t capture_buffer_q15[INPUT_BUFFER_SIZE];
volatile int new_samples_captured = 0;

q15_t input_q15[INPUT_BUFFER_SIZE + (FFT_SIZE / 2)];

DSPPipeline dsp_pipeline(FFT_SIZE);
MLModel ml_model(tflite_model, 128 * 1024);

int8_t* scaled_spectrum = nullptr;
int32_t spectogram_divider;
float spectrogram_zero_point;

// callback functions
void on_analog_samples_ready();
void on_pdm_samples_ready();

int main( void )
{
    // initialize stdio
    stdio_init_all();

    //printf("hello pico fire alarm detection\n");


     // Set up our UART with the required speed.
    uart_init(UART_ID, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    gpio_set_function(PICO_DEFAULT_LED_PIN, GPIO_FUNC_PWM);
    
    uint pwm_slice_num = pwm_gpio_to_slice_num(PICO_DEFAULT_LED_PIN);
    uint pwm_chan_num = pwm_gpio_to_channel(PICO_DEFAULT_LED_PIN);

    // Set period of 256 cycles (0 to 255 inclusive)
    pwm_set_wrap(pwm_slice_num, 256);

    // Set the PWM running
    pwm_set_enabled(pwm_slice_num, true);

    if (!ml_model.init()) {
        printf("Failed to initialize ML model!\n");
        while (1) { tight_loop_contents(); }
    }

    if (!dsp_pipeline.init()) {
        printf("Failed to initialize DSP Pipeline!\n");
        while (1) { tight_loop_contents(); }
    }

    scaled_spectrum = (int8_t*)ml_model.input_data();
    spectogram_divider = 64 * ml_model.input_scale(); 
    spectrogram_zero_point = ml_model.input_zero_point();


	//// initialize and start the analog microphone
	//if (analog_microphone_init(&config) < 0) {
	//    printf("analog microphone initialization failed!\n");
	//    while (1) { tight_loop_contents(); }
	//}

	//analog_microphone_set_samples_ready_handler(on_analog_samples_ready);


	//if (analog_microphone_start() < 0) {
 //       printf("analog microphone start failed!\n");
 //       while (1) { tight_loop_contents(); }
	//}

     //initialize the PDM microphone
    if (pdm_microphone_init(&pdm_config) < 0) {
        printf("PDM microphone initialization failed!\n");
        while (1) { tight_loop_contents(); }
    }

     //set callback that is called when all the samples in the library
     //internal sample buffer are ready for reading
    pdm_microphone_set_samples_ready_handler(on_pdm_samples_ready);

     //start capturing data from the PDM microphone
    if (pdm_microphone_start() < 0) {
        printf("PDM microphone start failed!\n");
        while (1) { tight_loop_contents(); }
    }

    bool detecting = false;
    int detected = 0;
    int notDetected = 0;

    while (1) {
        // wait for new samples
        while (new_samples_captured == 0) {
			//printf("nothing\n");
            tight_loop_contents();
        }
        new_samples_captured = 0;

        dsp_pipeline.shift_spectrogram(scaled_spectrum, SPECTRUM_SHIFT, 124);

        // move input buffer values over by INPUT_BUFFER_SIZE samples
        memmove(input_q15, &input_q15[INPUT_BUFFER_SIZE], (FFT_SIZE / 2));

        // copy new samples to end of the input buffer with a bit shift of INPUT_SHIFT
        arm_shift_q15(capture_buffer_q15, INPUT_SHIFT, input_q15 + (FFT_SIZE / 2), INPUT_BUFFER_SIZE);
    
        for (int i = 0; i < SPECTRUM_SHIFT; i++) {
            dsp_pipeline.calculate_spectrum(
                input_q15 + i * ((FFT_SIZE / 2)),
                scaled_spectrum + (129 * (124 - SPECTRUM_SHIFT + i)),
                spectogram_divider, spectrogram_zero_point
            );
        }

        struct MLModel::mlResult prediction = ml_model.predict();

        float lvalue = 0.0f;
        int largest = 0;
/*
        if (prediction.mlResults[0] > lvalue) {
            largest = 0;
            lvalue = prediction.mlResults[0];
        }
        if (prediction.mlResults[1] > lvalue) {
            largest = 1;
            lvalue = prediction.mlResults[1];
        }
        if (prediction.mlResults[2] > lvalue) {
            largest = 2;
            lvalue = prediction.mlResults[2];
        }
        if (prediction.mlResults[3] > lvalue) {
            largest = 3;
            lvalue = prediction.mlResults[3];
        }
        if (prediction.mlResults[4] > lvalue) {
            largest = 4;
            lvalue = prediction.mlResults[4];
        }
        if (prediction.mlResults[5] > lvalue) {
            largest = 5;
            lvalue = prediction.mlResults[5];
        }
        if (prediction.mlResults[6] > lvalue) {
            largest = 6;
            lvalue = prediction.mlResults[6];
        }
        if (prediction.mlResults[7] > lvalue) {
            largest = 7;
            lvalue = prediction.mlResults[7];
        }
        if (lvalue > 0.7f) {
            printf("\tTOP: \t%d", largest);
            printf("\tVAL: \t%f\n\n", lvalue);
        }*/




        float aThresholdL = 0.1f;
        float aThresholdH = 0.7f;
        float bThresholdL = 0.01f;
        float bThresholdH = 0.9f;
        float cThresholdL = 0.01f;
        float cThresholdH = 0.9f;
        float dThresholdL = 0.01f;
        float dThresholdH = 0.9f;
        float eThresholdL = 0.01f;
        float eThresholdH = 0.9f;
        float fThresholdL = 0.01f;
        float fThresholdH = 0.9f;
        float gThresholdL = 0.01f;
        float gThresholdH = 0.9f;
        float hThresholdL = 0.01f;
        float hThresholdH = 0.9f;
        float iThresholdL = 0.5f;
        float iThresholdH = 0.9f;

        string resultKey = "";

        string aP = "0";
        if (prediction.mlResults[0] < aThresholdL) {
            aP = "0";
        }
        else {
            if (prediction.mlResults[0] > aThresholdH) {
                aP = "A";
            }
            else {
                aP = "a";
            }
        }
        //printf("%c-", aP);
        resultKey = "" + aP + "-";


        string bP = "0";
        if (prediction.mlResults[1] < bThresholdL) {
            bP = "0";
        }
        else {
            if (prediction.mlResults[1] > bThresholdH) {
                bP = "B";
            }
            else {
                bP = "b";
            }
        }
        //printf("%c-", bP);

        resultKey = resultKey + bP + "-";

        string cP = "0";
        if (prediction.mlResults[2] < cThresholdL) {
            cP = "0";
        }
        else {
            if (prediction.mlResults[2] > cThresholdH) {
                cP = "C";
            }
            else {
                cP = "c";
            }
        }
        //printf("%c-", cP);
        resultKey = resultKey + cP + "-";

        string dP = "0";
        if (prediction.mlResults[3] < dThresholdL) {
            dP = "0";
        }
        else {
            if (prediction.mlResults[3] > dThresholdH) {
                dP = "D";
            }
            else {
                dP = "d";
            }
        }
        //printf("%c-", dP);
        resultKey = resultKey + dP + "-";

        string eP = "0";
        if (prediction.mlResults[4] < eThresholdL) {
            eP = "0";
        }
        else {
            if (prediction.mlResults[4] > eThresholdH) {
                eP = "E";
            }
            else {
                eP = "e";
            }
        }
        //printf("%c-", eP);
        resultKey = resultKey + eP + "-";

        string fP = "0";
        if (prediction.mlResults[5] < fThresholdL) {
            fP = "0";
        }
        else {
            if (prediction.mlResults[5] > fThresholdH) {
                fP = "F";
            }
            else {
                fP = "f";
            }
        }
        //printf("%c-", fP);
        resultKey = resultKey + fP + "-";

        string gP = "0";
        if (prediction.mlResults[6] < gThresholdL) {
            gP = "0";
        }
        else {
            if (prediction.mlResults[6] > gThresholdH) {
                gP = "G";
            }
            else {
                gP = "g";
            }
        }
        //printf("%c-", gP);
        resultKey = resultKey + gP + "-";

        string hP = "0";
        if (prediction.mlResults[7] < hThresholdL) {
            hP = "0";
        }
        else {
            if (prediction.mlResults[7] > hThresholdH) {
                hP = "H";
            }
            else {
                hP = "h";
            }
        }
        //printf("%c-", hP);
        resultKey = resultKey + hP + "-";

        string iP = "0";
        if (prediction.mlResults[8] < iThresholdL) {
            iP = "0";
        }
        else {
            if (prediction.mlResults[8] > iThresholdH) {
                iP = "I";
            }
            else {
                iP = "i";
            }
        }
        //printf("%c\t", iP);
        resultKey = resultKey + iP;
        
        if (iP == "0") {
            resultKeyLess3 = resultKeyLess2;
            resultKeyLess2 = resultKeyLess1;
            resultKeyLess1 = resultKey;

            string last3ResultKeys = resultKeyLess3 + "-" + resultKeyLess2 + "-" + resultKeyLess1;
            printf("%s\n", last3ResultKeys.c_str());

            if (last3ResultKeys == "A-0-0-0-0-0-0-0-0-A-0-0-0-0-0-0-0-0-A-0-0-0-0-0-0-0-0") {
                printf("\n===========================\n\nWAKE SOUND %d: DETECTED\n\n===========================\n\n", 1);
            }
        }

        //printf("\ta \t%f", prediction.mlResults[0]);
        //printf("\tb \t%f", prediction.mlResults[1]);
        //printf("\tc \t%f)", prediction.mlResults[2]);
        //printf("\td \t%f", prediction.mlResults[3]);
        //printf("\te \t%f", prediction.mlResults[4]);
        //printf("\tf \t%f)", prediction.mlResults[5]);
        //printf("\tg \t%f", prediction.mlResults[6]);
        //printf("\th \t%f)", prediction.mlResults[7]);
        //printf("\ti \t%f)\n\n\n", prediction.mlResults[8]);
        
        
        //if (prediction >= 0.9) {
        //    
        //    if (!detecting) {
        //        //send value of notDetected and reset
        //        uart_putc(UART_ID, '<');
        //        uart_puts(UART_ID, std::to_string(notDetected).c_str());
        //        uart_putc(UART_ID, '>');
        //        printf("not detected %d \n\n", notDetected);
        //        notDetected = 0;
        //        detecting = true;
        //    } 
        //    detected++;

        //  //printf("\t🔥 🔔\tdetected!\t(prediction = %f)\n\n", prediction);
        //} else {

        //    if (detecting) {
        //        //send value of detected and reset
        //        uart_putc(UART_ID, '<');
        //        uart_puts(UART_ID, std::to_string(detected).c_str());
        //        uart_putc(UART_ID, '>');
        //        printf("detected %d \n\n", detected);
        //        detected = 0;
        //        detecting = false;
        //    }
        //    notDetected++;

        //  //printf("\t🔕\tNOT detected\t(prediction = %f)\n\n", prediction);
        //}
		//printf("TEST\n");
        /*if (detected == 6) {
            uart_putc(UART_ID, '<');
            uart_putc(UART_ID, '0');
            uart_putc(UART_ID, '>');
        }*/
        //pwm_set_chan_level(pwm_slice_num, pwm_chan_num, prediction * 255);
    }

    return 0;
}

void on_pdm_samples_ready()
{
    // callback from library when all the samples in the library
    // internal sample buffer are ready for reading 

    // read in the new samples
    new_samples_captured = pdm_microphone_read(capture_buffer_q15, INPUT_BUFFER_SIZE);
}


void on_analog_samples_ready()
{
	// Callback from library when all the samples in the library
	// internal sample buffer are ready for reading.
	//
	// Read new samples into local buffer.
	new_samples_captured = analog_microphone_read(capture_buffer_q15, INPUT_BUFFER_SIZE);
}
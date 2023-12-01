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

//a variable to start counting when i value of zero first detected
bool counting = false;

// number of readings to count
int SAMPLE_WINDOW = 32;

// a variable to keep track of how many readings we have processed
int sampleCount = 0;

//some variables to count the number of times a category is detected at all over a time period
int aPresent = 0;
int bPresent = 0;
int cPresent = 0;
int dPresent = 0;
int ePresent = 0;
int fPresent = 0;
int gPresent = 0;
int hPresent = 0;
int iPresent = 0;

//some variables to count the number of times a category is strongly detected over a time period
int aHigh = 0;
int bHigh = 0;
int cHigh = 0;
int dHigh = 0;
int eHigh = 0;
int fHigh = 0;
int gHigh = 0;
int hHigh = 0;
int iHigh = 0;

//some variables to count the number of times a category is weakly detected over a time period
int aLow = 0;
int bLow = 0;
int cLow = 0;
int dLow = 0;
int eLow = 0;
int fLow = 0;
int gLow = 0;
int hLow = 0;
int iLow = 0;

//some variables to count the number of times a category is zero over a time period
int aZero = 0;
int bZero = 0;
int cZero = 0;
int dZero = 0;
int eZero = 0;
int fZero = 0;
int gZero = 0;
int hZero = 0;
int iZero = 0;

//some variables to count the number of times a combination of categories is trueover a time period
int wake1Present = 0;
int wake2Present = 0;
int wake3Present = 0;
int wake4Present = 0;
int wake5Present = 0;
int wake6Present = 0;
int wake7Present = 0;
int wake8Present = 0;

string aP = "";
string bP = "";
string cP = "";
string dP = "";
string eP = "";
string fP = "";
string gP = "";
string hP = "";
string iP = "";


struct MLModel::mlResult r8;
struct MLModel::mlResult r7;
struct MLModel::mlResult r6;
struct MLModel::mlResult r5;
struct MLModel::mlResult r4;
struct MLModel::mlResult r3;
struct MLModel::mlResult r2;
struct MLModel::mlResult r1;


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

    //initialise store of the last 8 readings
    for (int i = 0; i < 9; i++) {
        r1.mlResults[i] = 0.0f;
        r2.mlResults[i] = 0.0f;
        r3.mlResults[i] = 0.0f;
        r4.mlResults[i] = 0.0f;
        r5.mlResults[i] = 0.0f;
        r6.mlResults[i] = 0.0f;
        r7.mlResults[i] = 0.0f;
        r8.mlResults[i] = 0.0f;
    }



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


        if (!counting) {
            //reset counter and begin counting
            sampleCount = 0;
            int wake1Present = 0;
            int wake2Present = 0;
            int wake3Present = 0;
            int wake4Present = 0;
            int wake5Present = 0;
            int wake6Present = 0;
            int wake7Present = 0;
            int wake8Present = 0;
            counting = true;
        }


        struct MLModel::mlResult prediction = ml_model.predict();



        for (int i = 0; i < 9; i++) {
            r8.mlResults[i] = r7.mlResults[i];
            r7.mlResults[i] = r6.mlResults[i];
            r6.mlResults[i] = r5.mlResults[i];
            r5.mlResults[i] = r4.mlResults[i];
            r4.mlResults[i] = r3.mlResults[i];
            r3.mlResults[i] = r2.mlResults[i];
            r2.mlResults[i] = r1.mlResults[i];
            r1.mlResults[i] = prediction.mlResults[i];
        }

        if (r1.mlResults[0] < 0.85f &&
            r2.mlResults[0] < 0.85f &&
            r3.mlResults[0] > 0.5f &&
            r4.mlResults[0] > 0.85f &&
            r4.mlResults[8] < 0.1f &&
            r5.mlResults[0] > 0.5f &&
            r6.mlResults[0] < 0.1f &&
            r7.mlResults[0] < 0.1f &&
            r8.mlResults[8] < 0.1f) {


            //printf("\n===========================\n\nWAKE SOUND %d: DETECTED\n\n===========================\n\n", 1);
            wake1Present++;
        }

// probably wakesound 2
        if (r1.mlResults[1] > 0.7f) {

            printf("\n===========================\n\nWAKE SOUND %d: DETECTED\n\n===========================\n\n", 2);
            wake2Present++;

        }

        if (r1.mlResults[0] > 0.1f &&
            r1.mlResults[0] < 0.5f &&
            r1.mlResults[1] < 0.01f &&
            r1.mlResults[2] > 0.45f &&
            r1.mlResults[8] < 0.01f) {
            //printf("\n===========================\n\nWAKE SOUND %d: DETECTED\n\n===========================\n\n", 3);
            wake3Present++;
        }

 /*       if (r1.mlResults[0] < 0.01f &&
            r1.mlResults[1] < 0.01f &&
            r1.mlResults[2] < 0.01f &&
            r1.mlResults[3] > 0.6f &&
            r1.mlResults[4] < 0.01f &&
            r1.mlResults[5] < 0.01f &&
            r1.mlResults[6] < 0.01f &&
            r1.mlResults[7] < 0.01f &&
            r1.mlResults[8] < 0.01f     
            ) {
            printf("\n===========================\n\nWAKE SOUND %d: DETECTED\n\n===========================\n\n", 4);
            wake4Present++;
        }
*/
        if (r1.mlResults[0] > 0.01f &&
            r1.mlResults[1] > 0.01f &&
            r1.mlResults[2] > 0.01f &&
            r1.mlResults[3] > 0.6f &&
            r1.mlResults[4] > 0.01f &&
            r1.mlResults[5] > 0.01f &&
            r1.mlResults[6] > 0.01f &&
            r1.mlResults[7] > 0.01f &&
            r1.mlResults[8] > 0.01f) {
            //printf("\n===========================\n\nWAKE SOUND %d: DETECTED\n\n===========================\n\n", 4);
            wake4Present++;
        }

        if (r1.mlResults[4] > 0.8f &&
            r2.mlResults[4] > 0.8f &&
            r3.mlResults[4] > 0.8f &&
            r4.mlResults[4] > 0.8f &&
            r5.mlResults[4] > 0.8f &&
            r6.mlResults[4] > 0.8f) {

            //printf("\n===========================\n\nWAKE SOUND %d: DETECTED\n\n===========================\n\n", 5);
            wake5Present++;

        }

        if (r1.mlResults[3] > 0.9f &&
            r2.mlResults[3] > 0.9f &&
            r3.mlResults[3] > 0.9f &&
            r4.mlResults[3] > 0.9f &&
            r5.mlResults[3] > 0.9f) {
            //printf("\n===========================\n\nWAKE SOUND %d: DETECTED\n\n===========================\n\n", 6);
            wake6Present++;
        }

        if (false) {
            //printf("\n===========================\n\nWAKE SOUND %d: DETECTED\n\n===========================\n\n", 7);
            wake7Present++;
        }

        if (r1.mlResults[7] > 0.7f) {
            printf("\n===========================\n\nWAKE SOUND %d: DETECTED\n\n===========================\n\n", 8);
            wake8Present++;
        }


        if (sampleCount> SAMPLE_WINDOW) {
            //report detected 
            printf("\n===========================\nWAKE SOUND1:\t%d\n", wake1Present);
            printf("WAKE SOUND2:\t%d\n", wake2Present);
            printf("WAKE SOUND3:\t%d\n", wake3Present);
            printf("WAKE SOUND4:\t%d\n", wake4Present);
            printf("WAKE SOUND5:\t%d\n", wake5Present);
            printf("WAKE SOUND6:\t%d\n", wake6Present);
            printf("WAKE SOUND7:\t%d\n", wake7Present);
            printf("WAKE SOUND8:\t%d\n===========================\n\n", wake8Present);

            //resolve clashes


            //send to nano
            if (wake1Present > 0 &&
                wake2Present == 0 &&
                wake3Present == 0 &&
                wake4Present == 0 &&
                wake5Present == 0 &&
                wake6Present == 0 &&
                wake7Present == 0 &&
                wake8Present == 0
                ) 
            {
                //uart_putc(UART_ID, '<');
                //uart_puts(UART_ID, std::to_string(1).c_str());
                //uart_putc(UART_ID, '>');
            }

            if (wake1Present == 0 &&
                wake2Present > 0 &&
                wake3Present == 0 &&
                wake4Present == 0 &&
                wake5Present == 0 &&
                wake6Present == 0 &&
                wake7Present == 0 &&
                wake8Present == 0
                )
            {
                //uart_putc(UART_ID, '<');
                //uart_puts(UART_ID, std::to_string(2).c_str());
                //uart_putc(UART_ID, '>');
            }

            if (wake1Present == 0 &&
                wake2Present == 0 &&
                wake3Present > 0 &&
                wake4Present == 0 &&
                wake5Present == 0 &&
                wake6Present == 0 &&
                wake7Present == 0 &&
                wake8Present == 0
                )
            {
                //uart_putc(UART_ID, '<');
                //uart_puts(UART_ID, std::to_string(3).c_str());
                //uart_putc(UART_ID, '>');
            }

            if (wake1Present == 0 &&
                wake2Present == 0 &&
                wake3Present == 0 &&
                wake4Present > 0 &&
                wake5Present == 0 &&
                wake6Present == 0 &&
                wake7Present == 0 &&
                wake8Present == 0
                )
            {
                //wake sound 4 is actually linked to 5: Honesty
                //uart_putc(UART_ID, '<');
                //uart_puts(UART_ID, std::to_string(5).c_str());
                //uart_putc(UART_ID, '>');
            }

            if (wake1Present == 0 &&
                wake2Present == 0 &&
                wake3Present == 0 &&
                wake4Present == 0 &&
                wake5Present > 0 &&
                wake6Present == 0 &&
                wake7Present == 0 &&
                wake8Present == 0
                )
            {
                //wake sound 5 is actually linked to 10: Finale
                //uart_putc(UART_ID, '<');
                //uart_puts(UART_ID, std::to_string(10).c_str());
                //uart_putc(UART_ID, '>');
            }

            if (wake1Present == 0 &&
                wake2Present == 0 &&
                wake3Present == 0 &&
                wake4Present == 0 &&
                wake5Present == 0 &&
                wake6Present > 0 &&
                wake7Present == 0 &&
                wake8Present == 0
                )
            {
                //wake sound 6 is actually linked to 7: Kindness
                //uart_putc(UART_ID, '<');
                //uart_puts(UART_ID, std::to_string(7).c_str());
                //uart_putc(UART_ID, '>');
            }

            if (wake1Present == 0 &&
                wake2Present == 0 &&
                wake3Present == 0 &&
                wake4Present == 0 &&
                wake5Present == 0 &&
                wake6Present == 0 &&
                wake7Present > 0 &&
                wake8Present == 0
                )
            {
                //wake sound 7 is actually linked to 6: Integrity
                //uart_putc(UART_ID, '<');
                //uart_puts(UART_ID, std::to_string(6).c_str());
                //uart_putc(UART_ID, '>');
            }

            if (wake1Present == 0 &&
                wake2Present == 0 &&
                wake3Present == 0 &&
                wake4Present == 0 &&
                wake5Present == 0 &&
                wake6Present == 0 &&
                wake7Present == 0 &&
                wake8Present > 0
                )
            {
                //wake sound 8 is actually linkked to 4: Determination
                //uart_putc(UART_ID, '<');
                //uart_puts(UART_ID, std::to_string(4).c_str());
                //uart_putc(UART_ID, '>');
            }


            counting = false;

        }


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
        float aThresholdH = 0.8f;
        float bThresholdL = 0.1f;
        float bThresholdH = 0.8f;
        float cThresholdL = 0.1f;
        float cThresholdH = 0.8f;
        float dThresholdL = 0.1f;
        float dThresholdH = 0.8f;
        float eThresholdL = 0.1f;
        float eThresholdH = 0.8f;
        float fThresholdL = 0.1f;
        float fThresholdH = 0.8f;
        float gThresholdL = 0.1f;
        float gThresholdH = 0.8f;
        float hThresholdL = 0.1f;
        float hThresholdH = 0.8f;
        float iThresholdL = 0.1f;
        float iThresholdH = 0.8f;

        string resultKey = "";

/*
        // if we detect zero background noise then start counting
        if (!counting && prediction.mlResults[8] < 0.001f) {
            counting = true;
        } 

        // if we are counting increment the number of times each category detected over SAMPLE_WINDOW
        if (counting) {
            aP = "0";
            if (prediction.mlResults[0] < 0.001f) {
                aP = "0";
                aZero += 1;
            }
            if (prediction.mlResults[0] > 0.001f) {
                aPresent += 1;
            }
            if (prediction.mlResults[0] < aThresholdL) {
                aP = "0";
                aLow += 1;

            }
            else {
                if (prediction.mlResults[0] > aThresholdH) {
                    aP = "A";
                    aHigh += 1;
                }
                else {
                    aP = "a";
                }
            }

            //printf("%c-", aP);
            aP = to_string((int)(round(prediction.mlResults[0] * 10)));
            aP = string(2 - aP.length(), '0') + aP;
            resultKey = "" + aP + "-";


            bP = "0";
            if (prediction.mlResults[1] < 0.001f) {
                bP = "0";
                bZero += 1;
            }
            if (prediction.mlResults[1] > 0.001f) {
                bPresent += 1;
            }
            if (prediction.mlResults[1] < bThresholdL) {
                bP = "0";
                bLow += 1;
            }
            else {
                if (prediction.mlResults[1] > bThresholdH) {
                    bP = "B";
                    bHigh += 1;
                }
                else {
                    bP = "b";
                }
            }
            //printf("%c-", bP);
            bP = to_string((int)(round(prediction.mlResults[1] * 10)));
            bP = string(2 - bP.length(), '0') + bP;
            resultKey = resultKey + bP + "-";

            cP = "0";
            if (prediction.mlResults[2] < 0.001f) {
                cP = "0";
                cZero += 1;
            }
            if (prediction.mlResults[2] > 0.001f) {
                cPresent += 1;
            }
            if (prediction.mlResults[2] < cThresholdL) {
                cP = "0";
                cLow += 1;
            }
            else {
                if (prediction.mlResults[2] > cThresholdH) {
                    cP = "C";
                    cHigh += 1;
                }
                else {
                    cP = "c";
                }
            }
            //printf("%c-", cP);
            cP = to_string((int)(round(prediction.mlResults[2] * 10)));
            cP = string(2 - cP.length(), '0') + cP;
            resultKey = resultKey + cP + "-";

            dP = "0";
            if (prediction.mlResults[3] < 0.001f) {
                dP = "0";
                dZero += 1;
            }
            if (prediction.mlResults[3] > 0.001f) {
                dPresent += 1;
            }
            if (prediction.mlResults[3] < dThresholdL) {
                dP = "0";
                dLow += 1;
            }
            else {
                if (prediction.mlResults[3] > dThresholdH) {
                    dP = "D";
                    dHigh += 1;
                }
                else {
                    dP = "d";
                }
            }
            //printf("%c-", dP);
            dP = to_string((int)(round(prediction.mlResults[3] * 10)));
            dP = string(2 - dP.length(), '0') + dP;
            resultKey = resultKey + dP + "-";

            eP = "0";
            if (prediction.mlResults[4] < 0.001f) {
                eP = "0";
                eZero += 1;
            }
            if (prediction.mlResults[4] > 0.001f) {
                ePresent += 1;
            }
            if (prediction.mlResults[4] < eThresholdL) {
                eP = "0";
                eLow += 1;
            }
            else {
                if (prediction.mlResults[4] > eThresholdH) {
                    eP = "E";
                    eHigh += 1;
                }
                else {
                    eP = "e";
                }
            }
            //printf("%c-", eP);
            eP = to_string((int)(round(prediction.mlResults[4] * 10)));
            eP = string(2 - eP.length(), '0') + eP;
            resultKey = resultKey + eP + "-";

            fP = "0";
            if (prediction.mlResults[5] < 0.001f) {
                fP = "0";
                fZero += 1;
            }
            if (prediction.mlResults[5] > 0.001f) {
                fPresent += 1;
            }
            if (prediction.mlResults[5] < fThresholdL) {
                fP = "0";
                fLow += 1;
            }
            else {
                if (prediction.mlResults[5] > fThresholdH) {
                    fP = "F";
                    fHigh += 1;
                }
                else {
                    fP = "f";
                }
            }
            //printf("%c-", fP);
            fP = to_string((int)(round(prediction.mlResults[5] * 10)));
            fP = string(2 - fP.length(), '0') + fP;
            resultKey = resultKey + fP + "-";

            gP = "0";
            if (prediction.mlResults[6] < 0.001f) {
                gP = "0";
                gZero += 1;
            }
            if (prediction.mlResults[6] > 0.001f) {
                gPresent += 1;
            }
            if (prediction.mlResults[6] < gThresholdL) {
                gP = "0";
                gLow += 1;
            }
            else {
                if (prediction.mlResults[6] > gThresholdH) {
                    gP = "G";
                    gHigh += 1;
                }
                else {
                    gP = "g";
                }
            }
            //printf("%c-", gP);
            gP = to_string((int)(round(prediction.mlResults[6] * 10)));
            gP = string(2 - gP.length(), '0') + gP;
            resultKey = resultKey + gP + "-";

            hP = "0";
            if (prediction.mlResults[7] < 0.001f) {
                hP = "0";
                hZero += 1;
            }
            if (prediction.mlResults[7] > 0.001f) {
                hPresent += 1;
            }
            if (prediction.mlResults[7] < hThresholdL) {
                hP = "0";
                hLow += 1;
            }
            else {
                if (prediction.mlResults[7] > hThresholdH) {
                    hP = "H";
                    hHigh += 1;
                }
                else {
                    hP = "h";
                }
            }
            //printf("%c-", hP);
            hP = to_string((int)(round(prediction.mlResults[7] * 10)));
            hP = string(2 - hP.length(), '0') + hP;
            resultKey = resultKey + hP + "-";

            iP = "0";
            if (prediction.mlResults[8] < 0.001f) {
                iP = "0";
                iZero += 1;
            }
            if (prediction.mlResults[8] > 0.001f) {
                iPresent += 1;
            }
            if (prediction.mlResults[8] < iThresholdL) {
                iP = "0";
                iLow += 1;
            }
            else {
                if (prediction.mlResults[8] > iThresholdH) {
                    iP = "I";
                    iHigh += 1;
                }
                else {
                    iP = "i";
                }
            }

            //printf("%c\t", iP);
            iP = to_string((int)(round(prediction.mlResults[8] * 10)));
            iP = string(2 - iP.length(), '0') + iP;
            resultKey = resultKey + iP;

            sampleCount++;
            if (sampleCount > SAMPLE_WINDOW) {

                // output counts
                printf("%d\t", aPresent);
                printf("%d\t", bPresent);
                printf("%d\t", cPresent);
                printf("%d\t", dPresent);
                printf("%d\t", ePresent);
                printf("%d\t", fPresent);
                printf("%d\t", gPresent);
                printf("%d\t", hPresent);
                printf("%d\n\n", iPresent);


                //reset variables
                counting = false;
                sampleCount = 0;
                aZero = 0;
                aPresent = 0;
                aLow = 0;
                aHigh = 0;
                bZero = 0;
                bPresent = 0;
                bLow = 0;
                bHigh = 0;
                cZero = 0;
                cPresent = 0;
                cLow = 0;
                cHigh = 0;
                dZero = 0;
                dPresent = 0;
                dLow = 0;
                dHigh = 0;
                eZero = 0;
                ePresent = 0;
                eLow = 0;
                eHigh = 0;
                fZero = 0;
                fPresent = 0;
                fLow = 0;
                fHigh = 0;
                gZero = 0;
                gPresent = 0;
                gLow = 0;
                gHigh = 0;
                hZero = 0;
                hPresent = 0;
                hLow = 0;
                hHigh = 0;
                iZero = 0;
                iPresent = 0;
                iLow = 0;
                iHigh = 0;
            }

        }
 */       
        //if (iP == "0") {
        //    resultKeyLess3 = resultKeyLess2;
        //    resultKeyLess2 = resultKeyLess1;
        //    resultKeyLess1 = resultKey;

        //    string last3ResultKeys = resultKeyLess3 + "-" + resultKeyLess2 + "-" + resultKeyLess1;
        //    printf("%s\n", last3ResultKeys.c_str());

        //    if (last3ResultKeys == "A-0-0-0-0-0-0-0-0-A-0-0-0-0-0-0-0-0-A-0-0-0-0-0-0-0-0") {
        //        printf("\n===========================\n\nWAKE SOUND %d: DETECTED\n\n===========================\n\n", 1);
        //    }
        //}

 /*     printf("\ta \t%f", prediction.mlResults[0]);
        printf("\tb \t%f", prediction.mlResults[1]);
        printf("\tc \t%f\n", prediction.mlResults[2]);
        printf("\td \t%f", prediction.mlResults[3]);
        printf("\te \t%f", prediction.mlResults[4]);
        printf("\tf \t%f\n", prediction.mlResults[5]);
        printf("\tg \t%f", prediction.mlResults[6]);
        printf("\th \t%f", prediction.mlResults[7]);
        printf("\ti \t%f\n\n\n", prediction.mlResults[8]);
  */      
        printf("%f,", prediction.mlResults[0]);
        printf("%f,", prediction.mlResults[1]);
        printf("%f,", prediction.mlResults[2]);
        printf("%f,", prediction.mlResults[3]);
        printf("%f,", prediction.mlResults[4]);
        printf("%f,", prediction.mlResults[5]);
        printf("%f,", prediction.mlResults[6]);
        printf("%f,", prediction.mlResults[7]);
        printf("%f\n", prediction.mlResults[8]);
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
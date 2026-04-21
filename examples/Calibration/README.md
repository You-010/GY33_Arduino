# GY-33 Calibration Helper

This utility generates a custom GY33_Calibration struct to improve accuracy by compensating for sensor noise and light leakage (crosstalk).

## Preparation
1. **Blackout Cover:** A cap or box to block all light.
2. **Matte Samples:** Non-shiny Red, Green, Blue, and White cards.

## Instructions
1. Open Calibration.ino and set GY33_PROTOCOL to 1 (UART) or 0 (I2C). 
2. Ensure S0 jumper is set for the chosen protocol (open/high (UART) ot closed/grounded (I2C))
3. Verify that the pins and Serial/Wire objects in the config section match your wiring.
4. Upload the sketch and open the Serial Monitor at 115200 baud.
5. Follow the prompts to place your samples. Press any key to capture each step.
6. Copy the generated code block from the Serial Monitor into your main project's setup().

## Calibration Details
* **Black Offset:** Removes noise seen in total darkness.
* **Crosstalk:** Subtracts color bleeding (e.g., Red leaking into Green readings).
* **White Balance:** Scales RGB channels so white reads as (255, 255, 255).
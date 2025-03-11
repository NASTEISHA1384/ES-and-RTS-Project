#include <systemc.h>

/**
 * Spec 01
 * This project produces current by mapping the intensity to a
current using a ratio and a constant(line 77 $ 78).
 * The temperature is linearly interpolated.
 * Output temperature is respective to environment.
 */

/*
Example Calculation

Let's say the desired intensity is 600 lux, and the desired temperature is 4500K:

* 4500K is between warm (2700K) and cool (6500K).
* The system calculates white ratio = 0.5, yellow ratio = 0.5.
* Current output:
    * White LED current = 0.5 × 600 lux = 300 lux
    * Yellow LED current = 0.5 × 600 lux = 300 lux
* Total remains at 600 lux, but the color balance shifts based on ratio.

This way the overall intensity will be balanced among two LEDs.
The overall temperature is measured by the balance required among 2 white and yellow LEDs.
*/

const float lux_to_amps = 0.001; // Define a constant for converting LUX into amps.

// Sensor module to measure ambient light conditions
SC_MODULE(SensorModule) {
    sc_in<bool> clk;  // Clock input
    sc_out<float> ambient_lux;  // Ambient light intensity output (lux)
    sc_out<int> ambient_color_temp;  // Ambient color temperature output (K)

    void measure_ambient_conditions() {
        ambient_lux.write(rand() % 1101 + 200);  // Random lux value (200 to 1300)
        ambient_color_temp.write(rand() % 3801 + 2700);  // Random temp value (2700K - 6500K)
    }

    SC_CTOR(SensorModule) {
        SC_METHOD(measure_ambient_conditions);
        sensitive << clk.pos();
    }
};

// Lamp module for controlling light intensity and color balance
SC_MODULE(LampModule) {
    sc_in<bool> clk;
    sc_in<float> target_lux;  // Desired total light intensity
    sc_in<int> target_color_temp;  // Desired color temperature
    sc_out<float> white_current;  // White LED current (normalized 0-1.5)
    sc_out<float> yellow_current;  // Yellow LED current (normalized 0-1.5)

    // Helper function to calculate LED intensity balance
    void adjust_lamps() {
        float total_intensity = target_lux.read();
        int temp_target = target_color_temp.read();

        // Define warm and cool color temperature limits (approximation)
        const int warm_temp = 2700;
        const int cool_temp = 6500;

        // Compute white/yellow ratio
        float white_ratio, yellow_ratio;

        if (temp_target <= warm_temp) {
            // 100% Yellow, 0% White (fully warm)
            white_ratio = 0.0;
            yellow_ratio = 1.0;
        } else if (temp_target >= cool_temp) {
            // 100% White, 0% Yellow (fully cool)
            white_ratio = 1.0;
            yellow_ratio = 0.0;
        } else {
            // Linearly interpolate between warm and cool (Specification 01)
            float factor = float(temp_target - warm_temp) / (cool_temp - warm_temp);
            white_ratio = factor;
            yellow_ratio = 1.0 - factor;
        }

        // Map intensity to current (adjust this based on the LED specs)
        float white_current_value = white_ratio * total_intensity * lux_to_amps;  // Example scaling factor (lux to amps)
        float yellow_current_value = yellow_ratio * total_intensity * lux_to_amps;  // Example scaling factor (lux to amps)

        // Apply the total intensity to each LED proportionally
        white_current.write(white_current_value);
        yellow_current.write(yellow_current_value);
    }

    SC_CTOR(LampModule) {
        SC_METHOD(adjust_lamps);
        sensitive << clk.pos();
    }
};

// Main system module integrating sensor and lamp control
SC_MODULE(ECRL_System) {
    sc_signal<float> ambient_lux_signal;
    sc_signal<int> ambient_color_temp_signal;
    sc_signal<float> white_current_signal;
    sc_signal<float> yellow_current_signal;

    SensorModule sensor;
    LampModule lamp;

    void control_logic() {
        float measured_lux = ambient_lux_signal.read();
        int measured_temp = ambient_color_temp_signal.read();

        // Define acceptable light range
        float min_lux = 200;
        float max_lux = 1300;

        // Compute desired total intensity
        float desired_lux;
        if (measured_lux < min_lux) {
            desired_lux = min_lux;
        } else if (measured_lux > max_lux) {
            desired_lux = max_lux;
        } else {
            desired_lux = measured_lux;
        }

        // Pass target values to the lamp module
        lamp.target_lux.write(desired_lux);
        lamp.target_color_temp.write(measured_temp); // Temperature must be respective to environment
    }

    SC_CTOR(ECRL_System) : sensor("Sensor"), lamp("Lamp") {
        sensor.clk(clk);
        sensor.ambient_lux(ambient_lux_signal); // Sensor module writes to ambient_lux_signal
        sensor.ambient_color_temp(ambient_color_temp_signal); // Sensor moudle writes to ambient_color_temp_signal

        lamp.clk(clk);
        lamp.target_lux(ambient_lux_signal); // Signal writes to the target_lux
        lamp.target_color_temp(ambient_color_temp_signal); // Signal writes to the target_color_temp
        lamp.white_current(white_current_signal); // White_current writes to the white_current_signal
        lamp.yellow_current(yellow_current_signal); // Yellow_current writes to the yellow_current_signal

        SC_METHOD(control_logic);
        sensitive << clk.pos();
    }

private:
    sc_clock clk;
};

// Main function for simulation
int sc_main(int argc, char* argv[]) {
    ECRL_System system("ECRL_System");
    sc_start(10000, SC_MS);
    return 0;
}



#pragma once

// Run only during setup(), after a failed HDMI initialization. Temporarily
// selects transmitter register banks; the next display init resets the chip.
void diagnoseHdmiI2c();

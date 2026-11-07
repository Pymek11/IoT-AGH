#ifndef RECORDER_H
#define RECORDER_H

// Recorder firmware mode (-DMIC_REC): continuous microphone recording with the 801S level logged
// alongside, started and stopped with "START" / "STOP" from external_tools/recorder/rec_capture.py
// -DVIB_DIAG instead prints the 801S level as text, without the microphone (for wiring/polarity)
// main.cpp calls these instead of its own setup() / loop() body

void recorderSetup();
void recorderLoop();

#endif // RECORDER_H

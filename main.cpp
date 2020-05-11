#include "mbed.h"
#include "uLCD_4DGL.h"
#include "DA7212/DA7212.h"

#include "accelerometer_handler.h"
#include "config.h"
#include "magic_wand_model_data.h"

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"

DA7212 audio;
Thread t_audio;

Serial pc(USBTX, USBRX);
uLCD_4DGL uLCD(D1, D0, D2);
InterruptIn sw2(SW2);
InterruptIn sw3(SW3);
Timer TimerSw2;
Timer TimerSw3;

int16_t waveform[kAudioTxBufferSize];


char serialInBuffer[16];
int serialCount = 0;
int si = 0;


int state = 0; //0: menu  1: mode  2: song
int songCount = 1;
int songState = 0;
int modeState = 0;
bool paused = 1;
int lastsongState = 0;


constexpr int notesMemorySize = 64;
char title[8][16] = { "jingal bell","-","-","-","-","-","-","-" };
int notes[8][notesMemorySize] = {{330,330,330,330,330,330,330,
								392,261,294,330,349,349,349,
								349,349,330,330,330,294,294,
								330,294,392,330,330,330,330,
								330,330,330,392,261,294,330,
								349,349,349,349,349,330,330,
								392,392,349,294,261,261,261}};
int dura[8][notesMemorySize] = {{1,1,2,1,1,2,1,
								1,1,1,4,1,1,2,
								1,1,1,1,1,1,1,
								1,2,2,1,1,2,1,
								1,2,1,1,1,1,4,
								1,1,2,1,1,1,1,
								1,1,1,1,2,2,2}};
int tlen[8] = {49};

///
constexpr int kTensorArenaSize = 60 * 1024;
uint8_t tensor_arena[kTensorArenaSize];
bool should_clear_buffer = false;
bool got_data = false;
int gesture_index;
static tflite::MicroErrorReporter micro_error_reporter;
tflite::ErrorReporter *error_reporter;
const tflite::Model *model;
static tflite::MicroOpResolver<6> micro_op_resolver;
tflite::MicroInterpreter *interpreter;
TfLiteTensor *model_input;
int input_length;

///

Thread t_gesture;
Thread t;
Thread t_load;
EventQueue queue(32 * EVENTS_EVENT_SIZE);

int PredictGesture(float *output) {
	// How many times the most recent gesture has been matched in a row
	static int continuous_count = 0;
	// The result of the last prediction
	static int last_predict = -1;

	// Find whichever output has a probability > 0.8 (they sum to 1)
	int this_predict = -1;
	for (int i = 0; i < label_num; i++)
	{
		if (output[i] > 0.8)
			this_predict = i;
	}

	// No gesture was detected above the threshold
	if (this_predict == -1)
	{
		continuous_count = 0;
		last_predict = label_num;
		return label_num;
	}

	if (last_predict == this_predict)
	{
		continuous_count += 1;
	}
	else
	{
		continuous_count = 0;
	}
	last_predict = this_predict;

	// If we haven't yet had enough consecutive matches for this gesture,
	// report a negative result
	if (continuous_count < config.consecutiveInferenceThresholds[this_predict])
	{
		return label_num;
	}
	// Otherwise, we've seen a positive result, so clear all our variables
	// and report it
	continuous_count = 0;
	last_predict = -1;

	return this_predict;
}

void playNote(int freq) {
	for (int i = 0; i < kAudioTxBufferSize; i++)
	{
		if(!paused) waveform[i] = (int16_t)(sin((double)i * 2. * M_PI / (double)(kAudioSampleFrequency / freq)) * ((1 << 14) - 1));
		else waveform[i] = (int16_t)0;
	}

	audio.spk.play(waveform, kAudioTxBufferSize);
}

void display() {
	
	switch (state)
	{
	case 0:
		uLCD.cls();
		uLCD.locate(1, 1);
		uLCD.color(WHITE);
		uLCD.printf("Menu");
		uLCD.locate(1, 3);
		uLCD.color(WHITE);
		uLCD.printf(title[songState]);
		break;
	case 1:
		uLCD.cls();
		uLCD.locate(1, 1);
		uLCD.color(WHITE);
		uLCD.printf("Mode");
		uLCD.color(WHITE);
		uLCD.locate(1, 3);
		if(modeState == 0) uLCD.printf("* ");
		else uLCD.printf("  ");
		uLCD.printf("Forward");
		uLCD.locate(1, 4);
		if (modeState == 1) uLCD.printf("* ");
		else uLCD.printf("  ");
		uLCD.printf("Backward");
		uLCD.locate(1, 5);
		if (modeState == 2) uLCD.printf("* ");
		else uLCD.printf("  ");
		uLCD.printf("Song List");
		break;
	case 2:
		uLCD.cls();
		uLCD.locate(1, 1);
		uLCD.color(WHITE);
		uLCD.printf("Song List");
		uLCD.color(WHITE);

		for (int i = 0; i < 8; i++) {
			uLCD.locate(1, 3+i);
			if(i == songState) uLCD.printf("* ");
			else uLCD.printf("  ");
			uLCD.printf(title[i]);
		}
		break;
	default:
		break;
	}

}


void sw2rise() {
	if(TimerSw2.read_ms() > 1000) {
		TimerSw2.reset();

		state = 1;
		modeState = 0;
		queue.call(display);
		paused = 1;
		
	}
}
void sw3rise(){
		if(sw3) {
		if(TimerSw3.read_ms() > 1000) {
			TimerSw3.reset();
			switch (state)
			{
			case 1:
				switch (modeState)
				{
					case 0:
						if (songState < songCount - 1)
						{
							songState++;
						}
						state = 0;
						queue.call(display);
						paused = 0;
						break;
					case 1:
						if (songState > 0)
						{
							songState--;
						}
						state = 0;
						queue.call(display);
						paused = 0;
						break;
					case 2:
						
						state = 2;
						songState = 0;
						queue.call(display);
						break;											
					default:
						break;
				}
				break;

			case 2:
				state = 0;
				
				queue.call(display);
				paused = 0;
				break;				
			default:
				break;
			}
		}
	}
}
void audioThread() {
	while(true) {
		for (int i = 0; i < tlen[songState]; i++)
		{
			int length = dura[songState][i];
			while (length--)
			{
				while(paused) { wait(0.1); }

				// the loop below will play the note for the duration of 1s
				for (int j = 0; j < kAudioSampleFrequency / kAudioTxBufferSize; ++j)
				{
					queue.call(playNote, notes[songState][i]);
				}
				if (length < 1)
					wait(0.5);

				if(songState != lastsongState) goto changeSong;
				lastsongState = songState;
			}
		}
		changeSong:
		lastsongState = songState;
	}
}

void gestureThread() {
	while(true) {
		got_data = ReadAccelerometer(error_reporter, model_input->data.f, input_length, should_clear_buffer);

		if (!got_data)
		{
			should_clear_buffer = false;
			continue;
		}

		TfLiteStatus invoke_status = interpreter->Invoke();
		if (invoke_status != kTfLiteOk)
		{
			error_reporter->Report("Invoke failed on index: %d\n", begin_index);
			continue;
		}

		gesture_index = PredictGesture(interpreter->output(0)->data.f);

		should_clear_buffer = gesture_index < label_num;

		if (gesture_index < label_num)
		{
			switch (state)
			{
				case 1:
					///
					switch (gesture_index)
					{
						case 0:
							if (modeState > 0)
							{
								modeState--;
								queue.call(display);
							}
							break;
						case 1:
							if (modeState < 2)
							{
								modeState++;
								queue.call(display);
							}
						default:
							break;
					}	
					///			
					break;
				case 2:
					///
					switch (gesture_index)
					{
						case 0:
							if (songState > 0)
							{
								songState--;
								queue.call(display);
							}
							break;
						case 1:
							if (songState < songCount - 1)
							{
								songState++;
								queue.call(display);
							}
							break;				
						default:
							break;
					}
					
					break;	

				default:
					break;
			}
		}
	}
}
void loadsong(){
	while (true) {
		if (pc.readable())
		{

			serialInBuffer[serialCount] = pc.getc();
			serialCount++;

			// Title
			if (serialInBuffer[serialCount - 1] == '@')
			{
				serialInBuffer[serialCount - 1] = '\0';
				for(int i = 0; i < 16; i++) title[songCount][i] = serialInBuffer[i];
				songCount++;
				serialCount = 0;
			}

			// Notes
			if (serialInBuffer[serialCount - 1] == '#')
			{
				serialInBuffer[serialCount - 1] = '\0';
				notes[songCount][si] = (int)atoi(serialInBuffer);
				serialCount = 0;
				si++;
			}

			// Dura
			if (serialInBuffer[serialCount - 1] == '*')
			{
				serialInBuffer[serialCount - 1] = '\0';
				dura[songCount][si] = (int)atoi(serialInBuffer);
				serialCount = 0;
				si++;
			}

			// Length
			if (serialInBuffer[serialCount - 1] == '$')
			{
				serialInBuffer[serialCount - 1] = '\0';
				tlen[songCount] = (int)atoi(serialInBuffer);
				serialCount = 0;
			}

			if (serialInBuffer[serialCount - 1] == '!')
			{
				serialCount = 0;
				si = 0;
			}
		}
	}
}
int main() {
	TimerSw2.start();
	TimerSw3.start();
	queue.call(display);

	t_gesture.start(gestureThread);
	t_load.start(loadsong);
	t_gesture.start(callback(&queue, &EventQueue::dispatch_forever));

	
	t.start(callback(&queue, &EventQueue::dispatch_forever));
	t_audio.start(audioThread);
	
	sw2.rise(&sw2rise);
	sw3.rise(&sw3rise);

	
	
	
	///////////
	error_reporter = &micro_error_reporter;
	model = tflite::GetModel(g_magic_wand_model_data);
	if (model->version() != TFLITE_SCHEMA_VERSION)
	{
		error_reporter->Report(
			"Model provided is schema version %d not equal "
			"to supported version %d.",
			model->version(), TFLITE_SCHEMA_VERSION);
		return -1;
	}
	micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_DEPTHWISE_CONV_2D, tflite::ops::micro::Register_DEPTHWISE_CONV_2D());
	micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_MAX_POOL_2D, tflite::ops::micro::Register_MAX_POOL_2D());
	micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_CONV_2D, tflite::ops::micro::Register_CONV_2D());
	micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED, tflite::ops::micro::Register_FULLY_CONNECTED());
	micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_SOFTMAX, tflite::ops::micro::Register_SOFTMAX());
	micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_RESHAPE, tflite::ops::micro::Register_RESHAPE(), 1);
	static tflite::MicroInterpreter static_interpreter(model, micro_op_resolver, tensor_arena, kTensorArenaSize, error_reporter);
	interpreter = &static_interpreter;
	interpreter->AllocateTensors();
	model_input = interpreter->input(0);
	if ((model_input->dims->size != 4) || (model_input->dims->data[0] != 1) ||
		(model_input->dims->data[1] != config.seq_length) ||
		(model_input->dims->data[2] != kChannelNumber) ||
		(model_input->type != kTfLiteFloat32))
	{
		error_reporter->Report("Bad input tensor parameters in model");
		return -1;
	}
	input_length = model_input->bytes / sizeof(float);
	TfLiteStatus setup_status = SetupAccelerometer(error_reporter);
	if (setup_status != kTfLiteOk) {
		error_reporter->Report("Set up failed\n");
		return -1;
	}
	error_reporter->Report("Set up successful...\n");
	////////////

	


}
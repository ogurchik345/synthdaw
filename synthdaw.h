#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <format>
#include <sstream>
#include <iomanip>
#include <cstdarg>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <ctime>

//pi without math.h
#define M_PI 3.14159265

//tone multiplier
#define tone 1.05946309436

//1st note (C0)
#define first_note 16.35

extern std::string work_dir;

//struct used for arrange notes over time
struct timezone {
    int start;
    int dur;
    int tact;
};

//struct used for chords
struct tick_chord {
    int tick;
    int count;
};

#pragma pack(push, 1)
struct headers {
    char FileTypeBlocID[4] = { 'R', 'I', 'F', 'F' };
    uint32_t FileSize;
    char FileFormatID[4] = {'W', 'A', 'V', 'E'};
    char FormadBlocID[4] = { 'f', 'm', 't', ' ' };
    uint32_t BlocSize = 16;
    uint16_t AudioFormat = 1;
    uint16_t NbrChannels = 2;
    uint32_t Freq = 44100;
    uint32_t BytesPerSec = Freq * NbrChannels * 16 / 8;
    uint16_t BytesPerBloc = NbrChannels * 16 / 8;
    uint16_t BitsPerSample = 16;
    char DataBlocID[4] = { 'd', 'a', 't', 'a' };
    uint32_t DataSize;
};
#pragma pack(pop)


//Structure of text is tempo(3 digits) only at start of string, TCT on start of every tact of 4/4, END at the end of string
//Every note is 3 symbols:
//          1 - A, B, C, D, E, F, G - note
//          2 - 0-9 octave
//          3 - '.' or '+', where '+' is up to 1/2 note (halftone)
//After note is 4 symbols, 2 first is starting tick in tact, 2 last is duration in ticks (start tick + duration < 32)
//Durations: 01-1/32, 02-1/16, 04-1/8, 08-1/4, 16-1/2, 32-1/1, duration*1.5 for notes with point
//Example: "120TCTC4.0008E4.0008G4.0008END" (C chord). Attention!!! Every tact has only 32 ticks!!!
bool static note_interpreter(std::string input, size_t* pos_next, int* tempo, std::vector<double>* freqs, std::vector<timezone>* durs, int* tacts, int track, int* instrument);

//system to get how many notes in tick
void static get_chords(std::vector<timezone> durs, std::vector<tick_chord>* chords, int tacts);

//Just sine wave y=a*sin(bx)
std::vector<double> static sine_wave(double freq, int time);

//Random white noise
std::vector<double> static noise_32767(double freq, int time);

//sawtooth wave like /|/|/|
std::vector<double> static sawtooth_wave(double freq, int time);

//kick C0, clap C0+, hihat D0, snare D0+ 
std::vector<double> static drum_kit(double freq, int time);

//uchar to hex_string for file test
std::string static uch_to_hex(unsigned char i);

//read sample in wav without headers
void static read_data(std::string filename, std::vector<double>& total);

//integer to reversed uint8_t for headers
void static int_ru8(unsigned short len, int number, std::uint8_t** result);

// wave like /\/\/\/\/\/\/
std::vector<double> static triangle_wave(double freq, int time);

//wave like |_|
std::vector<double> static square_wave(double freq, int time);

//main function to generate file
void create_sound(std::string filename, std::string input, bool memory, std::vector<char>& memoryfile, std::string& progress);
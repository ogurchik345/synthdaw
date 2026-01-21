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

//WORD(2 bytes) to string
#define from_word(word) reinterpret_cast<const char*>(word), 2

//DWORD(4 bytes) to string
#define from_dword(dword) reinterpret_cast<const char*>(dword), 4

//1st note (C0)
#define first_note 16.35


//struct used for arrange notes over time
struct timezone {
    int start;
    int dur;
    int tact;
    int nn;
};

//struct used for chords
struct tick_chord {
    int chord;
    int count;
};


//Structure of text is tempo(3 digits) only at start of string, TCT on start of every tact of 4/4, END at the end of string
//Every note is 3 symbols:
//          1 - A, B, C, D, E, F, G - note
//          2 - 0-9 octave
//          3 - '.' or '+', where '+' is up to 1/2 note (halftone)
//After note is 4 symbols, 2 first is starting tick in tact, 2 last is duration in ticks (start tick + duration < 32)
//Chords is structure "!XX" where XX is number of notes in chord (every note should have similar time structure)
//Durations: 01-1/32, 02-1/16, 04-1/8, 08-1/4, 16-1/2, 32-1/1, duration*1.5 for notes with point
//Example: "120TCT!03C4.0008E4.0008G4.0008END" (C chord). Attention!!! Every tact has only 32 ticks!!!
void static note_interpreter(std::string input, int* tempo, std::vector<double>* freqs, std::vector<timezone>* durs, int* tacts, std::vector<tick_chord>* chords);

//Just sine wave y=a*sin(bx)
std::vector<double> static sine_wave(double freq, int time);

//Random white noise
std::vector<double> static noise_32767(double freq, int time);

//sawtooth wave like /|/|/|
std::vector<double> static sawtooth_wave(double freq, int time);

//uchar to hex_string for file test
std::string static uch_to_hex(unsigned char i);

//integer to reversed uint8_t for headers
void static int_ru8(unsigned short len, int number, std::uint8_t** result);

//creating WAV headers
std::string static createHeaders(int time);

//main function to generate file
void create_sound(std::string filename, std::string input, int instrument);
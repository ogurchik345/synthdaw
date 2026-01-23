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
#include "synthdaw.h"

#define M_PI 3.14159265
#define tone 1.05946309436
#define from_word(word) reinterpret_cast<const char*>(word), 2
#define from_dword(dword) reinterpret_cast<const char*>(dword), 4
#define first_note 16.35

void static note_interpreter(std::string input, int* tempo, std::vector<double>* freqs, std::vector<timezone>* durs, int* tacts) {
    int tact = -1;
    std::cout << input << std::endl;
    size_t pos;
    *tempo = std::stoi(input, &pos);
    std::cout << "tempo is:" << *tempo << std::endl;
    while (1) {
        int note = 0;
        std::string new_note = input.substr(pos, 3);
        if (new_note == "END")
            break;
        if (new_note == "TCT") {
            tact++;
            pos += 3;
            continue;
        }
        switch (new_note[0]) {
        case 'C': note += 0; break;
        case 'D': note += 2; break;
        case 'E': note += 4; break;
        case 'F': note += 5; break;
        case 'G': note += 7; break;
        case 'A': note += 9; break;
        case 'B': note += 11; break;
        }
        if (new_note[2] == '+') note++;
        note += 12 * (new_note[1] - '0');
        double freq = first_note * (pow(tone, note));
        if (new_note == "PSE") {
            freq = 0;
        }
        pos += 3;
        std::string new_time = input.substr(pos, 4);
        timezone duration = { stoi(new_time.substr(0,2), nullptr) + (32 * tact), stoi(new_time.substr(2,2), nullptr), tact};
        pos += 4;
        std::cout << "NOTE: " << new_note << " STARTTIME/DURATION: " << duration.start << "/" << duration.dur << " FREQUENCY: " << freq << " TACT:" << tact << std::endl;
        freqs->push_back(freq);
        durs->push_back(duration);
    }
    *tacts = tact + 1;
}

void static get_chords(std::vector<timezone> durs, std::vector<tick_chord>* chords, int tacts) {
    int* counts = (int*)malloc(sizeof(int) * tacts * 32);
    memset(counts, 0, sizeof(int) * tacts * 32);
    for (int i = 0; i < durs.size(); i++) {
        for (int j = durs[i].start; j < durs[i].start + durs[i].dur; j++) {
            counts[j]++;
        }
    }
    for (int i = 0; i < tacts; i++) {
        for (int j = 0; j < 32; j++) {
            chords->push_back({ i * 32 + j, counts[i*32+j]});
        }
    }
    free(counts);
}

std::vector<double> static sine_wave(double freq, int time) {
    std::vector<double> total;
    int max = (int)(44100 * ((double)time / 1000));
    for (double i = 0; i < max; i += 1) {
        //volume = 32767 * (realvolume from 0 to 1)
        double value = 32767. * ((freq == 0 ? 0. : 1.) * (i < 1000 ? (double)i / 1000. : 1.) * (i > max - 1000 ? (double)(max - i) / 1000. : 1.)) * sin(2. * M_PI * freq * (i / 44100));
        total.push_back(value);
    }
    return total;
}

std::vector<double> static noise_32767(double freq, int time) {
    std::vector<double> total;
    int max = (int)(44100 * ((double)time / 1000));
    for (double i = 0; i < max; i += 1) {
        double random_min_to_max = -1.0 + (1.0 - -1.0) * ((double)rand() / RAND_MAX);
        //volume = 32767 * (realvolume from 0 to 1)
        double value = 32767. * ((freq == 0 ? 0. : 1.) * (i < 1000 ? (double)i / 1000. : 1.) * (i > max - 1000 ? (double)(max - i) / 1000. : 1.)) * random_min_to_max;
        total.push_back(value);
    }
    return total;
}

std::vector<double> static sawtooth_wave(double freq, int time) {
    std::vector<double> total;
    int max = (int)(44100 * ((double)time / 1000));
    double pred_value = 0., phase = -1., increment = freq / 44100;
    for (double i = 0; i < max; i += 1) {
        //volume = 32767 * (realvolume from 0 to 1)
        double value = 32767. * (((freq == 0 ? 0. : 1.) * (i < 1000 ? (double)i / 1000. : 1.) * (i > max - 1000 ? (double)(max - i) / 1000. : 1.)) * phase);
        phase += increment;
        if (phase >= 1.0f) phase -= 2.0f;
        pred_value = value;
        total.push_back(value);
    }
    return total;
}

std::string static uch_to_hex(unsigned char i) {
    std::stringstream ss;
    ss << "0x" << std::setfill('0') << std::setw(2) << std::hex << (int)i;
    return ss.str();
}

void static int_ru8(unsigned short len, int number, std::uint8_t** result) {
    for (int i = 0; i < len; i++) {
        result[0][i] = static_cast<std::uint8_t>(number & 0xFF);
        number >>= 8;
    }
}

std::string static createHeaders(int time) {
    std::uint8_t* dword = (uint8_t*)malloc(4 * sizeof(uint8_t));
    std::uint8_t* word = (uint8_t*)malloc(4 * sizeof(uint8_t));


    std::string FileTypeBlocID = "RIFF";

    int_ru8(4, (int)(44100 * 4 * ((double)time / 1000)) + 36, &dword);
    std::string FileSize(from_dword(dword));

    std::string FileFormatID = "WAVE";
    std::string FormatBlocId = "fmt ";

    int_ru8(4, 16, &dword);
    std::string BlocSize(from_dword(dword));

    int_ru8(2, 1, &word);
    std::string AudioFormat(from_word(word));

    int_ru8(1, 2, &word);
    std::string NbrChannels(from_word(word));

    int_ru8(4, 44100, &dword);
    std::string Freq(from_dword(dword));

    int_ru8(4, 44100 * 4, &dword);
    std::string BytesPerSec(from_dword(dword));

    int_ru8(2, 2, &word);
    std::string BytesPerBloc(from_word(word));

    int_ru8(2, 16, &word);
    std::string BitsPerSample(from_word(word));

    std::string DataBlocID = "data";

    int_ru8(4, (int)(44100 * 2 * ((double)time / 1000)), &dword);
    std::string DataSize(from_dword(dword));

    free(word);
    free(dword);
    return FileTypeBlocID + FileSize + FileFormatID + FormatBlocId + BlocSize + AudioFormat + NbrChannels + Freq + BytesPerSec + BytesPerBloc + BitsPerSample + DataBlocID + DataSize;
}

void create_sound(std::string filename, std::string input, int instrument) {
    int tempo;
    int tacts;
    int total_time = 0;
    std::vector<double> freqs;
    std::vector<timezone> durs;
    std::vector<tick_chord> chords;
    note_interpreter(input, &tempo, &freqs, &durs, &tacts);
    get_chords(durs, &chords, tacts);
    total_time = (int)(480000. / (double)tempo) * (tacts);
    double tempo_in_ticks = 480000. / (double)tempo / 32.;

    size_t notes_count = freqs.size();
    std::vector<char> data;
    int bytesPerTact = (int)((44100.) * (480. / (double)tempo));
    std::vector<short> ait_total(bytesPerTact, 0);
    std::vector<double> ait(bytesPerTact, 0);
    for (int t = 0; t < tacts; t++) {
        std::fill(ait.begin(), ait.end(), 0);
        for (int i = 0; i < notes_count; i++) {
            if (durs[i].tact != t) {
                continue;
            }
            int ptr = (bytesPerTact / 32) * (durs[i].start % 32);
            std::vector<double> note;
            switch (instrument) {
            case 0:
                note = sine_wave(freqs[i], tempo_in_ticks * durs[i].dur);
                break;
            case 1:
                note = sawtooth_wave(freqs[i], tempo_in_ticks * durs[i].dur);
                break;
            case 2:
                note = noise_32767(freqs[i], tempo_in_ticks * durs[i].dur);
                break;
            }
            for (int k = 0; k < note.size(); k++) {
                if (ptr + k < ait.size()) {
                    double sum = ait[ptr + k] + note[k];
                    ait[ptr + k] = sum;
                }
            }
            int last_ptr = ptr;
        }
        int tick = ait.size() / 32;
        for (int i = 0; i < 32; i++) {
            int demultiplier = 1;
            int current_tick = i + (32 * t);
            demultiplier = chords[current_tick].count;
            if (demultiplier <= 1)
                demultiplier = 1;
            for (int j = i * tick; j < (i + 1) * tick; j++) {
                ait_total[j] = static_cast<short>(ait[j] / (double)demultiplier);
            }
        }
        const char* pBegin = reinterpret_cast<const char*>(ait_total.data());
        data.insert(data.end(), pBegin, pBegin + (ait_total.size() * sizeof(short)));
    }
    std::string headers = createHeaders(total_time);
    FILE* f;
    fopen_s(&f, (filename + ".wav").c_str(), "wb");
    if (f != 0) {
        fwrite(headers.data(), 1, headers.length(), f);
        fwrite(data.data(), 1, data.size(), f);
        fclose(f);
    }
    else
        std::cout << "Failed to open file!" << std::endl;
}
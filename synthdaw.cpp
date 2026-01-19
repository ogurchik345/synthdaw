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
#define M_PI 3.14159265
#define tone 1.05946309436
#define from_word(word) reinterpret_cast<const char*>(word), 2
#define from_dword(dword) reinterpret_cast<const char*>(dword), 4
#define first_note 16.35

struct timezone {
    int start;
    int dur;
    int tact;
    int nn;
};

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
void static note_interpreter(std::string input, int* tempo, std::vector<double>* freqs, std::vector<timezone>* durs, int* tacts, std::vector<tick_chord>* chords) {
    int tact = -1;
    std::cout << input << std::endl;
    size_t pos;
    int chord = 0;
    int num_notes = 0;
    int counter = 0;
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
        (counter == 0) ? num_notes = 0 : num_notes = chord;
        if (new_note[0] == '!') {
            counter = stoi(new_note.substr(1, 2), nullptr);
            std::cout << " ACCORD/COUNT:" << chord + 1 << "/" << counter << std::endl;
            chords->push_back({ chord + 1, counter });
            chord += 1;
            pos += 3;
            continue;
        }
        else {
            (counter > 0) ? counter -= 1 : counter = 0;
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
        timezone duration = { stoi(new_time.substr(0,2), nullptr) + (32 * tact), stoi(new_time.substr(2,2), nullptr), tact, num_notes };
        pos += 4;
        std::cout << "NOTE: " << new_note << " STARTTIME/DURATION: " << duration.start << "/" << duration.dur << " FREQUENCY: " << freq << " TACT:" << tact << " CHORD:" << num_notes << std::endl;
        freqs->push_back(freq);
        durs->push_back(duration);
    }
    *tacts = tact + 1;
}


//Just sine wave y=a*sin(bx)
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

//Random white noise
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

//sawtooth wave like /|/|/|
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

//uchar to hex_string for file test
std::string static uch_to_hex(unsigned char i) {
    std::stringstream ss;
    ss << "0x" << std::setfill('0') << std::setw(2) << std::hex << (int)i;
    return ss.str();
}

//integer to reversed uint8_t for headers
void static int_ru8(unsigned short len, int number, std::uint8_t** result) {
    for (int i = 0; i < len; i++) {
        result[0][i] = static_cast<std::uint8_t>(number & 0xFF);
        number >>= 8;
    }
}
//creating WAV headers
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

void static create_sound(std::string filename, std::string input, int instrument) {
    std::string rickroll = "114TCT!04A1+0004A3+0004D4.0004F4.0004!05A1+0408A2+0408A3+0408D4.0408F4.0408!04C2.1208C4.1208E4.1208G4.1208!05C2.2004C3.2004C4.2004E4.2004G4.2004!02C2.2404C4.2404!03C2.2804C3.2804C4.2804TCT!04A1.0004C4.0004E4.0004G4.0004!05A1.0408A2.0408C4.0408E4.0408F4.0408!04A1+1208D4.1208F4.1208A4.1208!05A1+2004A2+2004D4.2004F4.2004A4.2004!02A1+2402C5.2402!02A1+2602A4+2602!03A1+2804A2+2804A4.2804TCT!04A1+0004A3+0004D4.0004F4.0004!05A1+0408A2+0408A3+0408D4.0408F4.0408!04C2.1208C4.1208E4.1208G4.1208!05C2.2004C3.2004C4.2004E4.2004G4.2004!02C2.2404C4.2404!03C2.2804C3.2804C4.2804TCTA1+0004!02A1+0408A2+0408C2.1208!03C2.2002C3.2002C4.2002C4.2202!02C2.2402D4.2402E4.2602!02C2.2802E4.2802F4.3002END";
    int tempo;
    int tacts;
    int total_time = 0;
    std::vector<double> freqs;
    std::vector<timezone> durs;
    std::vector<tick_chord> chords;
    note_interpreter(input, &tempo, &freqs, &durs, &tacts, &chords);
    total_time = (int)(480000. / (double)tempo) * (tacts);
    double tempo_in_ticks = 480000. / (double)tempo / 32.;
    int chord_notes = 0;

    size_t notes_count = freqs.size();
    std::vector<char> data;
    int bytesPerTact = (int)((44100.) * (480. / (double)tempo));
    std::vector<short> ait_total(bytesPerTact, 0);
    std::vector<double> ait(bytesPerTact, 0);
    for (int t = 0; t < tacts + 1; t++) {
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
            int chord = 0;
            int demultiplier = 1;
            int current_tick = i + (32 * t);
            for (int j = 0; j < durs.size(); j++) {
                if (current_tick >= durs[j].start && current_tick < durs[j].dur + durs[j].start) {
                    chord = durs[j].nn;
                    break;
                }
            }
            for (int j = 0; j < chords.size(); j++) {
                if (chords[j].chord == chord) {
                    demultiplier = chords[j].count;
                    break;
                }
            }
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



int main(void)
{
    std::string test = "116TCTD4+0006D4+0602D3+0802D3+1202B2.1606B2.2202D2+2408TCTD1+0006D2+0601D2+0802D2+1202G2+1616TCT!02D2+0001C5+0001D5+0101E5.0202!02D2+0402D5+0402C5+0602!02D3+0802D5+0802!02D3+1202C5+1202B4.1402!02G2+1604A4+1604!02G2+2002G4+2002G4.2202!02D3+2404G4+2404!02D3+2802D4+2802E4.3002TCT!02D2+0002D4+0002E4.0202!02D2+0402D4+0402E4.0602!02D3+0802D4+0802G4.1002!02D3+1202B4.1202A4+1402!02G2+1604G4+1604!02G2+2002G4+2002A4+2202!02D3+2404B4.2404!02D3+2802A4+2802B4.3002TCT!02D2+0001B4.0001C5.0101C5+0202!02D2+0402B4.0402A4+0602!02D3+0802G4.0802!02D3+1202G4+1202A4+1402!02G2+1604B4.1604!02G2+2002A4+2002G4+2202!02D3+2404D4+2404!02D3+2802D4+2802E4.3002TCT!02D2+0002D4+0002E4.0202!02D2+0402D4+0402E4.0602!02D3+0802D4+0802B4.1002!02D3+1202A4+1202G4.1402!02G2+1604G4+1604!02G2+2002G4+2002G4.2202!02D3+2404G4+2404!02D3+2801F5+2801G5.2901G5+3002END";
    int instrument = 0;
    int function = 0;
    std::string text = "120TCTEND";
    std::string filename = "0";
    srand((unsigned)time(NULL));
    while (1) {
        std::cout << "WELCOME TO SynthDAW 0.0.1!\nChoose instruction from the list\n1.Choose Instrument\n2.Create Sound\n3.Create Test Sound(test.wav)\n4.Exit\n5.Open GUI (WIP)" << std::endl;
        std::cin >> function;
        switch (function) {
        case 1:
            std::cout << "Choose Instrument\n0-sine(default)\n1-saw\n2-white noise" << std::endl;
            std::cin >> instrument;
            break;
        case 2:
            std::cout << "Create Sound" << std::endl;
            std::cout << "type text of your sound" << std::endl;
            std::cin >> text;
            std::cout << "type filename (without .wav)" << std::endl;
            std::cin >> filename;
            create_sound(filename, text, instrument);
            break;
        case 3:
            std::cout << "Creating test sound" << std::endl;
            create_sound("test", test, instrument);
            break;
        case 4:
            std::cout << "Exiting!" << std::endl;
            return 0;
        default:
            break;
        }
    }

    return 0;
}
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
#include <windows.h>
#include <filesystem>
#include "synthdaw.h"

#define M_PI 3.14159265
#define tone 1.05946309436
#define first_note 16.35

bool static note_interpreter(std::string input, size_t* pos_next, int* tempo, std::vector<double>* freqs, std::vector<timezone>* durs, int* tacts, int track, int* instrument) {
    std::cout << "TRACK:" << track << std::endl;
    int tact = -1;
    std::cout << input << std::endl;
    size_t pos = 0;
    if (track == 0) {
        std::string tempotext = input.substr(0, 3);
        *tempo = std::stoi(tempotext);
        pos += 3;
        std::cout << "tempo is:" << *tempo << std::endl;
    }
    else {
        pos = *pos_next;
    }
    std::string instrutext = input.substr(pos, 3);
    std::cout << instrutext << std::endl;
    if (instrutext == "EOF") {
        return false;
    }
    *instrument = stoi(instrutext);
    pos += 3;
    while (1) {
        int note = 0;
        std::string new_note = input.substr(pos, 3);
        if (new_note == "END") {
            pos += 3;
            break;
        }
        else if (new_note == "TCT") {
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
        std::cout << "NOTE: " << new_note << " STARTTIME/DURATION: " << duration.start << "/" << duration.dur << " FREQUENCY: " << freq << " TACT:" << tact <<  " TRACK:" << track << " INST:" << *instrument <<std::endl;
        freqs->push_back(freq);
        durs->push_back(duration);
    }
    *tacts = tact + 1;
    *pos_next = pos;
    return true;
}

void static get_chords(std::vector<timezone> durs, std::vector<tick_chord>* chords, int tacts) {
    int* counts = (int*)malloc(sizeof(int) * tacts * 32);
    if(counts != 0)
        memset(counts, 0, sizeof(int) * tacts * 32);
    if (counts) {
        for (int i = 0; i < durs.size(); i++) {
            for (int j = durs[i].start; j < durs[i].start + durs[i].dur; j++) {
                counts[j]++;
            }
        }
        for (int i = 0; i < tacts; i++) {
            for (int j = 0; j < 32; j++) {
                chords->push_back({ i * 32 + j, counts[i * 32 + j] });
            }
        }
    }
    if (counts != nullptr)
        free(counts);
    counts = nullptr;
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

std::vector<double> static square_wave(double freq, int time) {
    std::vector<double> total;
    int max = (int)(44100 * ((double)time / 1000));
    for (double i = 0; i < max; i += 1) {
        double value = 32767. * ((freq == 0 ? 0. : 1.) * (i < 1000 ? (double)i / 1000. : 1.) * (i > max - 1000 ? (double)(max - i) / 1000. : 1.)) * (sin(2. * M_PI * freq * (i / 44100)) >= 0 ? 1 : -1);
        total.push_back(value);
    }
    return total;
}

std::vector<double> static triangle_wave(double freq, int time) {
    std::vector<double> total;
    int max = (int)(44100 * ((double)time / 1000));
    for (double i = 0; i < max; i += 1) {
        //0.85 for right scale
        double value = 32767. * ((freq == 0 ? 0. : 0.85) * (i < 1000 ? (double)i / 1000.  * 0.85 : 0.85) * (i > max - 1000 ? (double)(max - i) / 1000. * 0.85: 0.85)) * asin(sin(2. * M_PI * freq * (i / 44100)));
        total.push_back(value);
    }
    return total;
}

void static read_data(std::string filename, std::vector<double>& total) {
    short value = 0;
    std::ifstream file(filename, std::ios::binary);
    if (file.is_open()) {

        //size of file
        file.seekg(0, std::ios::end);
        long size = file.tellg();

        file.seekg(0, std::ios::beg);
        std::string out = "SIZE" + std::to_string(size);
        //cycle in file
        for (long i = 0; i < size; i += 2) {
            file.read(reinterpret_cast<char*>(&value), sizeof(short));
            total.push_back(static_cast<double>(value));
            file.seekg(i, std::ios::beg);
        }
        file.close();
    }
    else {
        
    }
}

std::vector<double> static drum_kit(double freq, int time) {
    //fix working directory
    std::filesystem::current_path(work_dir);

    std::vector<double> total;
    int max = (int)(44100 * ((double)time / 1000));
    for (double i = 0; i < max; i += 1) {
        total.push_back(0);
    }
    //kick
    if (freq < 17.) {
        read_data("sounds/kick.dat", total);
    }
    //clap
    else if (freq > 17. && freq < 18.) {
        read_data("sounds/clap.dat", total);
    }
    //hihat
    else if (freq > 18. && freq < 19.) {
        read_data("sounds/hihat.dat", total);
    }
    //snare
    else if (freq > 19. && freq < 20.) {
        read_data("sounds/snare.dat", total);
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

void create_sound(std::string filename, std::string input, bool memory, std::vector<char>& memoryfile, std::string& progress) {
    size_t pn;
    std::vector<std::vector<double>> full_track;
    int tempo;
    int tacts;
    int track;
    int instrument;
    int total_time = 0;

    std::vector<char> data;
    
    bool run = 1;
    int max_tacts = 0;
    int cur_track = 0;
    int bytesPerTact;

    while (run) {
        std::vector<double> freqs;
        std::vector<timezone> durs;
        std::vector<tick_chord> chords;
        run = note_interpreter(input, &pn, &tempo, &freqs, &durs, &tacts, cur_track, &instrument);
        if (!run) {
            break;
        }


        std::cout << "ni true" << std::endl;
        size_t notes_count = freqs.size();
        total_time = (int)(480000. / (double)tempo) * (tacts);
        bytesPerTact = (int)((44100.) * (480. / (double)tempo));
        std::vector<short> ait_total(bytesPerTact, 0);
        std::vector<double> ait(bytesPerTact, 0);
        double tempo_in_ticks = 480000. / (double)tempo / 32.;
        get_chords(durs, &chords, tacts);
        std::cout << "gc true" << std::endl;

        for (int t = 0; t < tacts; t++) {
            std::fill(ait.begin(), ait.end(), 0);
            for (int i = 0; i < notes_count; i++) {
                progress = "Note generating:" + std::to_string(i) + "/" + std::to_string(notes_count) + "in tact:" + std::to_string(t) + "/" + std::to_string(tacts);
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
                case 3:
                    note = drum_kit(freqs[i], tempo_in_ticks * durs[i].dur);
                    break;
                case 4:
                    note = square_wave(freqs[i], tempo_in_ticks * durs[i].dur);
                    break;
                case 5:
                    note = triangle_wave(freqs[i], tempo_in_ticks * durs[i].dur);
                    break;
                }
                std::cout << "note true" << std::endl;
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
                progress = "Note divising:" + std::to_string(i) + "/" + std::to_string(notes_count) + "in tact:" + std::to_string(t) + "/" + std::to_string(tacts);
                int divisor = 1;
                int current_tick = i + (32 * t);
                divisor = chords[current_tick].count;
                if (divisor <= 1)
                    divisor = 1;
                for (int j = i * tick; j < (i + 1) * tick; j++) {
                    ait[j] = ait[j] / (double)divisor;
                }
            }
            std::cout << "div true" << std::endl;

            if (cur_track >= full_track.size()) {
                full_track.resize(cur_track + 1);
            }
            full_track[cur_track].insert(full_track[cur_track].end(), ait.begin(), ait.end());
            if(tacts > max_tacts)
                max_tacts = tacts;
            std::cout << "insert true" << std::endl;
        }
        cur_track++;
    }
    std::cout << "end of while" << std::endl;
    std::vector<short> full_sum(bytesPerTact*max_tacts, 0);
    for (int i = 0; i < bytesPerTact * max_tacts; i++) {
        int divisor = 1;
        double sum = 0;
        for (int j = 0; j < full_track.size(); j++) {
            if (i <= full_track[j].size()) {
                sum += full_track[j][i];
                if(full_track[j][i] != 0. && full_track[j][i-1] != 0. && j != 0)
                    divisor++;;
            }
            else {
                sum += 0;
            }
        }
        full_sum[i] = static_cast<short>((sum) / (divisor <= 1 ? 1 : divisor));
    }
    std::cout << "summed" << std::endl;
    const char* pBegin = reinterpret_cast<const char*>(full_sum.data());
    data.insert(data.end(), pBegin, pBegin + (full_sum.size() * sizeof(short)));

    progress = "Creating headers";
    headers header;
    header.FileSize = (header.Freq * 4 * ((double)total_time / 1000)) + 36;
    header.DataSize = (header.Freq * 4 * ((double)total_time / 1000)) + 36;
    std::vector<char>header_buf;
    header_buf.resize(sizeof(headers));
    memcpy(header_buf.data(), &header, sizeof(headers));
    if (!memory) {
        progress = "Saving into file";
        FILE* f;
        fopen_s(&f, (filename).c_str(), "wb");
        if (f != 0) {
            fwrite(header_buf.data(), 1, header_buf.size(), f);
            fwrite(data.data(), 1, data.size(), f);
            fclose(f);
        }
        else
            //old cout
            progress = "Failed to open file!";

        progress = "Succecfully saved in " + filename + ".wav"; //python ahh strings
    }
    else {
        progress = "Saving into memory";
        memoryfile.insert(memoryfile.end(), header_buf.begin(), header_buf.end());
        memoryfile.insert(memoryfile.end(), data.begin(), data.end());

        progress = "Succecfully saved in RAM";
    }

    std::cout << "filed" << std::endl;
}
#ifndef WAV_READER_H
#define WAV_READER_H

#include <stdint.h>

typedef struct {
    char chunkID[4];
    uint32_t chunkSize;
    char format[4];
} WAV_Header;

typedef struct {
    char subchunk1ID[4];
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
} WAV_Fmt;

typedef struct {
    char subchunk2ID[4];
    uint32_t subchunk2Size;
} WAV_Data;

int read_wav_file(const char* filename, WAV_Header* header, WAV_Fmt* fmt, WAV_Data* data);
int mix_wav_files(const char* output_file, const char** input_files, float* volumes, int num_files);

#endif
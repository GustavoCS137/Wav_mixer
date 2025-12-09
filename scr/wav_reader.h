#ifndef WAV_READER_H
#define WAV_READER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t sample_rate;
    uint16_t num_channels;
    uint16_t bits_per_sample;
    uint32_t data_size;
    uint32_t duration_samples;
} WAV_Info;

int mix_wav_files(const char* output_file, const char* input_files[], float volumes[], float pans[], int file_count);
int get_wav_info(const char* filename, WAV_Info* info);
int read_wav_samples(const char* filename, int16_t** samples, size_t* sample_count, int max_samples);

#ifdef __cplusplus
}
#endif

#endif

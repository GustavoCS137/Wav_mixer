#ifndef WAV_READER_H
#define WAV_READER_H

#ifdef __cplusplus
extern "C" {
#endif

int mix_wav_files(const char* output_file, const char* input_files[], float volumes[], int file_count);

#ifdef __cplusplus
}
#endif

#endif

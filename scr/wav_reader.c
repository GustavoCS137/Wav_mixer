#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "wav_reader.h"

#pragma pack(push, 1)
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
#pragma pack(pop)

int mix_wav_files(const char* output_file, const char* input_files[], float volumes[], float pans[], int file_count) {
    if (file_count == 0) {
        printf("Erro: Nenhum arquivo para mixar\n");
        return -1;
    }
    
    printf("🎵 Iniciando mixagem de %d arquivo(s)...\n", file_count);

    FILE** files = malloc(file_count * sizeof(FILE*));
    WAV_Header* headers = malloc(file_count * sizeof(WAV_Header));
    WAV_Fmt* fmts = malloc(file_count * sizeof(WAV_Fmt));
    WAV_Data* datas = malloc(file_count * sizeof(WAV_Data));
    
    uint32_t max_data_size = 0;
    uint32_t sample_rate = 44100;
    uint16_t num_channels = 2;
    uint16_t bits_per_sample = 16;

    for (int i = 0; i < file_count; i++) {
        files[i] = fopen(input_files[i], "rb");
        if (!files[i]) {
            printf("Erro ao abrir: %s\n", input_files[i]);
            for (int j = 0; j < i; j++) fclose(files[j]);
            free(files); free(headers); free(fmts); free(datas);
            return -1;
        }

        fread(&headers[i], sizeof(WAV_Header), 1, files[i]);
        fread(&fmts[i], sizeof(WAV_Fmt), 1, files[i]);
        
        char chunk_id[4];
        uint32_t chunk_size;
        while (fread(chunk_id, 4, 1, files[i]) == 1) {
            if (strncmp(chunk_id, "data", 4) == 0) {
                fread(&chunk_size, 4, 1, files[i]);
                datas[i].subchunk2ID[0] = 'd';
                datas[i].subchunk2ID[1] = 'a';
                datas[i].subchunk2ID[2] = 't';
                datas[i].subchunk2ID[3] = 'a';
                datas[i].subchunk2Size = chunk_size;
                break;
            } else {
                fread(&chunk_size, 4, 1, files[i]);
                fseek(files[i], chunk_size, SEEK_CUR);
            }
        }

        if (strncmp(headers[i].chunkID, "RIFF", 4) != 0 || 
            strncmp(headers[i].format, "WAVE", 4) != 0) {
            printf("Arquivo não é WAV válido: %s\n", input_files[i]);
            fclose(files[i]);
            for (int j = 0; j < i; j++) fclose(files[j]);
            free(files); free(headers); free(fmts); free(datas);
            return -1;
        }

        if (i == 0) {
            sample_rate = fmts[i].sampleRate;
            num_channels = fmts[i].numChannels;
            bits_per_sample = fmts[i].bitsPerSample;
        }

        if (datas[i].subchunk2Size > max_data_size) {
            max_data_size = datas[i].subchunk2Size;
        }
        
        printf("📁 Arquivo %d: %s (%u bytes, %d Hz, %d canais)\n", 
               i + 1, input_files[i], datas[i].subchunk2Size, 
               fmts[i].sampleRate, fmts[i].numChannels);
    }
    
    printf("📊 Tamanho máximo de dados: %u bytes\n", max_data_size);

    FILE* output = fopen(output_file, "wb");
    if (!output) {
        printf("Erro ao criar: %s\n", output_file);
        for (int i = 0; i < file_count; i++) fclose(files[i]);
        free(files); free(headers); free(fmts); free(datas);
        return -1;
    }

    WAV_Header out_header = headers[0];
    WAV_Fmt out_fmt = fmts[0];
    WAV_Data out_data;
    
    out_data.subchunk2ID[0] = 'd';
    out_data.subchunk2ID[1] = 'a';
    out_data.subchunk2ID[2] = 't';
    out_data.subchunk2ID[3] = 'a';
    
    out_data.subchunk2Size = max_data_size;
    out_header.chunkSize = 36 + max_data_size;

    fwrite(&out_header, sizeof(WAV_Header), 1, output);
    fwrite(&out_fmt, sizeof(WAV_Fmt), 1, output);
    fwrite(&out_data, sizeof(WAV_Data), 1, output);

    for (int i = 0; i < file_count; i++) {
        fclose(files[i]);
    }

    size_t max_samples = max_data_size / sizeof(int16_t);
    int16_t* mix_buffer = calloc(max_samples, sizeof(int16_t));
    int16_t* sample_buffer = malloc(max_data_size);

    for (int i = 0; i < file_count; i++) {
        files[i] = fopen(input_files[i], "rb");
        if (!files[i]) {
            printf("Erro ao reabrir: %s\n", input_files[i]);
            continue;
        }
        
        fseek(files[i], sizeof(WAV_Header) + sizeof(WAV_Fmt), SEEK_SET);
        
        char chunk_id[4];
        uint32_t chunk_size;
        while (fread(chunk_id, 4, 1, files[i]) == 1) {
            if (strncmp(chunk_id, "data", 4) == 0) {
                fread(&chunk_size, 4, 1, files[i]);
                break;
            } else {
                fread(&chunk_size, 4, 1, files[i]);
                fseek(files[i], chunk_size, SEEK_CUR);
            }
        }
        
        size_t samples_read = fread(sample_buffer, 1, datas[i].subchunk2Size, files[i]);
        size_t sample_count = samples_read / sizeof(int16_t);
        
        if (samples_read == 0) {
            printf("Aviso: Nenhum dado lido de %s\n", input_files[i]);
            fclose(files[i]);
            continue;
        }
        
        float pan = (pans != NULL) ? pans[i] : 0.0f;
        float pan_rad = (pan + 1.0f) * M_PI / 4.0f;
        float left_gain = cosf(pan_rad);
        float right_gain = sinf(pan_rad);

        if (num_channels == 2) {
            for (size_t j = 0; j < sample_count && j < max_samples; j += 2) {
                float left = sample_buffer[j] * volumes[i];
                float right = sample_buffer[j + 1] * volumes[i];
                
                left *= left_gain;
                right *= right_gain;
                
                float mixed_left = mix_buffer[j] + left;
                float mixed_right = mix_buffer[j + 1] + right;
                
                if (mixed_left > 32767) mixed_left = 32767;
                if (mixed_left < -32768) mixed_left = -32768;
                if (mixed_right > 32767) mixed_right = 32767;
                if (mixed_right < -32768) mixed_right = -32768;
                
                mix_buffer[j] = (int16_t)mixed_left;
                mix_buffer[j + 1] = (int16_t)mixed_right;
            }
        } else {
            for (size_t j = 0; j < sample_count && (j * 2) < max_samples; j++) {
                float sample = sample_buffer[j] * volumes[i];
                
                float left = sample * left_gain;
                float right = sample * right_gain;
                
                size_t left_idx = j * 2;
                size_t right_idx = j * 2 + 1;
                
                if (left_idx < max_samples) {
                    float mixed_left = mix_buffer[left_idx] + left;
                    if (mixed_left > 32767) mixed_left = 32767;
                    if (mixed_left < -32768) mixed_left = -32768;
                    mix_buffer[left_idx] = (int16_t)mixed_left;
                }
                
                if (right_idx < max_samples) {
                    float mixed_right = mix_buffer[right_idx] + right;
                    if (mixed_right > 32767) mixed_right = 32767;
                    if (mixed_right < -32768) mixed_right = -32768;
                    mix_buffer[right_idx] = (int16_t)mixed_right;
                }
            }
        }

        fclose(files[i]);
    }

    size_t written = fwrite(mix_buffer, 1, max_data_size, output);
    if (written != max_data_size) {
        printf("⚠️ Aviso: Escritos %zu bytes de %u esperados\n", written, max_data_size);
    } else {
        printf("✅ Dados mixados escritos com sucesso (%zu bytes)\n", written);
    }

    fclose(output);
    free(mix_buffer);
    free(sample_buffer);
    free(files);
    free(headers);
    free(fmts);
    free(datas);

    printf("🎉 Mixagem concluída com sucesso!\n");
    return 0;
}

int get_wav_info(const char* filename, WAV_Info* info) {
    if (!filename || !info) return -1;
    
    FILE* file = fopen(filename, "rb");
    if (!file) return -1;
    
    WAV_Header header;
    WAV_Fmt fmt;
    
    if (fread(&header, sizeof(WAV_Header), 1, file) != 1) {
        fclose(file);
        return -1;
    }
    
    if (fread(&fmt, sizeof(WAV_Fmt), 1, file) != 1) {
        fclose(file);
        return -1;
    }
    
    if (strncmp(header.chunkID, "RIFF", 4) != 0 || 
        strncmp(header.format, "WAVE", 4) != 0) {
        fclose(file);
        return -1;
    }
    
    char chunk_id[4];
    uint32_t chunk_size;
    uint32_t data_size = 0;
    
    while (fread(chunk_id, 4, 1, file) == 1) {
        if (strncmp(chunk_id, "data", 4) == 0) {
            fread(&chunk_size, 4, 1, file);
            data_size = chunk_size;
            break;
        } else {
            fread(&chunk_size, 4, 1, file);
            fseek(file, chunk_size, SEEK_CUR);
        }
    }
    
    fclose(file);
    
    if (data_size == 0) return -1;
    
    info->sample_rate = fmt.sampleRate;
    info->num_channels = fmt.numChannels;
    info->bits_per_sample = fmt.bitsPerSample;
    info->data_size = data_size;
    info->duration_samples = data_size / (fmt.numChannels * (fmt.bitsPerSample / 8));
    
    return 0;
}

int read_wav_samples(const char* filename, int16_t** samples, size_t* sample_count, int max_samples) {
    if (!filename || !samples || !sample_count) return -1;
    
    FILE* file = fopen(filename, "rb");
    if (!file) return -1;
    
    WAV_Header header;
    WAV_Fmt fmt;
    
    if (fread(&header, sizeof(WAV_Header), 1, file) != 1) {
        fclose(file);
        return -1;
    }
    
    if (fread(&fmt, sizeof(WAV_Fmt), 1, file) != 1) {
        fclose(file);
        return -1;
    }
    
    if (strncmp(header.chunkID, "RIFF", 4) != 0 || 
        strncmp(header.format, "WAVE", 4) != 0) {
        fclose(file);
        return -1;
    }
    
    char chunk_id[4];
    uint32_t chunk_size;
    long data_start = 0;
    
    while (fread(chunk_id, 4, 1, file) == 1) {
        if (strncmp(chunk_id, "data", 4) == 0) {
            fread(&chunk_size, 4, 1, file);
            data_start = ftell(file);
            break;
        } else {
            fread(&chunk_size, 4, 1, file);
            fseek(file, chunk_size, SEEK_CUR);
        }
    }
    
    if (data_start == 0) {
        fclose(file);
        return -1;
    }
    
    size_t bytes_to_read = chunk_size;
    if (max_samples > 0) {
        size_t max_bytes = max_samples * fmt.numChannels * sizeof(int16_t);
        if (bytes_to_read > max_bytes) {
            bytes_to_read = max_bytes;
        }
    } else {
        if (bytes_to_read > 1024 * 1024) {
            bytes_to_read = 1024 * 1024;
        }
    }
    
    *samples = malloc(bytes_to_read);
    if (!*samples) {
        fclose(file);
        return -1;
    }
    
    fseek(file, data_start, SEEK_SET);
    size_t bytes_read = fread(*samples, 1, bytes_to_read, file);
    *sample_count = bytes_read / sizeof(int16_t);
    
    if (fmt.numChannels == 2 && *sample_count > 0) {
        int16_t* mono_samples = malloc((*sample_count / 2) * sizeof(int16_t));
        if (mono_samples) {
            for (size_t i = 0; i < *sample_count / 2; i++) {
                mono_samples[i] = ((*samples)[i * 2] + (*samples)[i * 2 + 1]) / 2;
            }
            free(*samples);
            *samples = mono_samples;
            *sample_count = *sample_count / 2;
        }
    }
    
    fclose(file);
    return 0;
}

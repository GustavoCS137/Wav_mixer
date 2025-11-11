#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "wav_reader.h"

// Estruturas WAV
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

int mix_wav_files(const char* output_file, const char* input_files[], float volumes[], int file_count) {
    if (file_count == 0) return -1;

    FILE** files = malloc(file_count * sizeof(FILE*));
    WAV_Header* headers = malloc(file_count * sizeof(WAV_Header));
    WAV_Fmt* fmts = malloc(file_count * sizeof(WAV_Fmt));
    WAV_Data* datas = malloc(file_count * sizeof(WAV_Data));
    
    uint32_t max_data_size = 0;

    // Abrir arquivos e ler headers
    for (int i = 0; i < file_count; i++) {
        files[i] = fopen(input_files[i], "rb");
        if (!files[i]) {
            printf("Erro ao abrir: %s\n", input_files[i]);
            for (int j = 0; j < i; j++) fclose(files[j]);
            free(files); free(headers); free(fmts); free(datas);
            return -1;
        }

        // Ler headers
        fread(&headers[i], sizeof(WAV_Header), 1, files[i]);
        fread(&fmts[i], sizeof(WAV_Fmt), 1, files[i]);
        fread(&datas[i], sizeof(WAV_Data), 1, files[i]);

        // Verificar se é WAV válido
        if (strncmp(headers[i].chunkID, "RIFF", 4) != 0 || 
            strncmp(headers[i].format, "WAVE", 4) != 0) {
            printf("Arquivo não é WAV válido: %s\n", input_files[i]);
            fclose(files[i]);
            for (int j = 0; j < i; j++) fclose(files[j]);
            free(files); free(headers); free(fmts); free(datas);
            return -1;
        }

        // Encontrar maior tamanho de dados
        if (datas[i].subchunk2Size > max_data_size) {
            max_data_size = datas[i].subchunk2Size;
        }
    }

    // Criar arquivo de saída
    FILE* output = fopen(output_file, "wb");
    if (!output) {
        printf("Erro ao criar: %s\n", output_file);
        for (int i = 0; i < file_count; i++) fclose(files[i]);
        free(files); free(headers); free(fmts); free(datas);
        return -1;
    }

    // Preparar header de saída (usar do primeiro arquivo)
    WAV_Header out_header = headers[0];
    WAV_Fmt out_fmt = fmts[0];
    WAV_Data out_data = datas[0];
    
    out_data.subchunk2Size = max_data_size;
    out_header.chunkSize = 36 + max_data_size;

    // Escrever headers
    fwrite(&out_header, sizeof(WAV_Header), 1, output);
    fwrite(&out_fmt, sizeof(WAV_Fmt), 1, output);
    fwrite(&out_data, sizeof(WAV_Data), 1, output);

    // Mixar dados
    int16_t* mix_buffer = calloc(max_data_size / sizeof(int16_t), sizeof(int16_t));
    int16_t* sample_buffer = malloc(max_data_size);

    for (int i = 0; i < file_count; i++) {
        size_t samples_read = fread(sample_buffer, 1, datas[i].subchunk2Size, files[i]);
        size_t sample_count = samples_read / sizeof(int16_t);

        for (size_t j = 0; j < sample_count; j++) {
            float mixed = mix_buffer[j] + (sample_buffer[j] * volumes[i]);
            if (mixed > 32767) mixed = 32767;
            if (mixed < -32768) mixed = -32768;
            mix_buffer[j] = (int16_t)mixed;
        }

        fclose(files[i]);
    }

    // Escrever dados mixados
    fwrite(mix_buffer, 1, max_data_size, output);

    // Limpeza
    fclose(output);
    free(mix_buffer);
    free(sample_buffer);
    free(files);
    free(headers);
    free(fmts);
    free(datas);

    return 0;
}

#include "wav_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int read_wav_file(const char* filename, WAV_Header* header, WAV_Fmt* fmt, WAV_Data* data) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    fread(header, sizeof(WAV_Header), 1, fp);
    fread(fmt, sizeof(WAV_Fmt), 1, fp);
    fread(data, sizeof(WAV_Data), 1, fp);
    
    fclose(fp);
    return 0;
}

int mix_wav_files(const char* output_file, const char** input_files, float* volumes, int num_files) {
    if (num_files == 0) return -1;
    
    printf("🎵 Iniciando MIXAGEM REAL de %d arquivos...\n", num_files);
    
    // ABRIR TODOS OS ARQUIVOS
    FILE* files[5];
    WAV_Header headers[5];
    WAV_Fmt fmts[5];
    
    for (int i = 0; i < num_files; i++) {
        printf("🔍 Abrindo: %s\n", input_files[i]);
        files[i] = fopen(input_files[i], "rb");
        
        if (!files[i]) {
            printf("❌ Erro ao abrir: %s\n", input_files[i]);
            for (int j = 0; j < i; j++) fclose(files[j]);
            return -1;
        }
        
        // LER HEADERS BÁSICOS
        fread(&headers[i], sizeof(WAV_Header), 1, files[i]);
        fread(&fmts[i], sizeof(WAV_Fmt), 1, files[i]);
        
        printf("✅ %s - %dHz, %d canais (Volume: %.1f)\n", 
               input_files[i], fmts[i].sampleRate, fmts[i].numChannels, volumes[i]);
    }
    
    // CRIAR ARQUIVO DE SAÍDA
    FILE* output = fopen(output_file, "wb");
    if (!output) {
        printf("❌ Não consegui criar: %s\n", output_file);
        for (int i = 0; i < num_files; i++) fclose(files[i]);
        return -1;
    }
    
    // COPIAR HEADERS DO PRIMEIRO ARQUIVO PARA SAÍDA
    rewind(files[0]);
    unsigned char header_buffer[8192];
    size_t header_bytes = fread(header_buffer, 1, sizeof(header_buffer), files[0]);
    fwrite(header_buffer, 1, header_bytes, output);
    
    printf("📦 Headers copiados (%zu bytes)\n", header_bytes);
    
    // ENCONTRAR POSIÇÃO DOS DADOS DE ÁUDIO EM CADA ARQUIVO
    long audio_starts[5];
    for (int i = 0; i < num_files; i++) {
        rewind(files[i]);
        
        // LER ATÉ ENCONTRAR O CHUNK "data"
        WAV_Header h;
        WAV_Fmt f;
        char chunk_id[4];
        uint32_t chunk_size;
        
        fread(&h, sizeof(WAV_Header), 1, files[i]);
        fread(&f, sizeof(WAV_Fmt), 1, files[i]);
        
        // PROCURAR CHUNK "data"
        while (1) {
            if (fread(chunk_id, 1, 4, files[i]) != 4) break;
            fread(&chunk_size, 4, 1, files[i]);
            
            if (strncmp(chunk_id, "data", 4) == 0) {
                audio_starts[i] = ftell(files[i]);
                break;
            } else {
                // PULAR CHUNK (como "LIST")
                fseek(files[i], chunk_size, SEEK_CUR);
            }
        }
    }
    
    printf("🔧 Mixando áudio...\n");
    
    // MIXAGEM DOS SAMPLES
    int16_t samples[5];
    int16_t mixed_sample;
    int samples_processed = 0;
    
    while (1) {
        int samples_read = 0;
        int32_t total_sample = 0;
        
        // LER UM SAMPLE DE CADA ARQUIVO
        for (int i = 0; i < num_files; i++) {
            if (fread(&samples[i], sizeof(int16_t), 1, files[i]) == 1) {
                // APLICAR VOLUME E SOMAR
                total_sample += (int32_t)(samples[i] * volumes[i]);
                samples_read++;
            }
        }
        
        if (samples_read == 0) break; // TODOS TERMINARAM
        
        // CALCULAR MÉDIA
        mixed_sample = (int16_t)(total_sample / num_files);
        
        // EVITAR CLIPPING
        if (mixed_sample > 32767) mixed_sample = 32767;
        if (mixed_sample < -32768) mixed_sample = -32768;
        
        // ESCREVER SAMPLE MIXADO
        fwrite(&mixed_sample, sizeof(int16_t), 1, output);
        samples_processed++;
        
        if (samples_processed % 100000 == 0) {
            printf("⏳ %d samples processados...\n", samples_processed);
        }
    }
    
    printf("✅ MIXAGEM CONCLUÍDA! %d samples processados\n", samples_processed);
    
    // FECHAR ARQUIVOS
    for (int i = 0; i < num_files; i++) {
        fclose(files[i]);
    }
    fclose(output);
    
    return 0;
}
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "wav_reader.h"

int main() {
    char files[5][256];
    float volumes[5];
    int file_count = 0;
    
    printf("=== 🎵 SMART WAV MIXER (CONSOLE) ===\n\n");
    
    // Entrada de arquivos
    for (int i = 0; i < 5; i++) {
        printf("Arquivo %d (ou Enter para parar): ", i+1);
        
        if (fgets(files[i], sizeof(files[i]), stdin) == NULL) break;
        
        // Remover newline
        files[i][strcspn(files[i], "\n")] = 0;
        
        if (strlen(files[i]) == 0) break;
        
        printf("Volume (0.0 - 2.0) [1.0]: ");
        char vol_input[10];
        fgets(vol_input, sizeof(vol_input), stdin);
        volumes[i] = (strlen(vol_input) > 1) ? atof(vol_input) : 1.0f;
        
        file_count++;
        printf("\n");
    }
    
    if (file_count == 0) {
        printf("❌ Nenhum arquivo selecionado!\n");
        return 1;
    }
    
    printf("\n🎵 Mixando %d arquivos...\n", file_count);
    for (int i = 0; i < file_count; i++) {
        printf("📁 %s (volume: %.1f)\n", files[i], volumes[i]);
    }
    
    // Converter array para ponteiros
    const char* input_files[5];
    for (int i = 0; i < file_count; i++) {
        input_files[i] = files[i];
    }
    
    // Mixar
    if (mix_wav_files("bin/mixed_output.wav", input_files, volumes, file_count) == 0) {
        printf("\n✅ MIXAGEM CONCLUÍDA!\n");
        printf("📁 Arquivo: bin/mixed_output.wav\n");
        printf("🎧 Abra o arquivo para ouvir o resultado!\n");
    } else {
        printf("\n❌ ERRO na mixagem!\n");
    }
    
    return 0;
}
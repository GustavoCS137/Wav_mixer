#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio_editor.h"
#include "wav_reader.h"

int main(int argc, char *argv[]) {
    printf("🎵 Studio WAV - Editor de Áudio Profissional\n");
    printf("============================================\n");
    printf("Recursos:\n");
    printf("• Timeline visual com waveforms\n");
    printf("• Controles de reprodução\n");
    printf("• Mixer com volume e pan\n");
    printf("• Exportação para WAV\n");
    printf("• Interface profissional\n\n");
    
    if (argc > 1) {
        char input_buffer[256];
        int choice = 0;
        
        printf("Digite 1 para modo console ou 2 para modo grafico: ");
        if (scanf("%d", &choice) == 1) {
            if (choice == 1) {
                printf("Modo console selecionado.\n");
                if (get_user_input(input_buffer, sizeof(input_buffer)) == 0) {
                    printf("Arquivo informado: %s\n", input_buffer);
                }
            }
        }
        
        printf("Use sem argumentos para abrir o editor gráfico.\n");
        return 1;
    }
    
    launch_audio_editor(argc, argv);
    return 0;
}

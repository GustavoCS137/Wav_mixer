#include <stdio.h>
#include <stdlib.h>
#include "audio_editor.h"

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
        printf("Use sem argumentos para abrir o editor gráfico.\n");
        return 1;
    }
    
    launch_audio_editor(argc, argv);
    return 0;
}

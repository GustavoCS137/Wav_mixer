# Studio WAV - Editor de Áudio Profissional

## Descrição do Projeto

Studio WAV é um editor de áudio profissional desenvolvido em C com interface gráfica GTK3. O projeto permite carregar múltiplos arquivos WAV, mixá-los com controles de volume e pan, visualizar waveforms em uma timeline interativa e exportar o resultado final.

## Estrutura do Projeto

### Arquivos Principais

- **main.c**: Arquivo principal que inicializa a aplicação
- **audio_editor.h**: Cabeçalho com definições de estruturas e funções do editor
- **audio_editor.c**: Implementação da interface gráfica e lógica do editor
- **wav_reader.h**: Cabeçalho com funções de leitura e processamento de arquivos WAV
- **wav_reader.c**: Implementação das funções de manipulação de arquivos WAV
- **Makefile**: Arquivo de build do projeto

## Requisitos Técnicos Implementados

### 1. Funções de Entrada e Saída
- `printf()`: Saída formatada de dados
- `scanf()`: Entrada de dados do usuário (main.c)
- `fgets()`: Leitura de strings (wav_reader.c)
- `fread()`, `fwrite()`: Operações de arquivo

### 2. Estruturas de Controle
- `if/else`: Condicionais
- `while`: Loops condicionais
- `for`: Loops com contador
- `switch/case`: Seleção múltipla (implícito em condicionais)

### 3. Ponteiros
- Ponteiros declarados como variáveis: `int16_t *left_channel`, `FILE** files`
- Ponteiros como parâmetros de função: `const char* filename`, `WAV_Info* info`
- Aritmética de ponteiros e acesso indireto

### 4. Alocação Dinâmica
- `malloc()`: Alocação de memória
- `calloc()`: Alocação e inicialização
- `free()`: Desalocação de memória
- Exemplos em: wav_reader.c (linhas 40-43, 135, 403-405)

### 5. Funções com Vetores/Matrizes como Parâmetros
- `mix_wav_files()`: Recebe `const char* input_files[]`, `float volumes[]`, `float pans[]`
- `process_audio_matrix()`: Recebe `int16_t matrix[][2]` (matriz bidimensional)
- `process_audio_file_config_array()`: Recebe `AudioFileConfig configs[]`

### 6. Estruturas Homogêneas
- Vetores: `char chunk_id[4]`, `int16_t* sample_buffer`
- Strings: `char filename[256]`, `char* filename`
- Matrizes: `int16_t matrix[][2]` em process_audio_matrix()

### 7. Estruturas Heterogêneas
- **Estruturas simples**: `AudioClip`, `WAV_Info`, `MixSettings`
- **Estruturas aninhadas**: `AudioFileConfig` contém `WAV_Info` e `MixSettings`
- **Vetores de estruturas**: `AudioFileConfig configs[]`
- **Estrutura como parâmetro por valor**: `create_mix_settings_by_value(MixSettings settings)`
- **Estrutura como parâmetro por referência**: `modify_mix_settings_by_reference(MixSettings* settings)`
- **Estrutura como membro de outra estrutura**: `AudioEditor` contém `AudioClip* selected_clip`

### 8. Arquivos
- **Abertura**: `fopen(filename, "rb")`, `fopen(filename, "wb")`
- **Fechamento**: `fclose(file)`
- **Leitura**: `fread()`, `fseek()`, `ftell()`
- **Escrita**: `fwrite()`
- Implementado em: wav_reader.c (funções mix_wav_files, get_wav_info, read_wav_samples)

### 9. Divisão do Projeto em Arquivos
- **main.c**: Ponto de entrada do programa
- **audio_editor.h**: Declarações de tipos e funções do editor
- **audio_editor.c**: Implementação do editor (1122+ linhas)
- **wav_reader.h**: Declarações de tipos e funções de leitura WAV
- **wav_reader.c**: Implementação de leitura WAV (440+ linhas)
- **Makefile**: Sistema de build

## Estruturas de Dados Principais

### AudioClip
```c
typedef struct {
    char *filename;
    float volume;
    float pan;
    int start_pos;
    int duration;
    int muted;
    int solo;
} AudioClip;
```

### AudioEditor
```c
typedef struct {
    GtkWidget *window;
    GList *audio_clips;
    AudioClip *selected_clip;
    int16_t *audio_buffer;
    // ... outros membros
} AudioEditor;
```

### AudioFileConfig (Estrutura Aninhada)
```c
typedef struct {
    WAV_Info info;
    MixSettings settings;
    char filename[256];
} AudioFileConfig;
```

## Funcionalidades

1. **Carregamento de Arquivos**: Interface gráfica para seleção de arquivos WAV
2. **Timeline Visual**: Visualização de clips de áudio com waveforms
3. **Controles de Mixagem**: Ajuste de volume e pan por clip
4. **Reprodução**: Playback de áudio mixado (requer SDL2)
5. **Exportação**: Geração de arquivo WAV final com mixagem aplicada
6. **Processamento de Matriz**: Função para processar dados de áudio em formato matricial

## Compilação

```bash
cd scr
make
```

## Execução

```bash
./audio_editor.exe
```

## Dependências

- GTK3 (interface gráfica)
- SDL2 (opcional, para reprodução de áudio)
- Compilador C (GCC ou MinGW)

## Exemplos de Uso

### Modo Console
Execute com argumentos para modo console:
```bash
./audio_editor.exe console
```

### Modo Gráfico
Execute sem argumentos para interface gráfica:
```bash
./audio_editor.exe
```

## Notas Técnicas

- O projeto utiliza alocação dinâmica extensivamente para gerenciar buffers de áudio
- Ponteiros são usados para eficiência na manipulação de grandes volumes de dados
- Estruturas aninhadas permitem organização hierárquica de dados
- Funções com matrizes como parâmetros facilitam processamento de dados estéreo
- Todas as operações de arquivo incluem tratamento de erros adequado


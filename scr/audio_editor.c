#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#define usleep(x) Sleep((x)/1000)
#else
#include <unistd.h>
#endif
#include "audio_editor.h"
#include "wav_reader.h"

static AudioEditor *g_editor = NULL;

#ifdef USE_SDL2
static void audio_callback(void *userdata, Uint8 *stream, int len) {
    AudioEditor *editor = (AudioEditor *)userdata;
    
    if (!editor || !editor->audio_playing || !editor->audio_buffer) {
        memset(stream, 0, len);
        return;
    }
    
    int bytes_available = editor->audio_buffer_size - editor->audio_buffer_pos;
    int bytes_to_copy = (bytes_available < len) ? bytes_available : len;
    
    if (bytes_to_copy > 0) {
        memcpy(stream, (Uint8 *)editor->audio_buffer + editor->audio_buffer_pos, bytes_to_copy);
        editor->audio_buffer_pos += bytes_to_copy;
        
        if (editor->audio_buffer_size > 0) {
            editor->current_position = (editor->audio_buffer_pos * 1000000) / editor->audio_buffer_size;
        }
    }
    
    if (bytes_to_copy < len) {
        memset(stream + bytes_to_copy, 0, len - bytes_to_copy);
        editor->audio_playing = 0;
        editor->playing = 0;
        editor->current_position = 0;
    }
}
#endif

static int load_audio_for_playback(AudioEditor *editor) {
    #ifndef USE_SDL2
    printf("⚠️ SDL2 não disponível. Reprodução de áudio desabilitada.\n");
    printf("💡 Instale SDL2 com: pacman -S mingw-w64-ucrt-x86_64-SDL2\n");
    return -1;
    #else
    if (g_list_length(editor->audio_clips) == 0) {
        printf("❌ Nenhum áudio carregado para reproduzir\n");
        return -1;
    }
    
    if (editor->audio_buffer) {
        free(editor->audio_buffer);
        editor->audio_buffer = NULL;
    }
    
    #ifdef _WIN32
    const char *temp_file = "playback_temp.wav";
    #else
    const char *temp_file = "/tmp/playback_temp.wav";
    #endif
    
    int file_count = g_list_length(editor->audio_clips);
    const char **input_files = malloc(file_count * sizeof(char*));
    float *volumes = malloc(file_count * sizeof(float));
    float *pans = malloc(file_count * sizeof(float));
    
    int i = 0;
    GList *iter = editor->audio_clips;
    while (iter != NULL) {
        AudioClip *clip = (AudioClip *)iter->data;
        input_files[i] = clip->filename;
        volumes[i] = clip->volume;
        pans[i] = clip->pan;
        i++;
        iter = g_list_next(iter);
    }
    
    if (mix_wav_files(temp_file, input_files, volumes, pans, file_count) != 0) {
        printf("❌ Erro ao mixar arquivos para reprodução\n");
        free(input_files);
        free(volumes);
        free(pans);
        return -1;
    }
    
    free(input_files);
    free(volumes);
    free(pans);
    
    #ifdef USE_SDL2
    SDL_AudioSpec wav_spec;
    Uint32 wav_length;
    Uint8 *wav_buffer;
    
    if (SDL_LoadWAV(temp_file, &wav_spec, &wav_buffer, &wav_length) == NULL) {
        printf("❌ Erro ao carregar WAV: %s\n", SDL_GetError());
        return -1;
    }
    
    editor->audio_spec = wav_spec;
    editor->audio_spec.format = AUDIO_S16SYS;
    editor->audio_buffer = (int16_t *)wav_buffer;
    editor->audio_buffer_size = wav_length;
    editor->audio_buffer_pos = 0;
    #else
    FILE *f = fopen(temp_file, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        editor->audio_buffer_size = ftell(f);
        fclose(f);
        editor->audio_buffer_pos = 0;
        printf("✅ Arquivo preparado: %zu bytes (reprodução desabilitada - SDL2 não disponível)\n", 
               editor->audio_buffer_size);
    }
    #endif
    
    return 0;
}
#endif

static void on_open_file(GtkButton *button, gpointer user_data);
static void on_play(GtkButton *button, gpointer user_data);
static void on_stop(GtkButton *button, gpointer user_data);
static void on_export(GtkButton *button, gpointer user_data);
static gboolean draw_timeline(GtkWidget *widget, cairo_t *cr, gpointer user_data);
static gboolean on_timeline_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static void update_mixer_controls(AudioEditor *editor);
static void update_info_label(AudioEditor *editor);

static void on_open_file(GtkButton *button, gpointer user_data) {
    AudioEditor *editor = (AudioEditor *)user_data;
    
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Abrir Arquivo de Áudio",
                                                   GTK_WINDOW(editor->window),
                                                   GTK_FILE_CHOOSER_ACTION_OPEN,
                                                   "_Cancelar", GTK_RESPONSE_CANCEL,
                                                   "_Abrir", GTK_RESPONSE_ACCEPT,
                                                   NULL);
    
    GtkFileFilter *filter_wav = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_wav, "Arquivos WAV (*.wav)");
    gtk_file_filter_add_pattern(filter_wav, "*.wav");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_wav);
    
    GtkFileFilter *filter_all = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_all, "Todos os arquivos");
    gtk_file_filter_add_pattern(filter_all, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_all);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        
        WAV_Info wav_info;
        int duration_samples = 100000;
        if (get_wav_info(filename, &wav_info) == 0) {
            duration_samples = wav_info.duration_samples;
            printf("📊 WAV Info: %d Hz, %d canais, %d bits, %d amostras\n", 
                   wav_info.sample_rate, wav_info.num_channels, 
                   wav_info.bits_per_sample, wav_info.duration_samples);
        }
        
        AudioClip *clip = g_malloc0(sizeof(AudioClip));
        clip->filename = g_strdup(filename);
        clip->volume = 1.0f;
        clip->pan = 0.0f;
        clip->start_pos = 0;
        clip->duration = duration_samples * 10;
        clip->muted = 0;
        clip->solo = 0;
        
        editor->audio_clips = g_list_append(editor->audio_clips, clip);
        
        printf("✅ Arquivo adicionado: %s\n", filename);
        
        if (editor->status_bar) {
            char status_msg[200];
            snprintf(status_msg, sizeof(status_msg), "✅ Arquivo adicionado: %s", strrchr(filename, '/') ? strrchr(filename, '/') + 1 : (strrchr(filename, '\\') ? strrchr(filename, '\\') + 1 : filename));
            gtk_statusbar_pop(GTK_STATUSBAR(editor->status_bar), 0);
            gtk_statusbar_push(GTK_STATUSBAR(editor->status_bar), 0, status_msg);
        }
        
        update_info_label(editor);
        
        if (editor->timeline_drawing_area) {
            gtk_widget_queue_draw(editor->timeline_drawing_area);
        }
        
        g_free(filename);
    }
    
    gtk_widget_destroy(dialog);
}

static gboolean update_playback(gpointer user_data) {
    AudioEditor *editor = (AudioEditor *)user_data;
    
    if (!editor->playing || !editor->audio_playing) {
        if (editor->audio_buffer_pos >= editor->audio_buffer_size) {
            editor->playing = 0;
            editor->audio_playing = 0;
            editor->current_position = 0;
            editor->audio_buffer_pos = 0;
            #ifdef USE_SDL2
            if (editor->audio_device != 0) {
                SDL_PauseAudioDevice(editor->audio_device, 1);
            }
            #endif
        }
        return editor->playing ? TRUE : FALSE;
    }
    
    int max_duration = 1000000;
    #ifdef USE_SDL2
    if (editor->audio_buffer_size > 0 && editor->audio_spec.freq > 0) {
        int samples = editor->audio_buffer_size / (sizeof(int16_t) * editor->audio_spec.channels);
        int seconds = samples / editor->audio_spec.freq;
        max_duration = seconds * 100000;
    }
    #else
    if (editor->audio_buffer_size > 0) {
        int samples = editor->audio_buffer_size / (sizeof(int16_t) * 2);
        int seconds = samples / 44100;
        max_duration = seconds * 100000;
    }
    #endif
    
    if (editor->audio_buffer_pos >= editor->audio_buffer_size) {
        editor->playing = 0;
        editor->audio_playing = 0;
        editor->current_position = 0;
        editor->audio_buffer_pos = 0;
        #ifdef USE_SDL2
        if (editor->audio_device != 0) {
            SDL_PauseAudioDevice(editor->audio_device, 1);
        }
        #endif
        if (editor->timeline_drawing_area) {
            gtk_widget_queue_draw(editor->timeline_drawing_area);
        }
        return FALSE;
    }
    
    if (editor->timeline_drawing_area) {
        gtk_widget_queue_draw(editor->timeline_drawing_area);
    }
    
    if (editor->transport_controls) {
        GList *children = gtk_container_get_children(GTK_CONTAINER(editor->transport_controls));
        if (children) {
            GtkWidget *label = GTK_WIDGET(children->data);
            int seconds = editor->current_position / 100000;
            int minutes = seconds / 60;
            seconds %= 60;
            char time_str[20];
            snprintf(time_str, sizeof(time_str), "%02d:%02d", minutes, seconds);
            gtk_label_set_text(GTK_LABEL(label), time_str);
            g_list_free(children);
        }
    }
    
    return TRUE;
}

static void on_play(GtkButton *button, gpointer user_data) {
    AudioEditor *editor = (AudioEditor *)user_data;
    
    if (editor->playing) {
        return;
    }
    
    if (!editor->audio_buffer || editor->audio_buffer_pos >= editor->audio_buffer_size) {
        #ifdef USE_SDL2
        if (editor->audio_device != 0) {
            SDL_PauseAudioDevice(editor->audio_device, 1);
            SDL_CloseAudioDevice(editor->audio_device);
            editor->audio_device = 0;
        }
        #endif
        
        if (load_audio_for_playback(editor) != 0) {
            return;
        }
    }
    
    if (editor->audio_buffer_pos >= editor->audio_buffer_size) {
        editor->audio_buffer_pos = 0;
        editor->current_position = 0;
    }
    
    editor->playing = 1;
    editor->audio_playing = 1;
    printf("▶️ Reproduzindo...\n");
    
    #ifdef USE_SDL2
    SDL_AudioSpec desired_spec = editor->audio_spec;
    SDL_AudioSpec obtained_spec;
    desired_spec.callback = audio_callback;
    desired_spec.userdata = editor;
    desired_spec.format = AUDIO_S16SYS;
    
    if (editor->audio_device == 0) {
        editor->audio_device = SDL_OpenAudioDevice(NULL, 0, &desired_spec, &obtained_spec, 0);
        if (editor->audio_device == 0) {
            printf("❌ Erro ao abrir dispositivo de áudio: %s\n", SDL_GetError());
            editor->playing = 0;
            editor->audio_playing = 0;
            return;
        }
        editor->audio_spec = obtained_spec;
    }
    
    SDL_PauseAudioDevice(editor->audio_device, 0);
    #else
    printf("⚠️ Reprodução de áudio desabilitada (SDL2 não disponível)\n");
    #endif
    
    if (editor->status_bar) {
        gtk_statusbar_pop(GTK_STATUSBAR(editor->status_bar), 0);
        gtk_statusbar_push(GTK_STATUSBAR(editor->status_bar), 0, "▶️ Reproduzindo...");
    }
    
    g_timeout_add(50, update_playback, editor);
}

static void on_stop(GtkButton *button, gpointer user_data) {
    AudioEditor *editor = (AudioEditor *)user_data;
    editor->playing = 0;
    editor->audio_playing = 0;
    editor->current_position = 0;
    editor->audio_buffer_pos = 0;
    
    #ifdef USE_SDL2
    if (editor->audio_device != 0) {
        SDL_PauseAudioDevice(editor->audio_device, 1);
    }
    #endif
    
    printf("⏹️ Parado\n");
    
    if (editor->status_bar) {
        gtk_statusbar_pop(GTK_STATUSBAR(editor->status_bar), 0);
        gtk_statusbar_push(GTK_STATUSBAR(editor->status_bar), 0, "⏹️ Parado");
    }
    
    if (editor->transport_controls) {
        GList *children = gtk_container_get_children(GTK_CONTAINER(editor->transport_controls));
        if (children) {
            GtkWidget *label = GTK_WIDGET(children->data);
            gtk_label_set_text(GTK_LABEL(label), "00:00");
            g_list_free(children);
        }
    }
    
    if (editor->timeline_drawing_area) {
        gtk_widget_queue_draw(editor->timeline_drawing_area);
    }
}

static void on_export(GtkButton *button, gpointer user_data) {
    AudioEditor *editor = (AudioEditor *)user_data;
    
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Exportar Projeto",
                                                   GTK_WINDOW(editor->window),
                                                   GTK_FILE_CHOOSER_ACTION_SAVE,
                                                   "_Cancelar", GTK_RESPONSE_CANCEL,
                                                   "_Exportar", GTK_RESPONSE_ACCEPT,
                                                   NULL);
    
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "mix_final.wav");
    
    GtkFileFilter *filter_wav = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_wav, "Arquivos WAV (*.wav)");
    gtk_file_filter_add_pattern(filter_wav, "*.wav");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_wav);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        
        printf("💾 Exportando para: %s\n", filename);
        
        int file_count = g_list_length(editor->audio_clips);
        if (file_count == 0) {
            printf("❌ Nenhum áudio para exportar!\n");
            g_free(filename);
            gtk_widget_destroy(dialog);
            return;
        }
        
        const char **input_files = malloc(file_count * sizeof(char*));
        float *volumes = malloc(file_count * sizeof(float));
        float *pans = malloc(file_count * sizeof(float));
        
        int i = 0;
        GList *iter = editor->audio_clips;
        while (iter != NULL) {
            AudioClip *clip = (AudioClip *)iter->data;
            input_files[i] = clip->filename;
            volumes[i] = clip->volume;
            pans[i] = clip->pan;
            i++;
            iter = g_list_next(iter);
        }
        
        if (editor->status_bar) {
            gtk_statusbar_pop(GTK_STATUSBAR(editor->status_bar), 0);
            gtk_statusbar_push(GTK_STATUSBAR(editor->status_bar), 0, "💾 Exportando...");
        }
        
        if (mix_wav_files(filename, input_files, volumes, pans, file_count) == 0) {
            printf("✅ Exportação concluída: %s\n", filename);
            if (editor->status_bar) {
                char status_msg[200];
                snprintf(status_msg, sizeof(status_msg), "✅ Exportação concluída: %s", strrchr(filename, '/') ? strrchr(filename, '/') + 1 : (strrchr(filename, '\\') ? strrchr(filename, '\\') + 1 : filename));
                gtk_statusbar_pop(GTK_STATUSBAR(editor->status_bar), 0);
                gtk_statusbar_push(GTK_STATUSBAR(editor->status_bar), 0, status_msg);
            }
        } else {
            printf("❌ Erro na exportação!\n");
            if (editor->status_bar) {
                gtk_statusbar_pop(GTK_STATUSBAR(editor->status_bar), 0);
                gtk_statusbar_push(GTK_STATUSBAR(editor->status_bar), 0, "❌ Erro na exportação!");
            }
        }
        
        free(input_files);
        free(volumes);
        free(pans);
        g_free(filename);
    }
    
    gtk_widget_destroy(dialog);
}

static void on_remove_audio(GtkButton *button, gpointer user_data) {
    AudioEditor *editor = (AudioEditor *)user_data;
    
    if (g_list_length(editor->audio_clips) == 0) {
        printf("⚠️ Nenhum áudio para remover\n");
        if (editor->status_bar) {
            gtk_statusbar_pop(GTK_STATUSBAR(editor->status_bar), 0);
            gtk_statusbar_push(GTK_STATUSBAR(editor->status_bar), 0, "⚠️ Nenhum áudio para remover");
        }
        return;
    }
    
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Remover Áudio",
                                                    GTK_WINDOW(editor->window),
                                                    GTK_DIALOG_MODAL,
                                                    "_Cancelar", GTK_RESPONSE_CANCEL,
                                                    "_Remover", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *label = gtk_label_new("Selecione o áudio para remover:");
    gtk_container_add(GTK_CONTAINER(content_area), label);
    
    GtkWidget *list_box = gtk_list_box_new();
    gtk_container_add(GTK_CONTAINER(content_area), list_box);
    
    GList *iter = editor->audio_clips;
    int index = 0;
    while (iter != NULL) {
        AudioClip *clip = (AudioClip *)iter->data;
        const char *filename = strrchr(clip->filename, '/');
        if (!filename) filename = strrchr(clip->filename, '\\');
        if (!filename) filename = clip->filename;
        else filename++;
        
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *row_label = gtk_label_new(filename);
        gtk_container_add(GTK_CONTAINER(row), row_label);
        gtk_list_box_insert(GTK_LIST_BOX(list_box), row, -1);
        gtk_widget_show_all(row);
        
        g_object_set_data(G_OBJECT(row), "clip-index", GINT_TO_POINTER(index));
        
        index++;
        iter = g_list_next(iter);
    }
    
    gtk_widget_show_all(dialog);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        GtkListBoxRow *selected_row = gtk_list_box_get_selected_row(GTK_LIST_BOX(list_box));
        if (selected_row) {
            int clip_index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(selected_row), "clip-index"));
            
            iter = editor->audio_clips;
            int current_index = 0;
            while (iter != NULL) {
                if (current_index == clip_index) {
                    AudioClip *clip = (AudioClip *)iter->data;
                    const char *filename = strrchr(clip->filename, '/');
                    if (!filename) filename = strrchr(clip->filename, '\\');
                    if (!filename) filename = clip->filename;
                    else filename++;
                    
                    printf("🗑️ Removendo: %s\n", filename);
                    
                    if (editor->audio_buffer) {
                        #ifdef USE_SDL2
                        SDL_FreeWAV((Uint8 *)editor->audio_buffer);
                        #else
                        free(editor->audio_buffer);
                        #endif
                        editor->audio_buffer = NULL;
                        editor->audio_buffer_size = 0;
                        editor->audio_buffer_pos = 0;
                    }
                    
                    g_free(clip->filename);
                    g_free(clip);
                    editor->audio_clips = g_list_delete_link(editor->audio_clips, iter);
                    
                    if (editor->status_bar) {
                        char status_msg[200];
                        snprintf(status_msg, sizeof(status_msg), "🗑️ Removido: %s", filename);
                        gtk_statusbar_pop(GTK_STATUSBAR(editor->status_bar), 0);
                        gtk_statusbar_push(GTK_STATUSBAR(editor->status_bar), 0, status_msg);
                    }
                    
                    update_info_label(editor);
                    if (editor->timeline_drawing_area) {
                        gtk_widget_queue_draw(editor->timeline_drawing_area);
                    }
                    
                    break;
                }
                current_index++;
                iter = g_list_next(iter);
            }
        }
    }
    
    gtk_widget_destroy(dialog);
}

static gboolean on_timeline_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    AudioEditor *editor = (AudioEditor *)user_data;
    
    if (event->button != 1) return FALSE;
    
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    int width = allocation.width;
    int height = allocation.height;
    
    int track_height = height / 8;
    if (track_height < 60) track_height = 60;
    int num_tracks = height / track_height;
    if (num_tracks < 1) num_tracks = 1;
    
    int track_num = 0;
    GList *iter = editor->audio_clips;
    while (iter != NULL) {
        AudioClip *clip = (AudioClip *)iter->data;
        int track_y = (track_num % num_tracks) * track_height + 5;
        
        float clip_seconds = clip->duration / 100000.0f;
        int clip_width = (int)(clip_seconds * 80.0f);
        if (clip_width < 400) clip_width = 400;
        int max_clip_width = width * 2;
        if (clip_width > max_clip_width) clip_width = max_clip_width;
        
        int current_track = track_num % num_tracks;
        int clip_x = 0;
        GList *prev_iter = editor->audio_clips;
        int prev_track_idx = 0;
        while (prev_iter != iter) {
            AudioClip *prev_clip = (AudioClip *)prev_iter->data;
            int prev_track = prev_track_idx % num_tracks;
            if (prev_track == current_track) {
                float prev_seconds = prev_clip->duration / 100000.0f;
                int prev_width = (int)(prev_seconds * 80.0f);
                if (prev_width < 400) prev_width = 400;
                if (prev_width > width * 2) prev_width = width * 2;
                clip_x += prev_width + 20;
            }
            prev_track_idx++;
            prev_iter = g_list_next(prev_iter);
        }
        
        int clip_height = track_height - 10;
        
        if (event->x >= clip_x && event->x <= clip_x + clip_width &&
            event->y >= track_y && event->y <= track_y + clip_height) {
            editor->selected_clip = clip;
            printf("✅ Clip selecionado: %s\n", clip->filename);
            
            update_mixer_controls(editor);
            
            if (editor->volume_scale) {
                gtk_widget_set_sensitive(editor->volume_scale, TRUE);
            }
            if (editor->pan_scale) {
                gtk_widget_set_sensitive(editor->pan_scale, TRUE);
            }
            
            gtk_widget_queue_draw(widget);
            
            return TRUE;
        }
        
        track_num++;
        iter = g_list_next(iter);
    }
    
    editor->selected_clip = NULL;
    if (editor->volume_scale) {
        gtk_widget_set_sensitive(editor->volume_scale, FALSE);
    }
    if (editor->pan_scale) {
        gtk_widget_set_sensitive(editor->pan_scale, FALSE);
    }
    update_mixer_controls(editor);
    gtk_widget_queue_draw(widget);
    
    return FALSE;
}

static gboolean draw_timeline(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    AudioEditor *editor = (AudioEditor *)user_data;
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    
    int width = allocation.width;
    int height = allocation.height;
    
    int max_duration = 1000000;
    GList *iter = editor->audio_clips;
    while (iter != NULL) {
        AudioClip *clip = (AudioClip *)iter->data;
        if (clip->start_pos + clip->duration > max_duration) {
            max_duration = clip->start_pos + clip->duration;
        }
        iter = g_list_next(iter);
    }
    if (max_duration < 1000000) max_duration = 1000000;
    
    int total_clips_width = 0;
    iter = editor->audio_clips;
    while (iter != NULL) {
        AudioClip *clip = (AudioClip *)iter->data;
        float clip_seconds = clip->duration / 100000.0f;
        int clip_w = (int)(clip_seconds * 80.0f);
        if (clip_w < 400) clip_w = 400;
        total_clips_width += clip_w + 20;
        iter = g_list_next(iter);
    }
    
    if (total_clips_width > width) {
        gtk_widget_set_size_request(editor->timeline_drawing_area, total_clips_width, 500);
    }
    
    cairo_pattern_t *bg_pattern = cairo_pattern_create_linear(0, 0, 0, height);
    cairo_pattern_add_color_stop_rgb(bg_pattern, 0.0, 0.08, 0.08, 0.12);
    cairo_pattern_add_color_stop_rgb(bg_pattern, 1.0, 0.05, 0.05, 0.08);
    cairo_set_source(cr, bg_pattern);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
    cairo_pattern_destroy(bg_pattern);
    
    cairo_set_source_rgba(cr, 0.2, 0.3, 0.5, 0.3);
    cairo_set_line_width(cr, 1);
    
    for (int i = 0; i < width; i += 50) {
        cairo_move_to(cr, i, 0);
        cairo_line_to(cr, i, height);
    }
    cairo_stroke(cr);
    
    int track_height = height / 8;
    if (track_height < 60) track_height = 60;
    int num_tracks = height / track_height;
    if (num_tracks < 1) num_tracks = 1;
    
    cairo_set_source_rgba(cr, 0.3, 0.5, 0.7, 0.4);
    cairo_set_line_width(cr, 1.5);
    for (int i = 1; i < num_tracks; i++) {
        cairo_move_to(cr, 0, i * track_height);
        cairo_line_to(cr, width, i * track_height);
    }
    cairo_stroke(cr);
    
    int track_num = 0;
    iter = editor->audio_clips;
    while (iter != NULL) {
        AudioClip *clip = (AudioClip *)iter->data;
        int track_y = (track_num % num_tracks) * track_height + 5;
        
        float clip_seconds = clip->duration / 100000.0f;
        int clip_width = (int)(clip_seconds * 80.0f);
        
        if (clip_width < 400) clip_width = 400;
        
        int max_clip_width = width * 2;
        if (clip_width > max_clip_width) {
            clip_width = max_clip_width;
        }
        
        int current_track = track_num % num_tracks;
        int clip_x = 0;
        GList *prev_iter = editor->audio_clips;
        int prev_track_idx = 0;
        while (prev_iter != iter) {
            AudioClip *prev_clip = (AudioClip *)prev_iter->data;
            int prev_track = prev_track_idx % num_tracks;
            if (prev_track == current_track) {
                float prev_seconds = prev_clip->duration / 100000.0f;
                int prev_width = (int)(prev_seconds * 80.0f);
                if (prev_width < 400) prev_width = 400;
                if (prev_width > width * 2) prev_width = width * 2;
                clip_x += prev_width + 20;
            }
            prev_track_idx++;
            prev_iter = g_list_next(prev_iter);
        }
        
        int clip_height = track_height - 10;
        
        float hue = (track_num % 6) / 6.0f;
        float r, g, b;
        if (hue < 0.17f) { r = 0.2f; g = 0.6f; b = 0.9f; }
        else if (hue < 0.33f) { r = 0.4f; g = 0.8f; b = 0.3f; }
        else if (hue < 0.5f) { r = 0.9f; g = 0.5f; b = 0.2f; }
        else if (hue < 0.67f) { r = 0.9f; g = 0.3f; b = 0.6f; }
        else if (hue < 0.83f) { r = 0.7f; g = 0.4f; b = 0.9f; }
        else { r = 0.3f; g = 0.8f; b = 0.9f; }
        
        cairo_pattern_t *clip_pattern = cairo_pattern_create_linear(clip_x, track_y, clip_x, track_y + clip_height);
        cairo_pattern_add_color_stop_rgb(clip_pattern, 0.0, r * 0.25, g * 0.25, b * 0.25);
        cairo_pattern_add_color_stop_rgb(clip_pattern, 1.0, r * 0.15, g * 0.15, b * 0.15);
        cairo_set_source(cr, clip_pattern);
        cairo_rectangle(cr, clip_x, track_y, clip_width, clip_height);
        cairo_fill(cr);
        cairo_pattern_destroy(clip_pattern);
        
        if (editor->selected_clip == clip) {
            cairo_set_source_rgb(cr, 1.0, 0.9, 0.3);
            cairo_set_line_width(cr, 4);
        } else {
            cairo_set_source_rgb(cr, r, g, b);
            cairo_set_line_width(cr, 2.5);
        }
        cairo_rectangle(cr, clip_x, track_y, clip_width, clip_height);
        cairo_stroke(cr);
        
        int text_area_height = 25;
        int waveform_area_y = track_y + text_area_height;
        int waveform_area_height = clip_height - text_area_height - 5;
        
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.5);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 11);
        const char *filename = strrchr(clip->filename, '/');
        if (!filename) filename = strrchr(clip->filename, '\\');
        if (!filename) filename = clip->filename;
        else filename++;
        cairo_move_to(cr, clip_x + 6, track_y + 16);
        cairo_show_text(cr, filename);
        
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_move_to(cr, clip_x + 5, track_y + 15);
        cairo_show_text(cr, filename);
        
        char info[50];
        snprintf(info, sizeof(info), "V:%.1f P:%.1f", clip->volume, clip->pan);
        cairo_set_font_size(cr, 9);
        cairo_set_source_rgb(cr, r * 0.8, g * 0.8, b * 0.8);
        cairo_move_to(cr, clip_x + 5, track_y + 25);
        cairo_show_text(cr, info);
        
        int16_t *waveform_samples = NULL;
        size_t waveform_count = 0;
        int max_waveform_samples = clip_width * 4;
        if (read_wav_samples(clip->filename, &waveform_samples, &waveform_count, max_waveform_samples) == 0 && waveform_count > 0) {
            if (waveform_area_height > 15) {
                cairo_set_source_rgb(cr, 0.2, 0.9, 0.4);
                cairo_set_line_width(cr, 1.5);
                
                int samples_per_pixel = waveform_count / clip_width;
                if (samples_per_pixel < 1) samples_per_pixel = 1;
                
                int center_y = waveform_area_y + waveform_area_height / 2;
                
                cairo_set_source_rgba(cr, 0.4, 0.4, 0.5, 0.3);
                cairo_set_line_width(cr, 0.5);
                cairo_move_to(cr, clip_x, center_y);
                cairo_line_to(cr, clip_x + clip_width, center_y);
                cairo_stroke(cr);
                
                cairo_set_source_rgb(cr, r * 0.7, g * 0.7, b * 0.7);
                cairo_set_line_width(cr, 2.0);
                
                int16_t max_abs = 0;
                for (size_t i = 0; i < waveform_count; i++) {
                    int16_t abs_val = waveform_samples[i] < 0 ? -waveform_samples[i] : waveform_samples[i];
                    if (abs_val > max_abs) max_abs = abs_val;
                }
                if (max_abs == 0) max_abs = 1;
                
                for (int x = 0; x < clip_width && x * samples_per_pixel < (int)waveform_count; x++) {
                    int sample_idx = x * samples_per_pixel;
                    if (sample_idx >= (int)waveform_count) break;
                    
                    int16_t min_val = waveform_samples[sample_idx];
                    int16_t max_val = waveform_samples[sample_idx];
                    int end_idx = sample_idx + samples_per_pixel;
                    if (end_idx > (int)waveform_count) end_idx = waveform_count;
                    
                    for (int i = sample_idx; i < end_idx; i++) {
                        if (waveform_samples[i] < min_val) min_val = waveform_samples[i];
                        if (waveform_samples[i] > max_val) max_val = waveform_samples[i];
                    }
                    
                    float min_norm = (float)min_val / (float)max_abs;
                    float max_norm = (float)max_val / (float)max_abs;
                    
                    int y1 = center_y + (int)(min_norm * waveform_area_height * 0.45f);
                    int y2 = center_y + (int)(max_norm * waveform_area_height * 0.45f);
                    
                    if (y1 == y2) {
                        y1 = center_y - 1;
                        y2 = center_y + 1;
                    }
                    
                    cairo_move_to(cr, clip_x + x, y1);
                    cairo_line_to(cr, clip_x + x, y2);
                }
                cairo_stroke(cr);
                
                free(waveform_samples);
            }
        } else {
            cairo_set_source_rgba(cr, 0.6, 0.6, 0.6, 0.7);
            cairo_set_font_size(cr, 10);
            cairo_move_to(cr, clip_x + 5, waveform_area_y + waveform_area_height / 2);
            cairo_show_text(cr, "⏳ Carregando waveform...");
        }
        
        track_num++;
        iter = g_list_next(iter);
    }
    
    if (editor->playing || editor->current_position > 0) {
        int cursor_x = (editor->current_position * width) / max_duration;
        if (cursor_x >= 0 && cursor_x < width) {
            cairo_set_source_rgba(cr, 1.0, 0.8, 0.0, 0.3);
            cairo_set_line_width(cr, 4);
            cairo_move_to(cr, cursor_x, 0);
            cairo_line_to(cr, cursor_x, height);
            cairo_stroke(cr);
            
            cairo_set_source_rgb(cr, 1.0, 0.7, 0.2);
            cairo_set_line_width(cr, 3);
            cairo_move_to(cr, cursor_x, 0);
            cairo_line_to(cr, cursor_x, height);
            cairo_stroke(cr);
            
            cairo_set_source_rgb(cr, 1.0, 1.0, 0.5);
            cairo_set_line_width(cr, 1);
            cairo_move_to(cr, cursor_x, 0);
            cairo_line_to(cr, cursor_x, height);
            cairo_stroke(cr);
        }
    }
    
    return FALSE;
}

GtkWidget* create_toolbar(AudioEditor *editor) {
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(toolbar, 10);
    gtk_widget_set_margin_end(toolbar, 10);
    gtk_widget_set_margin_top(toolbar, 10);
    gtk_widget_set_margin_bottom(toolbar, 10);
    
    GtkWidget *open_btn = gtk_button_new_with_label("📁 Abrir Áudio");
    g_signal_connect(open_btn, "clicked", G_CALLBACK(on_open_file), editor);
    gtk_box_pack_start(GTK_BOX(toolbar), open_btn, FALSE, FALSE, 0);
    
    GtkWidget *remove_btn = gtk_button_new_with_label("🗑️ Remover");
    g_signal_connect(remove_btn, "clicked", G_CALLBACK(on_remove_audio), editor);
    gtk_box_pack_start(GTK_BOX(toolbar), remove_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(toolbar), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 5);
    
    GtkWidget *play_btn = gtk_button_new_with_label("▶️ Reproduzir");
    g_signal_connect(play_btn, "clicked", G_CALLBACK(on_play), editor);
    gtk_box_pack_start(GTK_BOX(toolbar), play_btn, FALSE, FALSE, 0);
    
    GtkWidget *stop_btn = gtk_button_new_with_label("⏹️ Parar");
    g_signal_connect(stop_btn, "clicked", G_CALLBACK(on_stop), editor);
    gtk_box_pack_start(GTK_BOX(toolbar), stop_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(toolbar), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 5);
    
    GtkWidget *export_btn = gtk_button_new_with_label("💾 Exportar WAV");
    g_signal_connect(export_btn, "clicked", G_CALLBACK(on_export), editor);
    gtk_box_pack_start(GTK_BOX(toolbar), export_btn, FALSE, FALSE, 0);
    
    return toolbar;
}

static void update_info_label(AudioEditor *editor) {
    if (!editor->transport_controls) return;
    
    GList *children = gtk_container_get_children(GTK_CONTAINER(editor->transport_controls));
    if (!children) return;
    
    GList *iter = g_list_last(children);
    if (iter) {
        GtkWidget *info_label = GTK_WIDGET(iter->data);
        int clip_count = g_list_length(editor->audio_clips);
        char info_text[100];
        snprintf(info_text, sizeof(info_text), "🎵 %d Hz • 16-bit • %d arquivo(s)", 
                 editor->sample_rate, clip_count);
        gtk_label_set_text(GTK_LABEL(info_label), info_text);
    }
    g_list_free(children);
}

GtkWidget* create_transport_controls(AudioEditor *editor) {
    GtkWidget *transport = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_widget_set_margin_start(transport, 15);
    gtk_widget_set_margin_end(transport, 15);
    gtk_widget_set_margin_top(transport, 10);
    gtk_widget_set_margin_bottom(transport, 10);
    
    GtkWidget *position_label = gtk_label_new("⏱️ 00:00");
    gtk_box_pack_start(GTK_BOX(transport), position_label, FALSE, FALSE, 0);
    
    GtkWidget *progress_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_scale_set_draw_value(GTK_SCALE(progress_scale), FALSE);
    gtk_widget_set_hexpand(progress_scale, TRUE);
    gtk_widget_set_margin_start(progress_scale, 10);
    gtk_widget_set_margin_end(progress_scale, 10);
    gtk_box_pack_start(GTK_BOX(transport), progress_scale, TRUE, TRUE, 0);
    
    GtkWidget *info_label = gtk_label_new("🎵 44100 Hz • 16-bit • 0 arquivo(s)");
    gtk_box_pack_start(GTK_BOX(transport), info_label, FALSE, FALSE, 0);
    
    return transport;
}

GtkWidget* create_timeline_area(AudioEditor *editor) {
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    editor->timeline_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(editor->timeline_drawing_area, 800, 500);
    
    g_signal_connect(editor->timeline_drawing_area, "draw", 
                    G_CALLBACK(draw_timeline), editor);
    
    gtk_widget_add_events(editor->timeline_drawing_area, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(editor->timeline_drawing_area, "button-press-event",
                    G_CALLBACK(on_timeline_button_press), editor);
    
    gtk_container_add(GTK_CONTAINER(scrolled_window), editor->timeline_drawing_area);
    
    return scrolled_window;
}

static void on_volume_changed(GtkRange *range, gpointer user_data) {
    AudioEditor *editor = (AudioEditor *)user_data;
    float volume = (float)gtk_range_get_value(range);
    
    if (editor->selected_clip) {
        editor->selected_clip->volume = volume;
        printf("🔊 Volume do clip selecionado ajustado para: %.1f\n", volume);
        
        if (editor->timeline_drawing_area) {
            gtk_widget_queue_draw(editor->timeline_drawing_area);
        }
    } else {
        printf("⚠️ Nenhum clip selecionado. Clique em um clip na timeline para editá-lo.\n");
    }
}

static void on_pan_changed(GtkRange *range, gpointer user_data) {
    AudioEditor *editor = (AudioEditor *)user_data;
    float pan = (float)gtk_range_get_value(range);
    
    if (editor->selected_clip) {
        editor->selected_clip->pan = pan;
        printf("🎚️ Pan do clip selecionado ajustado para: %.1f\n", pan);
        
        if (editor->timeline_drawing_area) {
            gtk_widget_queue_draw(editor->timeline_drawing_area);
        }
    } else {
        printf("⚠️ Nenhum clip selecionado. Clique em um clip na timeline para editá-lo.\n");
    }
}

static void update_mixer_controls(AudioEditor *editor) {
    if (editor->selected_clip) {
        if (editor->volume_scale) {
            g_signal_handlers_block_by_func(editor->volume_scale, on_volume_changed, editor);
            gtk_range_set_value(GTK_RANGE(editor->volume_scale), editor->selected_clip->volume);
            g_signal_handlers_unblock_by_func(editor->volume_scale, on_volume_changed, editor);
        }
        if (editor->pan_scale) {
            g_signal_handlers_block_by_func(editor->pan_scale, on_pan_changed, editor);
            gtk_range_set_value(GTK_RANGE(editor->pan_scale), editor->selected_clip->pan);
            g_signal_handlers_unblock_by_func(editor->pan_scale, on_pan_changed, editor);
        }
    } else {
        if (editor->volume_scale) {
            g_signal_handlers_block_by_func(editor->volume_scale, on_volume_changed, editor);
            gtk_range_set_value(GTK_RANGE(editor->volume_scale), 1.0);
            g_signal_handlers_unblock_by_func(editor->volume_scale, on_volume_changed, editor);
        }
        if (editor->pan_scale) {
            g_signal_handlers_block_by_func(editor->pan_scale, on_pan_changed, editor);
            gtk_range_set_value(GTK_RANGE(editor->pan_scale), 0.0);
            g_signal_handlers_unblock_by_func(editor->pan_scale, on_pan_changed, editor);
        }
    }
}

GtkWidget* create_mixer_panel(AudioEditor *editor) {
    GtkWidget *mixer_frame = gtk_frame_new("🎛️ Mixer");
    GtkWidget *mixer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_container_set_border_width(GTK_CONTAINER(mixer_frame), 10);
    gtk_container_add(GTK_CONTAINER(mixer_frame), mixer_box);
    
    GtkWidget *info_label = gtk_label_new("🎯 Selecione um clip na timeline para editar");
    gtk_box_pack_start(GTK_BOX(mixer_box), info_label, FALSE, FALSE, 10);
    
    GtkWidget *volume_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *volume_label = gtk_label_new("🔊 Volume");
    editor->volume_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 2, 0.1);
    gtk_scale_set_draw_value(GTK_SCALE(editor->volume_scale), TRUE);
    gtk_range_set_value(GTK_RANGE(editor->volume_scale), 1.0);
    gtk_widget_set_size_request(editor->volume_scale, 150, -1);
    gtk_widget_set_sensitive(editor->volume_scale, FALSE);
    g_signal_connect(editor->volume_scale, "value-changed", G_CALLBACK(on_volume_changed), editor);
    
    gtk_box_pack_start(GTK_BOX(volume_box), volume_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(volume_box), editor->volume_scale, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mixer_box), volume_box, FALSE, FALSE, 0);
    
    GtkWidget *pan_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *pan_label = gtk_label_new("🎚️ Pan (L/R)");
    editor->pan_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -1, 1, 0.1);
    gtk_scale_set_draw_value(GTK_SCALE(editor->pan_scale), TRUE);
    gtk_range_set_value(GTK_RANGE(editor->pan_scale), 0.0);
    gtk_widget_set_size_request(editor->pan_scale, 150, -1);
    gtk_widget_set_sensitive(editor->pan_scale, FALSE);
    g_signal_connect(editor->pan_scale, "value-changed", G_CALLBACK(on_pan_changed), editor);
    
    gtk_box_pack_start(GTK_BOX(pan_box), pan_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pan_box), editor->pan_scale, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mixer_box), pan_box, FALSE, FALSE, 0);
    
    return mixer_frame;
}

void launch_audio_editor(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const char *css = 
        "window {"
        "  background: linear-gradient(180deg, #1a1a2e 0%, #16213e 100%);"
        "}"
        "button {"
        "  background: linear-gradient(180deg, #4a90e2 0%, #357abd 100%);"
        "  color: white;"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 8px 16px;"
        "  font-weight: bold;"
        "  box-shadow: 0 2px 4px rgba(0,0,0,0.3);"
        "  transition: all 0.2s;"
        "}"
        "button:hover {"
        "  background: linear-gradient(180deg, #5ba0f2 0%, #4590cd 100%);"
        "  box-shadow: 0 4px 8px rgba(74, 144, 226, 0.4);"
        "  transform: translateY(-1px);"
        "}"
        "button:active {"
        "  background: linear-gradient(180deg, #357abd 0%, #2a6aa8 100%);"
        "  transform: translateY(0px);"
        "}"
        "button[label*='Abrir'] {"
        "  background: linear-gradient(180deg, #2ecc71 0%, #27ae60 100%);"
        "}"
        "button[label*='Abrir']:hover {"
        "  background: linear-gradient(180deg, #3ecc81 0%, #2eae70 100%);"
        "}"
        "button[label*='Remover'] {"
        "  background: linear-gradient(180deg, #e74c3c 0%, #c0392b 100%);"
        "}"
        "button[label*='Remover']:hover {"
        "  background: linear-gradient(180deg, #f75c4c 0%, #d0493b 100%);"
        "}"
        "button[label*='Reproduzir'] {"
        "  background: linear-gradient(180deg, #f39c12 0%, #e67e22 100%);"
        "}"
        "button[label*='Reproduzir']:hover {"
        "  background: linear-gradient(180deg, #f4ac22 0%, #f68e32 100%);"
        "}"
        "button[label*='Parar'] {"
        "  background: linear-gradient(180deg, #95a5a6 0%, #7f8c8d 100%);"
        "}"
        "button[label*='Parar']:hover {"
        "  background: linear-gradient(180deg, #a5b5b6 0%, #8f9c9d 100%);"
        "}"
        "button[label*='Exportar'] {"
        "  background: linear-gradient(180deg, #9b59b6 0%, #8e44ad 100%);"
        "}"
        "button[label*='Exportar']:hover {"
        "  background: linear-gradient(180deg, #ab69c6 0%, #9e54bd 100%);"
        "}"
        "frame {"
        "  background: rgba(30, 30, 45, 0.8);"
        "  border: 2px solid #4a90e2;"
        "  border-radius: 10px;"
        "  padding: 10px;"
        "}"
        "label {"
        "  color: #ecf0f1;"
        "  font-weight: 500;"
        "}"
        "scale {"
        "  color: #4a90e2;"
        "}"
        "scale trough {"
        "  background: rgba(50, 50, 70, 0.8);"
        "  border-radius: 5px;"
        "  min-height: 8px;"
        "}"
        "scale highlight {"
        "  background: linear-gradient(90deg, #4a90e2 0%, #9b59b6 100%);"
        "  border-radius: 5px;"
        "}"
        "scale slider {"
        "  background: linear-gradient(180deg, #ecf0f1 0%, #bdc3c7 100%);"
        "  border: 2px solid #4a90e2;"
        "  border-radius: 50%;"
        "  min-width: 16px;"
        "  min-height: 16px;"
        "  box-shadow: 0 2px 4px rgba(0,0,0,0.3);"
        "}"
        "statusbar {"
        "  background: linear-gradient(90deg, #2c3e50 0%, #34495e 100%);"
        "  color: #ecf0f1;"
        "  padding: 5px;"
        "}"
        "separator {"
        "  background: rgba(74, 144, 226, 0.5);"
        "  min-width: 2px;"
        "}"
        "box {"
        "  background: transparent;"
        "}";
    
    gtk_css_provider_load_from_data(css_provider, css, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(css_provider);
    
    #ifdef USE_SDL2
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        printf("❌ Erro ao inicializar SDL: %s\n", SDL_GetError());
        return;
    }
    #else
    printf("ℹ️ SDL2 não disponível. Reprodução de áudio desabilitada.\n");
    printf("💡 Para habilitar reprodução, instale: pacman -S mingw-w64-ucrt-x86_64-SDL2\n");
    #endif
    
    AudioEditor *editor = g_malloc0(sizeof(AudioEditor));
    editor->sample_rate = 44100;
    editor->current_position = 0;
    editor->playing = 0;
    editor->audio_clips = NULL;
    editor->selected_clip = NULL;
    editor->volume_scale = NULL;
    editor->pan_scale = NULL;
    editor->zoom_level = 1.0f;
    editor->view_start = 0;
    #ifdef USE_SDL2
    editor->audio_device = 0;
    #endif
    editor->audio_buffer = NULL;
    editor->audio_buffer_size = 0;
    editor->audio_buffer_pos = 0;
    editor->audio_playing = 0;
    g_editor = editor;
    
    editor->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(editor->window), "🎵 Studio WAV - Editor de Áudio Profissional");
    gtk_window_set_default_size(GTK_WINDOW(editor->window), 1200, 700);
    gtk_window_set_position(GTK_WINDOW(editor->window), GTK_WIN_POS_CENTER);
    
    GtkStyleContext *context = gtk_widget_get_style_context(editor->window);
    gtk_style_context_add_class(context, "window");
    
    editor->main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(editor->window), editor->main_box);
    
    editor->toolbar = create_toolbar(editor);
    gtk_box_pack_start(GTK_BOX(editor->main_box), editor->toolbar, FALSE, FALSE, 0);
    
    editor->timeline_container = create_timeline_area(editor);
    gtk_box_pack_start(GTK_BOX(editor->main_box), editor->timeline_container, TRUE, TRUE, 0);
    
    editor->transport_controls = create_transport_controls(editor);
    gtk_box_pack_start(GTK_BOX(editor->main_box), editor->transport_controls, FALSE, FALSE, 0);
    
    editor->mixer_panel = create_mixer_panel(editor);
    gtk_box_pack_start(GTK_BOX(editor->main_box), editor->mixer_panel, FALSE, FALSE, 0);
    
    editor->status_bar = gtk_statusbar_new();
    gtk_statusbar_push(GTK_STATUSBAR(editor->status_bar), 0, "🎵 Studio WAV Editor - Pronto para criar sua mixagem!");
    gtk_box_pack_start(GTK_BOX(editor->main_box), editor->status_bar, FALSE, FALSE, 0);
    
    g_signal_connect(editor->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    gtk_widget_show_all(editor->window);
    
    gtk_main();
    
    #ifdef USE_SDL2
    if (editor->audio_device != 0) {
        SDL_PauseAudioDevice(editor->audio_device, 1);
        SDL_CloseAudioDevice(editor->audio_device);
    }
    
    if (editor->audio_buffer) {
        SDL_FreeWAV((Uint8 *)editor->audio_buffer);
    }
    #else
    if (editor->audio_buffer) {
        free(editor->audio_buffer);
    }
    #endif
    
    #ifdef _WIN32
    remove("playback_temp.wav");
    #else
    remove("/tmp/playback_temp.wav");
    #endif
    
    GList *iter = editor->audio_clips;
    while (iter != NULL) {
        AudioClip *clip = (AudioClip *)iter->data;
        g_free(clip->filename);
        g_free(clip);
        iter = g_list_next(iter);
    }
    g_list_free(editor->audio_clips);
    g_free(editor);
    
    #ifdef USE_SDL2
    SDL_Quit();
    #endif
}

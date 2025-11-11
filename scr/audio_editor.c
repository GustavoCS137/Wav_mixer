#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include "audio_editor.h"
#include "wav_reader.h"

static void on_open_file(GtkButton *button, gpointer user_data);
static void on_play(GtkButton *button, gpointer user_data);
static void on_stop(GtkButton *button, gpointer user_data);
static void on_export(GtkButton *button, gpointer user_data);
static gboolean draw_timeline(GtkWidget *widget, cairo_t *cr, gpointer user_data);

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
        
        // Adicionar arquivo à lista
        AudioClip *clip = g_malloc0(sizeof(AudioClip));
        clip->filename = g_strdup(filename);
        clip->volume = 1.0f;
        clip->pan = 0.0f;
        clip->start_pos = 0;
        clip->duration = 1000000;
        clip->muted = 0;
        clip->solo = 0;
        
        editor->audio_clips = g_list_append(editor->audio_clips, clip);
        
        printf("✅ Arquivo adicionado: %s\n", filename);
        
        if (editor->timeline_drawing_area) {
            gtk_widget_queue_draw(editor->timeline_drawing_area);
        }
        
        g_free(filename);
    }
    
    gtk_widget_destroy(dialog);
}

static void on_play(GtkButton *button, gpointer user_data) {
    AudioEditor *editor = (AudioEditor *)user_data;
    editor->playing = 1;
    printf("▶️ Reproduzindo...\n");
    
    // Simular avanço do cursor (em uma aplicação real, isso seria com threads)
    editor->current_position = 0;
    while (editor->playing && editor->current_position < 1000000) {
        editor->current_position += 10000;
        if (editor->timeline_drawing_area) {
            gtk_widget_queue_draw(editor->timeline_drawing_area);
        }
        // Pequena pausa para animação
        usleep(50000);
    }
}

static void on_stop(GtkButton *button, gpointer user_data) {
    AudioEditor *editor = (AudioEditor *)user_data;
    editor->playing = 0;
    editor->current_position = 0;
    printf("⏹️ Parado\n");
    
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
        
        int i = 0;
        GList *iter = editor->audio_clips;
        while (iter != NULL) {
            AudioClip *clip = (AudioClip *)iter->data;
            input_files[i] = clip->filename;
            volumes[i] = clip->volume;
            printf("📁 %s (volume: %.1f)\n", input_files[i], volumes[i]);
            i++;
            iter = g_list_next(iter);
        }
        
        if (mix_wav_files(filename, input_files, volumes, file_count) == 0) {
            printf("✅ Exportação concluída: %s\n", filename);
        } else {
            printf("❌ Erro na exportação!\n");
        }
        
        free(input_files);
        free(volumes);
        g_free(filename);
    }
    
    gtk_widget_destroy(dialog);
}

static gboolean draw_timeline(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    AudioEditor *editor = (AudioEditor *)user_data;
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    
    int width = allocation.width;
    int height = allocation.height;
    
    // Fundo
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
    
    // Grade
    cairo_set_source_rgb(cr, 0.3, 0.3, 0.3);
    cairo_set_line_width(cr, 1);
    
    for (int i = 0; i < width; i += 50) {
        cairo_move_to(cr, i, 0);
        cairo_line_to(cr, i, height);
    }
    cairo_stroke(cr);
    
    // Cursor de reprodução
    if (editor->playing || editor->current_position > 0) {
        cairo_set_source_rgb(cr, 1.0, 0.6, 0.0);
        cairo_set_line_width(cr, 2);
        int cursor_x = (editor->current_position * width) / 1000000;
        cairo_move_to(cr, cursor_x, 0);
        cairo_line_to(cr, cursor_x, height);
        cairo_stroke(cr);
    }
    
    return FALSE;
}

GtkWidget* create_toolbar(AudioEditor *editor) {
    GtkWidget *toolbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_margin_start(toolbar, 5);
    gtk_widget_set_margin_end(toolbar, 5);
    gtk_widget_set_margin_top(toolbar, 5);
    gtk_widget_set_margin_bottom(toolbar, 5);
    
    GtkWidget *open_btn = gtk_button_new_with_label("📁 Abrir Áudio");
    g_signal_connect(open_btn, "clicked", G_CALLBACK(on_open_file), editor);
    gtk_box_pack_start(GTK_BOX(toolbar), open_btn, FALSE, FALSE, 0);
    
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

GtkWidget* create_transport_controls(AudioEditor *editor) {
    GtkWidget *transport = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_margin_start(transport, 10);
    gtk_widget_set_margin_end(transport, 10);
    gtk_widget_set_margin_top(transport, 5);
    gtk_widget_set_margin_bottom(transport, 5);
    
    GtkWidget *position_label = gtk_label_new("00:00:00");
    gtk_box_pack_start(GTK_BOX(transport), position_label, FALSE, FALSE, 0);
    
    GtkWidget *progress_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_scale_set_draw_value(GTK_SCALE(progress_scale), FALSE);
    gtk_widget_set_hexpand(progress_scale, TRUE);
    gtk_box_pack_start(GTK_BOX(transport), progress_scale, TRUE, TRUE, 0);
    
    GtkWidget *info_label = gtk_label_new("44100 Hz • 16-bit • Stereo");
    gtk_box_pack_start(GTK_BOX(transport), info_label, FALSE, FALSE, 0);
    
    return transport;
}

GtkWidget* create_timeline_area(AudioEditor *editor) {
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    editor->timeline_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(editor->timeline_drawing_area, 800, 300);
    
    g_signal_connect(editor->timeline_drawing_area, "draw", 
                    G_CALLBACK(draw_timeline), editor);
    
    gtk_container_add(GTK_CONTAINER(scrolled_window), editor->timeline_drawing_area);
    
    return scrolled_window;
}

GtkWidget* create_mixer_panel(AudioEditor *editor) {
    GtkWidget *mixer_frame = gtk_frame_new("🎛️ Mixer");
    GtkWidget *mixer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(mixer_frame), 5);
    gtk_container_add(GTK_CONTAINER(mixer_frame), mixer_box);
    
    // Controles de volume
    GtkWidget *volume_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *volume_label = gtk_label_new("Volume");
    GtkWidget *volume_scale = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0, 2, 0.1);
    gtk_scale_set_draw_value(GTK_SCALE(volume_scale), TRUE);
    gtk_range_set_value(GTK_RANGE(volume_scale), 1.0);
    
    gtk_box_pack_start(GTK_BOX(volume_box), volume_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(volume_box), volume_scale, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(mixer_box), volume_box, FALSE, FALSE, 0);
    
    // Controles de pan
    GtkWidget *pan_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *pan_label = gtk_label_new("Pan");
    GtkWidget *pan_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -1, 1, 0.1);
    gtk_scale_set_draw_value(GTK_SCALE(pan_scale), TRUE);
    gtk_range_set_value(GTK_RANGE(pan_scale), 0.0);
    
    gtk_box_pack_start(GTK_BOX(pan_box), pan_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pan_box), pan_scale, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(mixer_box), pan_box, TRUE, TRUE, 0);
    
    return mixer_frame;
}

void launch_audio_editor(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    AudioEditor *editor = g_malloc0(sizeof(AudioEditor));
    editor->sample_rate = 44100;
    editor->current_position = 0;
    editor->playing = 0;
    editor->audio_clips = NULL;
    
    // Criar janela principal
    editor->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(editor->window), "🎵 Studio WAV - Editor de Áudio Profissional");
    gtk_window_set_default_size(GTK_WINDOW(editor->window), 1000, 600);
    gtk_window_set_position(GTK_WINDOW(editor->window), GTK_WIN_POS_CENTER);
    
    // Layout principal
    editor->main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(editor->window), editor->main_box);
    
    // Adicionar componentes
    editor->toolbar = create_toolbar(editor);
    gtk_box_pack_start(GTK_BOX(editor->main_box), editor->toolbar, FALSE, FALSE, 0);
    
    editor->timeline_container = create_timeline_area(editor);
    gtk_box_pack_start(GTK_BOX(editor->main_box), editor->timeline_container, TRUE, TRUE, 0);
    
    editor->transport_controls = create_transport_controls(editor);
    gtk_box_pack_start(GTK_BOX(editor->main_box), editor->transport_controls, FALSE, FALSE, 0);
    
    editor->mixer_panel = create_mixer_panel(editor);
    gtk_box_pack_start(GTK_BOX(editor->main_box), editor->mixer_panel, FALSE, FALSE, 0);
    
    // Barra de status
    editor->status_bar = gtk_statusbar_new();
    gtk_statusbar_push(GTK_STATUSBAR(editor->status_bar), 0, "🎵 Studio WAV Editor - Pronto para criar sua mixagem!");
    gtk_box_pack_start(GTK_BOX(editor->main_box), editor->status_bar, FALSE, FALSE, 0);
    
    // Conectar sinal de fechamento
    g_signal_connect(editor->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    // Mostrar tudo
    gtk_widget_show_all(editor->window);
    
    // Loop principal
    gtk_main();
    
    // Limpeza
    GList *iter = editor->audio_clips;
    while (iter != NULL) {
        AudioClip *clip = (AudioClip *)iter->data;
        g_free(clip->filename);
        g_free(clip);
        iter = g_list_next(iter);
    }
    g_list_free(editor->audio_clips);
    g_free(editor);
}

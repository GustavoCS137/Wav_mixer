#ifndef AUDIO_EDITOR_H
#define AUDIO_EDITOR_H

#include <gtk/gtk.h>

typedef struct {
    char *filename;
    float volume;
    float pan;
    int start_pos;
    int duration;
    int muted;
    int solo;
} AudioClip;

typedef struct {
    GtkWidget *window;
    GtkWidget *main_box;
    GtkWidget *toolbar;
    GtkWidget *timeline_container;
    GtkWidget *transport_controls;
    GtkWidget *status_bar;
    GtkWidget *mixer_panel;
    
    GList *audio_clips;
    int sample_rate;
    int current_position;
    int playing;
    
    GtkWidget *timeline_drawing_area;
    
} AudioEditor;

void launch_audio_editor(int argc, char *argv[]);

#endif

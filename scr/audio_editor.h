#ifndef AUDIO_EDITOR_H
#define AUDIO_EDITOR_H

#include <gtk/gtk.h>

#ifdef USE_SDL2
#include <SDL2/SDL.h>
#endif

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
    AudioClip *selected_clip;
    GtkWidget *volume_scale;
    GtkWidget *pan_scale;
    int sample_rate;
    int current_position;
    int playing;
    
    GtkWidget *timeline_drawing_area;
    
    float zoom_level;
    int view_start;
    #ifdef USE_SDL2
    SDL_AudioDeviceID audio_device;
    SDL_AudioSpec audio_spec;
    #endif
    int16_t *audio_buffer;
    size_t audio_buffer_size;
    size_t audio_buffer_pos;
    int audio_playing;
    
} AudioEditor;

void launch_audio_editor(int argc, char *argv[]);

#endif

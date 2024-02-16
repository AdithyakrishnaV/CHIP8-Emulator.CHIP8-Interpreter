#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL2/SDL_render.h>

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
}sdl_t;

typedef struct {
    uint32_t window_width;//SDL WINDOW WIDTH
    uint32_t window_height;//SDL WINDOW HEIGHT  
    uint32_t fg_color; // foreground color RGBA8888
    uint32_t bg_color; //foreground color RGBA8888
    uint32_t scale_factor; //Scale a CHIP8 pixel by e.g. 20x will be a 20x larger window
}config_t;

//initialize SDL
bool init_sdl(sdl_t *sdl, const config_t config){
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)>0){
        SDL_Log("Could not initialize! SDL Error: %s\n", SDL_GetError());
        return false;
    }

    sdl->window = SDL_CreateWindow("Chip8 Emulator", SDL_WINDOWPOS_CENTERED, 
                                                    SDL_WINDOWPOS_CENTERED, 
                                                    config.window_width * config.scale_factor, 
                                                    config.window_height * config.scale_factor, 
                                                    0);
                                                    //title, x, y, w, h, flags

    if(!sdl->window){
        SDL_Log("Could not create SDL window %s\n", SDL_GetError());
        return false;
    }

    sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);
    if(!sdl->renderer){
        SDL_Log("Could not render SDL window %s\n", SDL_GetError());
        return false;
    }

    return true; //success
}

//Set up initial emulator configuration form passed in arguments
bool set_config_from_args(config_t *config, const int argc,  char **argv){
    //set defaults
    *config = (config_t){
        .window_width = 64, //CHIP8 original X resolution
        .window_height = 32, //CHIP8 original Y resolution
        .fg_color = 0xFFFFFFFF, //White
        .bg_color = 0xFFFF00FF, // Yellow
        .scale_factor = 20, //Default resolution will be (64x20)x(32x20) 1280x640
    };

    //override defaults from passed in arguments
    for(int i=1; i < argc; i++){
        (void)argv[i];//prevent compiler errors form  unused variables argc/argv
    }

    return true;
}

//final cleanup function
void final_cleanup(const sdl_t sdl){
    SDL_DestroyRenderer(sdl.renderer);
    SDL_DestroyWindow(sdl.window);
    SDL_Quit();//Clean up or Shutdown all initialized SDL subsystems.
}

//Clear screen / SDL window to background colour
void clear_screen(const sdl_t sdl, const config_t config){
    const uint8_t r = (config.bg_color >> 24) & 0xFF;
    const uint8_t g = (config.bg_color >> 16) & 0xFF;
    const uint8_t b = (config.bg_color >> 8) & 0xFF;
    const uint8_t a = (config.bg_color >> 0) & 0xFF;

    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
    SDL_RenderClear(sdl.renderer);
}

//update window with any changes
void update_screen(const sdl_t sdl){
    SDL_RenderPresent(sdl.renderer);
}

int main(int argc, char **argv) {

    //Initialize emulator configuration 
    config_t config = {0};
    if (!set_config_from_args(&config, argc, argv)) exit(EXIT_FAILURE);

    //Initialize SDL
    sdl_t sdl ={0};
    if(!init_sdl(&sdl, config)) exit(EXIT_FAILURE);

    //Initial screen clear to background colour
    clear_screen(sdl, config);

    //Main emulator loop
    while(true){
        //Get_time()
        //Emulate CHIP8 instruction
        //Get_time() elapsed since last  get_time();
        //Delay approx 60Hz/60fps (16.6ms)
        // SDL_Delay(16 - actual time delayed);
        SDL_Delay(16);
        //Update window with changes
        update_screen(sdl);
    }

    //Final cleanup
    final_cleanup(sdl);

    exit(EXIT_SUCCESS);
}


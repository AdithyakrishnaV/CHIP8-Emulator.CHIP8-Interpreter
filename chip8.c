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


//Emulator states
typedef enum{
    QUIT,
    RUNNING,
    PAUSED,
}emulator_state_t;

// CHIP8 instruction format
typedef struct {
    uint16_t opcode;
    uint16_t NNN;    // 12 bit address
    uint16_t NN;    // 8 bit constant
    uint8_t N;     // 4 bit constant
    uint8_t X;    // 4 bit register identifier
    uint8_t Y;   // 4 bit register identifier
}instruction_t;

//CHIP8 machine object
typedef struct {
    emulator_state_t state;
    uint8_t ram[4098];
    bool display[64*32]; //emulate original chip8 resolution pixels
    //uint8_t *display; //display = &ram[0XF00]; display[10]
    uint16_t stack[12]; //subroutine stack
    uint16_t *stack_ptr;
    uint8_t V[16];     //Data registers V0-VF
    uint16_t I; 
    uint16_t PC;       //program counter  
    uint8_t delay_timer;
    uint8_t sound_timer;
    bool keyboard[16];  // Hexadecimal keyboard 0x0-0xF
    const char *rom_name;  
    instruction_t inst;   // Currently executing instruction
}chip8_t;


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
        .bg_color = 0x000000FF, // Black
        .scale_factor = 20, //Default resolution will be (64x20)x(32x20) 1280x640
    };

    //override defaults from passed in arguments
    for(int i=1; i < argc; i++){
        (void)argv[i];//prevent compiler errors form  unused variables argc/argv
    }

    return true;
}

//Initialize CHIP8 machine
bool init_chip8(chip8_t *chip8, const char rom_name[]) {

    const uint32_t entry_point= 0x200; //Chip8 roms will be loaded to 0x200
    const uint8_t font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };
    //Load font 
    memcpy(&chip8->ram[0], font, sizeof(font));

    //Open rom file
    FILE *rom = fopen(rom_name, "rb");
    if (!rom) {
        SDL_Log("ROM file %s is invalid or does not exit\n", rom_name);
        return false;
    }

    //Get/check rom size
    fseek(rom, 0, SEEK_END);
    const size_t rom_size = ftell(rom);
    const size_t max_size = sizeof chip8->ram - entry_point;
    rewind(rom);

    if (rom_size > max_size) {
        SDL_Log("ROM file %s is too big! Rom size: %zu, Max size allowed: %zu\n", rom_name, rom_size, max_size);
        return false;
    }

    //Load ROM
    if (fread(&chip8->ram[entry_point], rom_size, 1, rom)!= 1){

        SDL_Log("Could not read ROM file %s CHIP8 into the memory\n", rom_name);
        return false;
    };

    fclose(rom);

    //Set chip8 defaults 
    chip8->state = RUNNING; //Default machine state to running
    chip8->PC = entry_point; //Start program counter at ROM entry point
    chip8->rom_name = rom_name;
    chip8->stack_ptr = &chip8->stack[0];


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
void update_screen(const sdl_t sdl, const config_t config, const chip8_t chip8){
    SDL_Rect rect = {.x=0, .y=0, .w=config.scale_factor, .h= config.scale_factor};


    const uint8_t fg_r = (config.fg_color >> 24) & 0xFF;
    const uint8_t fg_g = (config.fg_color >> 16) & 0xFF;
    const uint8_t fg_b = (config.fg_color >> 8) & 0xFF;
    const uint8_t fg_a = (config.fg_color >> 0) & 0xFF;

    const uint8_t bg_r = (config.bg_color >> 24) & 0xFF;
    const uint8_t bg_g = (config.bg_color >> 16) & 0xFF;
    const uint8_t bg_b = (config.bg_color >> 8) & 0xFF;
    const uint8_t bg_a = (config.bg_color >> 0) & 0xFF;

    for (uint32_t i = 0; i < sizeof chip8.display; i++  ){
        rect.x = (i % config.window_width) * config.scale_factor;
        rect.y = (i / config.window_width)  * config.scale_factor;

        if (chip8.display[i]){

            SDL_SetRenderDrawColor(sdl.renderer, fg_r, fg_g, fg_b, fg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        }else {
            SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        }
    }
    SDL_RenderPresent(sdl.renderer);
}

void handle_input(chip8_t *chip8) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type)
        {
        case SDL_QUIT:
            //Exit window; End program
            chip8->state = QUIT;// Will exit main emulator loop
            return;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym)
            {
            case SDLK_ESCAPE:
                //Escape key; Exit window & end program
                chip8->state = QUIT;
                return;

            case SDLK_SPACE:
                //space bar
                if (chip8->state == RUNNING) {
                    chip8->state = PAUSED; 
                    puts("======PAUSED======");
                }else{
                     chip8->state = RUNNING;
                }
                return;

            default:
                break;

            }
            break;
        
        case SDL_KEYUP:
            break;
        
        default:
            break;
        }
    }
}

//Emulate 1 CHIP8 instruction
void emulate_instruction(chip8_t *chip8, const config_t config) {
    // Get next opcode from ROM/ram
    chip8->inst.opcode = (chip8->ram[chip8->PC] << 8 ) | chip8->ram[chip8->PC+1];
    chip8->PC +=2; //Pre-increment program counter for next opcode

    //Fill out current instruction format
    chip8->inst.NNN = chip8->inst.opcode & 0x0FFF;
    chip8->inst.NN = chip8->inst.opcode & 0x0FF;
    chip8->inst.N = chip8->inst.opcode & 0x0F;
    chip8->inst.X = (chip8->inst.opcode >> 8)  & 0x0F;
    chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x0F;


    //Emulate opcode
    switch ((chip8->inst.opcode  >> 12) & 0x0F) 
    {
    case 0x00:
        if (chip8->inst.NN == 0xE0)//lowest 12 bits
        {
            //0x00E0: clear the screen
            memset(&chip8->display[0], false, sizeof chip8->display);
        }else if (chip8->inst.NN == 0xEE)
        {   //0x00EE: return from subroutine
            chip8->PC = *--chip8->stack_ptr;
        } 
        break;
    
    case 0x02:
        //0x2NNN: call subroutine at NNN
        *chip8->stack_ptr++ = chip8->PC;
        chip8->PC = chip8->inst.NNN;
    
    case 0x06: 
        // 0x6XNN: set register VX to NNN;
        chip8->V[chip8->inst.X] = chip8->inst.NN;

    case 0x0A:
        //0xANNN: set index register I to NNN
        chip8->I = chip8->inst.NNN;
        break;

    case 0x0D:
         uint8_t X_coord = chip8->V[chip8->inst.X] % config.window_width;
         uint8_t Y_coord = chip8->V[chip8->inst.Y] % config.window_height;
         const uint8_t orig_X = X_coord;


        chip8->V[0xF] = 0;// carry flag to 0 inialized

        for (uint8_t i = 0; i < chip8->inst.N; i++){
            const uint8_t sprite_data = chip8->ram[chip8->I = i];
            X_coord = orig_X; // Reset X for next row
            
            for (int8_t j = 7; j >= 0; j--){

                bool *pixel = &chip8->display[Y_coord*config.window_width + X_coord];
                const bool sprite_bit = (sprite_data & (1 << j));
                if ((sprite_bit & (1 << j)) && 
                    *pixel ){
                    
                    chip8->V[0xF] = 1;

                }

                *pixel ^= sprite_bit;

                if (++X_coord >= config.window_width) break;
            }

            if (++Y_coord >= config.window_height) break;
        }
        break;
    
    default:
        break;
    }
}

int main(int argc, char **argv) {

    //Default Usage message for argv
    if(argc < 2){
        fprintf(stderr, "Usage: %s <rom_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    //Initialize emulator configuration 
    config_t config = {0};
    if (!set_config_from_args(&config, argc, argv)) exit(EXIT_FAILURE);

    //Initialize SDL
    sdl_t sdl ={0};
    if(!init_sdl(&sdl, config)) exit(EXIT_FAILURE);

    //Initialize chip8 machine
    chip8_t chip8 = {0};
    const char *rom_name = argv[1];
    if (!init_chip8(&chip8, rom_name)) exit(EXIT_FAILURE);

    //Initial screen clear to background colour
    clear_screen(sdl, config);

    //Main emulator loop
    while(chip8.state != QUIT){
        //handle user input
        handle_input(&chip8);

        if (chip8.state == PAUSED) continue;
        //Get_time()
        //Emulate CHIP8 instruction
        emulate_instruction(&chip8, config);
        //Get_time() elapsed since last  get_time();
        //Delay approx 60Hz/60fps (16.6ms)
        // SDL_Delay(16 - actual time delayed);
        SDL_Delay(16);
        //Update window with changes
        update_screen(sdl, config, chip8);
    }

    //Final cleanup
    final_cleanup(sdl);

    exit(EXIT_SUCCESS);
}


//these are loaded from Settings in production code
double update_rate = 60;
int update_multiplicity = 1;
bool unlock_framerate = true;

//compute how many ticks one update should be
double fixed_deltatime = 1.0 / update_rate;
int64_t desired_frametime = SDL_GetPerformanceFrequency() / update_rate;

//these are to snap deltaTime to vsync values if it's close enough
int64_t vsync_maxerror = SDL_GetPerformanceFrequency() * .0002;
int64_t time_60hz = SDL_GetPerformanceFrequency()/60; //since this is about snapping to common vsync values
int64_t snap_frequencies[] = {time_60hz,        //60fps
                              time_60hz*2,      //30fps
                              time_60hz*3,      //20fps
                              time_60hz*4,      //15fps
                              (time_60hz+1)/2,  //120fps //120hz, 240hz, or higher need to round up, so that adding 120hz twice guaranteed is at least the same as adding time_60hz once
                           // (time_60hz+2)/3,  //180fps //that's where the +1 and +2 come from in those equations
                           // (time_60hz+3)/4,  //240fps //I do not want to snap to anything higher than 120 in my engine, but I left the math in here anyway
                             }; 

//this is for delta time averaging
//I know you can and should use a ring buffer for this, but I didn't want to include dependencies in this sample code
const int time_history_count = 4;
int64_t time_averager[time_history_count] = {desired_frametime, desired_frametime, desired_frametime, desired_frametime};
int64_t averager_residual = 0;

//these are stored in my Application class and are not local variables in production code
bool running = true;
bool resync = true;
int64_t prev_frame_time = SDL_GetPerformanceCounter();
int64_t frame_accumulator = 0;

while (running){
  //frame timer
    int64_t current_frame_time = SDL_GetPerformanceCounter();
    int64_t delta_time = current_frame_time - prev_frame_time;
    prev_frame_time = current_frame_time;

  //handle unexpected timer anomalies (overflow, extra slow frames, etc)
    if(delta_time > desired_frametime*8){ //ignore extra-slow frames
        delta_time = desired_frametime;
    }
    if(delta_time < 0){
        delta_time = 0;
    }


  //vsync time snapping
    for(int64_t snap : snap_frequencies){
        if(std::abs(delta_time - snap) < vsync_maxerror){
            delta_time = snap;
            break;
        }
    }

  //delta time averaging
    for(int i = 0; i<time_history_count-1; i++){
        time_averager[i] = time_averager[i+1];
    }
    time_averager[time_history_count-1] = delta_time;
    int64_t averager_sum = 0;
    for(int i = 0; i<time_history_count; i++){
        averager_sum += time_averager[i];
    }
    delta_time = averager_sum / time_history_count;

    averager_residual += averager_sum % time_history_count;
    delta_time += averager_residual / time_history_count;
    averager_residual %= time_history_count;

  //add to the accumulator
    frame_accumulator += delta_time;

  //spiral of death protection
    if(frame_accumulator > desired_frametime*8){ 
        resync = true;
    }

  //timer resync if requested
    if(resync) {
        frame_accumulator = 0;
        delta_time = desired_frametime;
        resync = false;
    }

  // process system events
    ProcessEvents();

    if(unlock_framerate){ //UNLOCKED FRAMERATE, INTERPOLATION ENABLED
        int64_t consumedDeltaTime = delta_time;

        while(frame_accumulator >= desired_frametime){
            game.fixed_update(fixed_deltatime);
            if(consumedDeltaTime > desired_frametime){ //cap variable update's dt to not be larger than fixed update, and interleave it (so game state can always get animation frames it needs)
                game.variable_update(fixed_deltatime);
                consumedDeltaTime -= desired_frametime;
            }
            frame_accumulator -= desired_frametime;
        }

        game.variable_update((double)consumedDeltaTime / SDL_GetPerformanceFrequency());
        game.render((double)frame_accumulator / desired_frametime);
        display(); //swap buffers
        
    } else { //LOCKED FRAMERATE, NO INTERPOLATION
        while(frame_accumulator >= desired_frametime*update_multiplicity){
            for(int i = 0; i<update_multiplicity; i++){
                game.fixed_update(fixed_deltatime);
                game.variable_update(fixed_deltatime);
                frame_accumulator -= desired_frametime;
            }
        }
        
        game.render(1.0);
        display(); //swap buffers
    }
}
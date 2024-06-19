// written by Timo Raphael Kouroupis 2024
// Under GPL License 

#include <stdio.h>
#include <stdlib.h>

#include <time.h> // for date and time functionality, and sleep function

#include <string.h> // for manipulating strings
#include <ctype.h> // for character handling

#include <ncurses.h> // for better user i/o in the terminal

#include <SDL2/SDL.h> // for sound
#include <SDL2/SDL_mixer.h>

#include <unistd.h> // POSIX operating system api
#include <pthread.h> // for POSIX multithreading


#define MAX_TIMERS 5 //set maximum amount of timers and alarms, just for memory management
#define MAX_ALARMS 5

// initialise structs for timer(s) and alarm(s) to be able to create multiple alarms and timers
// each Timer/Alarm has an hours, minutes, seconds component.
// 'active' indicates whether it is currently active or not
// 'done' indicates whether the alarm/timer is done or not (note: active does not neccesarily mean not done, and vice versa - due to timing/function calls)
// 'done_time' indicates the moment in time when it WILL be done (used for management of when they should be removed)

typedef struct Timer {
    int hours;
    int minutes;
    int seconds;
    int active;
    int done;
    time_t done_time;
} Timer;

typedef struct Alarm {
    int hours;
    int minutes;
    int seconds;
    int active;
    int done;
    time_t done_time;
} Alarm;


//globally initialises arrays for timers and alarms
Timer timers[MAX_TIMERS];
Alarm alarms[MAX_ALARMS];

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // for syncing time and alarm operations
int setting_time_or_alarm = 0; // flag indicates if timer/alarm currently BEING set

pthread_mutex_t jingle_mutex = PTHREAD_MUTEX_INITIALIZER; // for syncing jingle playing correctly
int jingle_playing = 0; // flag indicates if a jingle is currently playing

void *jingle(void *arg) { // function to play jingle

    pthread_mutex_lock(&jingle_mutex); // 'acquire' jingle_mutex to ensure exclusive access to jingle_playing
    
    if (jingle_playing) { // break if jingle is already playing
        pthread_mutex_unlock(&jingle_mutex);
        return NULL;  
    }

    jingle_playing = 1;  // set flag to indicate jingle is NOW playing
    pthread_mutex_unlock(&jingle_mutex); // 'release' mutex after updating

    Mix_Init(MIX_INIT_MP3); // initialize SDL_mixer 

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) { // if audio device initialisation fails, return error and release everything
        printf("SDL_mixer could not initialize! Mix_Error: %s\n", Mix_GetError());
        pthread_mutex_lock(&jingle_mutex);
        jingle_playing = 0;  
        pthread_mutex_unlock(&jingle_mutex);
        return NULL;
    }

    Mix_Chunk *sound = Mix_LoadWAV("jingle.mp3"); // load jingle from file

    if (sound == NULL) { // if loading file fails, return error and release everything
        printf("Failed to load sound! SDL_mixer Error: %s\n", Mix_GetError());
        Mix_CloseAudio();
        pthread_mutex_lock(&jingle_mutex);
        jingle_playing = 0; 
        pthread_mutex_unlock(&jingle_mutex);
        return NULL;
    }

    Mix_PlayChannel(-1, sound, 0); // play sound on any available channel (usually just one anyway)
    SDL_Delay(8000); // jingle is about 8s long

    Mix_FreeChunk(sound); // free allocated memory for the jingle sound chunk
    Mix_CloseAudio(); // close audio device
    Mix_Quit(); // cleanup SDL_mixer - which fully closes the sound system SDL

    pthread_mutex_lock(&jingle_mutex); // acquire mutex again 
    jingle_playing = 0;  // reset flag after jingle finishes
    pthread_mutex_unlock(&jingle_mutex); // release mutex again

    return NULL; // end function
}


void *update_time(void *arg) { // function for handling updates and displaying time, timers, alarms
    while (1) { // runs indefinitely 
        if (!setting_time_or_alarm) { // check if time or alarm is currently BEING set, if not continue
            pthread_mutex_lock(&mutex); // acquire mutex to access timers and alarms 
            time_t now = time(NULL); // get time in seconds since epoch (since Jan 1. 1970)
            struct tm *t = localtime(&now); // convert to (readable) local time structure
            clear(); // clears the terminal
            mvprintw(0, 0, "Current Time: %02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec); // print current time in hh:mm:ss
    
            int timer_line = 1; // which line should the first time be printed on
            for (int i = 0; i < MAX_TIMERS; ++i) { // go through all the possible active timers
                if (timers[i].active) { // check if timer and index i is active
                    if (timers[i].done) { // check if that timer is done
                        if (difftime(now, timers[i].done_time) > 7) {
                            timers[i].active = 0; // reset timer after 7 seconds of being done
                        } else {
                            //play jingle
                            pthread_mutex_lock(&jingle_mutex);
                            if (!jingle_playing) {
                                pthread_t jingle_thread;
                                pthread_create(&jingle_thread, NULL, jingle, NULL);
                                pthread_detach(jingle_thread);
                            }
                            pthread_mutex_unlock(&jingle_mutex); 
                            
                            mvprintw(timer_line, 0, "TIMER DONE"); // print message that its done
                            timer_line++; // so that the next timer (if there is one active) will be printed on the NEXT line
                        }
                    } else {
                        mvprintw(timer_line, 0, "Timer: %02d:%02d:%02d", timers[i].hours, timers[i].minutes, timers[i].seconds); // print remaining time for active timer using the hours, minutes, seconds attributes from the struct
                        timer_line++;
                    }
                }
            }
            
            int alarm_line = timer_line; // start alarm displays AFTER timer displays
            for (int i = 0; i < MAX_ALARMS; ++i) { // iterate through all potentially active alarms
                if (alarms[i].active) {  // check if alarm i is active
                    if (alarms[i].done) { // check is alarm i is done
                        if (difftime(now, alarms[i].done_time) > 7) {
                            alarms[i].active = 0; // reset alarm after 7 seconds of being done
                        } else {
                             //play jingle
                            pthread_mutex_lock(&jingle_mutex);
                            if (!jingle_playing) {
                                pthread_t jingle_thread;
                                pthread_create(&jingle_thread, NULL, jingle, NULL);
                                pthread_detach(jingle_thread);
                            }
                            pthread_mutex_unlock(&jingle_mutex);  
                            
                            mvprintw(alarm_line, 0, "ALARM DONE"); // print message that its done
                            alarm_line++; // so that the next alarm (if there is one active) will be printed on the NEXT line
                        }
                    } else {
                        mvprintw(alarm_line, 0, "Alarm set for %02d:%02d:%02d", alarms[i].hours, alarms[i].minutes, alarms[i].seconds); // print when the alarm is set for
                        alarm_line++;
                    }
                }
            }

            // FYI mvprintw() just 'collects' print statements and only once they are refreshed will they actually be outputted to the terminal. in this case the terminal and everything that is printed refreshes every second
            refresh();
            pthread_mutex_unlock(&mutex); // release mutex accessing shared stuff
        }
        sleep(1); // wait 1 second. for you know, time. 
    }
    return NULL;
}

void *timer_countdown(void *arg) {
    while (1) { // indefinitely countdown loops
        pthread_mutex_lock(&mutex); 
        for (int i = 0; i < MAX_TIMERS; ++i) { // iterate through all timers
            if (timers[i].active && !timers[i].done) { // chick if timers are ACTIVE and NOT DONE
                if (timers[i].hours == 0 && timers[i].minutes == 0 && timers[i].seconds == 0) {
                    // timer has reached zero
                    timers[i].done = 1;
                    timers[i].done_time = time(NULL);
                } else {
                    // decrement by 1 second
                    if (timers[i].seconds == 0) {
                        if (timers[i].minutes == 0) {
                            if (timers[i].hours > 0) {
                                timers[i].hours--; // decrements hours if minutes and seconds are 0
                                timers[i].minutes = 59; // set minute to 59
                                timers[i].seconds = 59; // set seconds to 59
                            }
                        } else {
                            timers[i].minutes--; // decrement minutes if seconds are 0
                            timers[i].seconds = 59; // set seconds to 59
                        }
                    } else {
                        timers[i].seconds--;  // decrements seconds if they are NOT 0
                    }
                }
            }
        }
        pthread_mutex_unlock(&mutex);
        sleep(1); // the gradual passing of time...
    }
    return NULL;
}

void *alarm_check(void *arg) {
    while (1) { // indefinitely check alarms
        pthread_mutex_lock(&mutex); 
        time_t now = time(NULL); // get time in seconds since epoch (since Jan 1. 1970)
        struct tm *t = localtime(&now); // convert to (readable) local time structure
        for (int i = 0; i < MAX_ALARMS; ++i) { // iterate through all alarms
            if (alarms[i].active && !alarms[i].done) { // check if alarm is ACTIVE and NOT done
                if (alarms[i].hours == t->tm_hour && alarms[i].minutes == t->tm_min && alarms[i].seconds == t->tm_sec) { // check if alarm time matches current time - if so alarm is done
                    alarms[i].done = 1;
                    alarms[i].done_time = now;
                }
            }
        }
        pthread_mutex_unlock(&mutex);
        sleep(1); // hurry up!!
    }
    return NULL;
}

void set_timer() {
    char input[20]; // initialise string to store user input
    setting_time_or_alarm = 1; // set flag to indicate setting a timer
    clear(); // clear the terminal
    mvprintw(0, 0, "Set timer (e.g., 60s, 1m30s, 2h10s): "); // print instruction message
    refresh(); // refresh the screen to display only above message
    echo(); // enable echoing of inpute chars to the terminal
    getstr(input); // get user input and write to input variable
    noecho(); // close echoing
    
    Timer new_timer; // create a new timer struct to store timer settings
    new_timer.hours = new_timer.minutes = new_timer.seconds = 0; // set timer to 0
    new_timer.done = 0; // set timer as not done
    new_timer.active = 1; // set timer as active
    

    // this is code to translate the user input (for example '2h30m15s') into the single hours minutes and seconds components and set the timer to them
    int len = strlen(input);
    int i = 0;
    while (i < len) {
        int value = 0;
        while (i < len && isdigit(input[i])) {
            value = value * 10 + (input[i] - '0');
            i++;
        }
        if (i < len) {
            if (input[i] == 'h') {
                new_timer.hours = value; // assign hours if 'h' is found
            } else if (input[i] == 'm') {
                new_timer.minutes = value; // assign minutes if 'm' is found
            } else if (input[i] == 's') {
                new_timer.seconds = value; //assign seconds if 's' is found
            }
            i++;
        }
    }
    
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_TIMERS; ++i) { 
        if (!timers[i].active) { // find next inactive timer slot in the timers array
            timers[i] = new_timer; // assign new_timer to next inactive slot
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
    
    setting_time_or_alarm = 0; // reset flag to indicate setting timer is complete
    clear(); // clear the terminal (and go back to the main clock screen)
}

void set_alarm() {
    char input[20]; // initialise string to store user input
    setting_time_or_alarm = 1; // set flag to indicate setting an alarm
    clear(); // clear the terminal
    mvprintw(0, 0, "Set alarm (hhmmss): "); // print instruction message
    refresh(); // refresh the screen to display only above message
    echo(); // enable echoing of inpute chars to the terminal
    getstr(input); // get user input and write to input variable
    noecho(); // close echoing
    
    Alarm new_alarm; // create a new alarm struct to store the alarm settings

    // parse the hours, minutes, seconds from the user input 
    new_alarm.hours = (input[0] - '0') * 10 + (input[1] - '0');
    new_alarm.minutes = (input[2] - '0') * 10 + (input[3] - '0');
    new_alarm.seconds = (input[4] - '0') * 10 + (input[5] - '0');

    new_alarm.done = 0; // mark alarm as not done
    new_alarm.active = 1; // mark alarm as active
    
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_ALARMS; ++i) {
        if (!alarms[i].active) {  // find next inactive alarm slot in the timers array
            alarms[i] = new_alarm; // assign new_alarm to next inactive slot
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
    
    setting_time_or_alarm = 0; // reset flag to indicate setting timer is complete
    clear(); // clear terminal
}

int main() {
    pthread_t time_thread, timer_thread, alarm_thread; // thread identifiers for time, timers, alarm checks

    initscr(); // initialise ncurses for screen manipulation
    noecho(); // disable echoing (ie input) of characters
    cbreak(); // put terminal into 'cbreak' mode meaning inputed characters are immediately available to the program, meaning that the user selection is just one character

    // create the actual threads for time update, timer countdown and alarm checks
    pthread_create(&time_thread, NULL, update_time, NULL);
    pthread_create(&timer_thread, NULL, timer_countdown, NULL);
    pthread_create(&alarm_thread, NULL, alarm_check, NULL);

    // indefinitely loop and check for user input
    while (1) {
        char ch = getch(); // get a single character from user input
        if (ch == 't') {
            set_timer(); // if 't' is pressed, trigger function to set a timer
        } else if (ch == 'a') {
            set_alarm(); // if 'a' is pressed, trigger function to set an alarm
        }
    }

    endwin(); // end ncurses, restore default terminal

    // cancel all threads and destroy mutexes
    pthread_cancel(time_thread);
    pthread_cancel(timer_thread);
    pthread_cancel(alarm_thread);
    pthread_mutex_destroy(&mutex);

    return 0; // exit the main function - ending the program
}
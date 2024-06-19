# multiclock.c

## Description

A clock program with time keeping, timers, alarms, and audio feedback. Written in C primarily for GNU/Linux. Uses basic multithreading for synchronicity between the current time and current active timer and alarms. Audio feature to play a sound file when a timer has reached 0 or when an alarm is triggered.

## Usage

The main screen shows the current time, followed by the current active timers, followed by the current active alarms.

Press 't' to prompt setting an alarm. Input time in 'xhxmxs' format e.g. '1m30s' or '6h66s'. Press enter to begin the timer.

Press 'a' to prompt setting alarm. Input time in 'hhmmss' format e.g. '083000' or '154530'. Press enter to set the alarm.

Once a timer has reached 0 or an alarm has been triggered, a jingle will play (jingle.mp3) and the timer/alarm will disappear after a few seconds.

Due to its multithreading capabilities, multiple timers and alarms can be set at once. The default value is set to 5 but this can be increased arbitrarily in the source file.

## Installation

multiclock requires [ncurses](https://invisible-island.net/ncurses/announce.html#h2-overview), [SDL2](https://www.libsdl.org/), and [pthread](https://pubs.opengroup.org/onlinepubs/7908799/xsh/pthread.h.html) to run. 

These are common libraries but may still have to be installed depending on the users operating system and environment. 

## Compilation

Compile using:

```$ gcc -o out multiclock.c -lncurses -lpthread -lSDL2 -lSDL2_mixer```

## Project Write-up

Although the scope of this project in terms of user experience may not seem very large, the underlying code of this project is very complex. It includes multithreading, different forms of user input, and reasonably complex algorithms and data structures. 

The program uses several libraries to handle different functionalities such as thread management, time manipulation, screen handling, and audio playback. 

The **time** library is used for accessing and manipulating the system date and time. ```time()``` returns UNIX time and ```struct tm``` can be used to get the actual readable date and time. It also provides the ```sleep()``` function which is pretty handy considering the entire program runs second by second.

The **string** library is used to easily return the length of a string using ```strlen()```.

The **ctype** library is used to easily check if a character is a digit using ```isdigit()```.

*Note: I'm aware that the 2 functions above can be written/derived using functions in the standard library (i.e. without using extra libraries) but it was just easier this way.*

The **ncurses** library is used to create text-based user interfaces. In this case it is used for echoing (and noechoing when necessary) user input, creating multi-line prints using the ```mvprintw()``` function, and refreshing the screen every second. It also includes ```cbreak()``` which allows the program to take single-character inputs from the user and process them immediately (without having to use ```scanf()``` or press enter or anything) - which is why pressing 't' or 'a' immediately prompts the user to create a new timer or alarm, respectively.

The **SDL** and **SDL_mixer** libraries are used to play audio. In order to simply play a sound you have to initialise the mixer, load the sound file, and play it through an audio channel. Thankfully this is well documented and works pretty easily.

The **pthread (POSIX Threads)** library is used to create and manage threads. Threads allow multiple functions/program instruction to run concurrently. 

The timer and alarm system is threaded through one mutex while the system of playing the jingle is threaded through another. Throughout the entire code these mutexes ensure that shared data structures (like the timer and alarm arrays) are accessed by only one thread at a time, ensuring everything is synced properly.

The ```jingle()``` function plays whenever a timer or clock finishes. It first does a bunch of checks, then initialises the sound system, plays the sound, then lastly closes the sound system. It is run through its own mutex and can only be running once at a time (so that jingle sounds don't overlap each other and hurt your ears), but the rest of the program still runs while this function is running. This is the benefit of multithreading.

The ```update_time()``` function actually handles displaying and updating the current time, timers, and alarms. It uses the timer and alarm mutex to access the timers and alarms.

The ```timer_countdown()``` function manages the countdown of each active timer, updating every second.

The ```alarm_check()``` function checks if any alarms match the current time. If they do then the alarm is of course triggered.

The ```set_timer``` and ```set_alarm()``` functions sets new timers and alarms, respectively. They do this by leaving the so-called 'main screen' where the current time, timers, and alarms are shown and entering into a user input screen. Doing this while maintaining the the already active timers and alarms is made possible by the general mutex. 

The ```main()``` function is obviously called at runtime and initialises the program. It handles the single character user input for triggering ```set_timer``` and ```set_alarm()```. It also manages the overall thread lifecycle.

Written by Timo Raphael Kouroupis 2024

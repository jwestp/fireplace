#include <ctime>
#include <iostream>
#include <ncurses.h>
#include <signal.h>
#include <stdlib.h> //random
#include <unistd.h> //getopt

#define MIN(X, Y)  ((X) < (Y) ? (X) : (Y))
#define MAX(X, Y)  ((X) > (Y) ? (X) : (Y))

//----------------------------------------[Global variables]----------------------------------------

static char dispch;        //the character used to draw the flames
static int WIDTH, HEIGHT;  //Set by ncurses
static int framerate;      //framerate
static int heightrecord;   //max height reached by the flames
static int maxtemp;        //maximum flame temperature
static int wolfrule;       //rule for wolfram eca

static volatile sig_atomic_t sig_caught = 0;

//------------------------------[Memory Management and Initialization]------------------------------

int** init(int y, int x){
    int** out = new int*[y];
    for(int i = 0; i < y; i++){
        out[i] = new int[x];
    }

    for(int i = 0; i < y; i++){
        for(int j = 0; j < x; j++){
            out[i][j] = 0;
        }
    }
    return out;
}

void deallocate(int** in, int rows){
    for(int i = 0; i < rows; i++){
        delete[] in[i];
    }
    delete[] in;
}

void start_ncurses(){
    initscr();
    start_color();
    init_color(COLOR_BLACK,    100,   100,   100);
    init_color(COLOR_RED,      300,   0,     0);
    init_color(COLOR_GREEN,    500,   0,     0);
    init_color(COLOR_BLUE,     700,   100,   0);
    init_color(COLOR_YELLOW,   900,   300,   0);
    init_color(COLOR_MAGENTA,  1000,  500,   100);
    init_color(COLOR_CYAN,     1000,  800,   500);
    init_color(COLOR_WHITE,    1000,  1000,  1000);
    
    init_pair(1,  COLOR_RED,      COLOR_BLACK);
    init_pair(2,  COLOR_GREEN,    COLOR_BLACK);
    init_pair(3,  COLOR_BLUE,     COLOR_BLACK);
    init_pair(4,  COLOR_YELLOW,   COLOR_BLACK);
    init_pair(5,  COLOR_MAGENTA,  COLOR_BLACK);
    init_pair(6,  COLOR_CYAN,     COLOR_BLACK);
    init_pair(7,  COLOR_WHITE,    COLOR_BLACK);
    curs_set(0);    //invisible cursor
    timeout(0);     //make getch() non-blocking
    cbreak();
    noecho();
    keypad(stdscr, TRUE); 
    getmaxyx(stdscr, HEIGHT, WIDTH);
    heightrecord = HEIGHT;
}

//---------------------------------------[Cellular Automata]---------------------------------------

//As a cell cools it has a higher chance of cooling again on the next frame.
int cooldown(int heat){
    if(heat == 0) return 0;
    int r = (rand() % heat);
    if(r == 0) heat--;
    return heat;
}

void cleargrid(int** grid, int h){
    for(int i = h; i < HEIGHT; i++){
        for(int j = 0; j < WIDTH; j++){
            grid[i][j] = 0;
        }
    }
}

void nextframe(int** field, int** count, int* hotplate){
    cleargrid(count, heightrecord);
    int rowsum = 0;
    int h = heightrecord - 3;
    h = MAX(h, 1);  //we can ignore the vast majority of cold cells
                    //and skip down to the bottom of the window
    for(int i = h; i <= HEIGHT; i++){
        for(int j = 0; j < WIDTH; j++){
            float avg = 0;
            //int temp = rand() % maxtemp * 4;
            int counter = 0;

            //the search space around the current cell is as follows
            //    .......
            //    ...X...
            //    .......
            //    .......
            //    .......
            for(int xoff = -3; xoff <= 3; xoff++){
                for(int yoff = -1; yoff <= 3; yoff++){
                    int y = i + yoff;
                    y = MAX(y,0); //if y is less than zero, clamp it to zero.
                    int x = j + xoff;
                    //if the search has gon beyond the left or right, no heat is added
                    if(x < 0 || x >= WIDTH) avg += 0;
                    //if the search goes below the screen, add the hotplate value.
                    //the hotplate has infinite depth.
                    else if(y >= HEIGHT)  avg += hotplate[x];
                    else avg += field[y][x];
                    counter++;
                }
            }
            avg /= counter;
            //see if the cell cools or not
            //we add the value at (i-1) so that an upward motion will be created.
            count[i - 1][j] = cooldown(avg);
            rowsum += count[i-1][j];
        }
        if(rowsum > 0 && i < heightrecord) heightrecord = i;
        rowsum = 0;
    }
    
    for(int i = 0; i < HEIGHT; i++){
        for(int j = 0; j < WIDTH; j++){
            field[i][j] = count[i][j];
        }
    }
}

void printframe(int** field, int** count, int* hotplate){
    char disp;
    for(int i = 0; i < HEIGHT; i++){
        for(int j = 0; j < WIDTH; j++){
            move(i,j);
            //if the cell is cold, print a space, otherwise print [dispch]
            int color = (7 * field[i][j] / maxtemp) + 1;
            color = MIN(color,7);
            disp = field[i][j] == 0 ? ' ' : dispch;
            attron(COLOR_PAIR(color));
            addch(disp);
            attroff(COLOR_PAIR(color));
        }
    }
    //mvaddstr(0, 0, std::to_string(wolfrule).c_str());
    refresh();
}

//Wolfram's Elementary cellular atomaton
void wolfram(int* world, int* next, const int rule){
    int l,c,r;
    int lidx, ridx;
    int current;
    for(int i = 0; i < WIDTH; i++){
        lidx = i > 0 ? i - 1 : WIDTH - 1;
        ridx = (i + 1) % WIDTH;
        l = world[lidx];
        c = world[i];
        r = world[ridx];
        current = (l<<2) | (c<<1) | r;
        next[i] = ((1<<current) & rule) > 0 ? 1 : 0;
    }

    for(int i = 0; i < WIDTH; i++){
        world[i] = next[i];
    }
}

void warm(int* heater, int* hotplate){
    for(int i = 0; i < WIDTH; i++){
        hotplate[i] /= 2;
    }
    for(int i = 0; i < WIDTH; i++){
        hotplate[i] += heater[i] * maxtemp;
    }
}

//-------------------------------------------[Main Loop]-------------------------------------------

void flames(){
    int** field = init(HEIGHT, WIDTH); //The cells that will be displayed
    int** count = init(HEIGHT, WIDTH); //A grid of cells used to tally neighbors for CA purposes
    int* heater = new int[WIDTH]; //these special cells provide "heat" at the bottom of the screen.
    int* heater_count = new int[WIDTH];
    int* hotplate = new int[WIDTH]; //The heater heats the hotplate. The hotplate will cool without heat.
    
    for(int i = 0; i < WIDTH; i++){
        heater[i] = rand() % 2;
    }
    
    char c = 0;
    
    while(sig_caught == 0){
        //Use Rule 60 (http://mathworld.wolfram.com/Rule60.html) to make flames flicker nicely.
        wolfram(heater, heater_count, wolfrule);
        warm(heater, hotplate);
        printframe(field, count, hotplate);
        nextframe(field, count, hotplate);
        usleep(framerate);
    }

    refresh();
    delete[] hotplate;
    delete[] heater;
    delete[] heater_count;
    deallocate(field, HEIGHT);
    deallocate(count, HEIGHT);
}

void printhelp(char progname[]){
    std::cout << "\nUsage: " << progname << " [options]\n"
        << "\t-c character\tAn ASCII character to draw the flames. Default is '@'.\n"
        << "\t-h\t\tPrint this message.\n"
        << "\t-f framerate\tSet the framerate in frames/sec. Default is 20.\n"
        << "\t\t\tA framerate of zero will make frames spit out as soon as they are ready.\n"
        << "\t-t temp\t\tSet the maximum temperature of the flames. Default is 10.\n"
        << "\t\t\tA higher temp means taller flames.\n"
        << "\n"
        << "Press ^C at any time to douse the flames.\n\n";
}

void signal_handler(int signum){
    if(signum == SIGINT){
        sig_caught = 1;
    }
}

int main(int argc, char** argv){
    signal(SIGINT, signal_handler);
    
    int persecond = 1000000;
    srand(time(NULL));
    framerate = persecond / 20;
    maxtemp = 10;
    dispch = '@';
    wolfrule = 60;
    
    int c;
    opterr = 0;
    while((c = getopt(argc, argv, "c:hf:t:w:")) != -1){
        switch (c){
            case 'c':
                dispch = optarg[0];
                break;
            case 'h':
                printhelp(argv[0]);
                return 0;
            case 'f':
                if(atoi(optarg) < 1) framerate = 0;
                else framerate = persecond / atoi(optarg);
                break;
            case 't':
                maxtemp = atoi(optarg);
                break;
            case 'w':
                wolfrule = atoi(optarg);
                break;
            case '?':
                std::cout << "\nYou've really bunged this one up. Here, this may help:\n";
                printhelp(argv[0]);
                return 1;
            default:
                std::cerr << "What the hell?!";
                return 2;
        }
    }
    
    start_ncurses();
    flames();
    use_default_colors();
    endwin();
    return 0;
}

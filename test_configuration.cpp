// Link layer parameters
unsigned int timeout = 100000;
unsigned int max_win = 10;
unsigned int num_seq = 20;

// Physical Layer parameters
const int PHYSICAL_LAYER_DELAY = 1000;

double drop[] = {0.0,0.3,0.2,0.5,0.4};
double corrupt[] = {0.8,0.7,0.8,0.9,0.0};
Impair a_impair(drop,5, corrupt,5, PHYSICAL_LAYER_DELAY);

Impair b_impair(NULL,0, NULL,0, PHYSICAL_LAYER_DELAY);

// Link layer parameters
unsigned int timeout = 100000;
unsigned int max_win = 3;
unsigned int num_seq = 10;

// Physical Layer parameters
const int PHYSICAL_LAYER_DELAY = 1000;

double drop[] = {1.0,0.7,0.8,0.9,0,4};
Impair a_impair(drop,5, NULL,0, PHYSICAL_LAYER_DELAY);

Impair b_impair(NULL,0, NULL,0, PHYSICAL_LAYER_DELAY);

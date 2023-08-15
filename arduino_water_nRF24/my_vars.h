
// need to put this enum in a seperate .h file so
// we can pass it as a parameter in the arduino IDE
enum State {
  BOWLMISSING,
  WATEREMPTY,
  WATERFILLING, 
  READY, 
  CALIBRATING, 
  RADIOTEST, 
  ARMWAIT, 
  ERRORS, 
  DISCONNECTED
};


const int BLACK[]   = {  0,   0,   0};
const int RED[]     = {255,   0,   0};
const int GREEN[]   = {  0, 255,   0};
const int BLUE[]    = {  0,   0, 255};
const int YELLOW[]  = {255, 255,   0};
const int CYAN[]    = {  0, 255, 255};
const int MAGENTA[] = {255,   0, 255};
const int WHITE[]   = {255, 255, 255};

const int PURPLE[]  = {128,   0, 255};
const int PINK[]    = {175,  75, 148};
const int ORANGE[]  = {237, 120,   6};


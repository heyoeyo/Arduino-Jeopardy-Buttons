
// Pin numbers of LED data-in & player buttons
const int LED_DATA_PIN_NUMBER = 8;
const uint8_t BTN_PIN_NUMBERS[] = {4,5,6,7};

// Color of LED as: {red, green, blue}
// -> Rows correspond to button pins (i.e. determines the color-per-pin)
// -> Numbers need to be between 0 and 255
// -> Larger number make brighter colors, but it's VERY non-linear
const uint8_t BTN_COLORS[][3] = {
  {20, 10, 0},
  {25, 0, 0}, 
  {0, 25, 0},
  {0, 0, 25}
};

// The number of addressable LEDs
const int NUM_LEDS = 16;

// The amount of time LEDs stay lit after button press, in milliseconds
const float ANSWER_TIME_MS = 2350;

// The amount of time that the startup animation (snaking LED colors) lasts, in milliseconds
const int STARTUP_TIME_MS = 4500;

// Debounce timer, makes sure buttons must be pressed (not held) to trigger LEDs
const uint32_t BTN_DEBOUNCE_MS = 50;


// ---------------------------------------------------------------------------------------------------

// Library used to control 5V addressable RGB LED strip
// -> Uses a single data signal input, with very fancy 'unipolar non-return-to-zero' protocol
// -> Searchable technical name is: WS2812B (also SK6812?)
// -> Example product & more info here: https://www.adafruit.com/product/1138
#include <Adafruit_NeoPixel.h>

// Used internally, don't change
int _num_btns;
float _led_index_normscale;
uint32_t _led_color;
uint32_t _led_start_ms;
uint32_t _led_last_update_ms;
bool _leds_are_active;

// Used internally for button debouncing (only allocates memory for 16 possible buttons!)
uint32_t _btn_last_pressed_ms[16];
bool _btn_rising[16];

// Set up use of NeoPixel library
Adafruit_NeoPixel _pixels(NUM_LEDS, LED_DATA_PIN_NUMBER, NEO_GRB + NEO_KHZ800);


// ---------------------------------------------------------------------------------------------------

bool update_led_state(uint32_t time_ms) {

  /*
  Function used to control LED light-up after button presses
  Also turns LEDs off as answer timer runs out
  */

  // Bail once the answer time has elapsed
  float t_elapsed_ms = float(time_ms - _led_start_ms);
  bool need_turn_off = t_elapsed_ms > ANSWER_TIME_MS;
  if (need_turn_off) {
    return need_turn_off;
  }

  // Avoid blasting LEDs with updates
  uint32_t ms_since_last_led_update = time_ms - _led_last_update_ms;
  if (ms_since_last_led_update < 10) {
    return need_turn_off;
  }
  _led_last_update_ms = time_ms;

  // Get a 'relative' time remaining, used to 'chase' the LEDs as time runs out
  // -> 1.0 if full answer time remaining, 0.0 if time has run out
  // -> Scale/offset so the value only counts down in last 25%
  float t_remaining_rel = 4.0*(1.0 - (t_elapsed_ms / ANSWER_TIME_MS));

  // Determine on/off state of every LED in the strip
  float led_dist;
  bool is_led_on;
  uint32_t on_color;
  for(int led_index = 0; led_index < NUM_LEDS; led_index++) {
    led_dist = abs(float(led_index) * _led_index_normscale - 0.5)*2.0;
    is_led_on = led_dist < t_remaining_rel;
    on_color = is_led_on ? _led_color : 0;
    _pixels.setPixelColor(led_index, on_color);
  }

  // Need to call this to have LEDs physically update! (otherwise operations are just in RAM...?)
  _pixels.show();

  return need_turn_off;
}

// ...................................................................................................

void setup() {

  // Figure out how many buttons we have & pre-record LED count as a float for efficiency
  _num_btns = sizeof(BTN_PIN_NUMBERS);
  _led_index_normscale = 1.0 / float(NUM_LEDS - 1);
  _leds_are_active = false;

  // Make sure each button pin is configured properly
  // -> Each is an input with pull-up resistor
  // -> Set default state values
  uint32_t t_setup = millis();
  for (int b = 0; b < _num_btns; b++) {
    pinMode(BTN_PIN_NUMBERS[b], INPUT_PULLUP);
    _btn_last_pressed_ms[b] = t_setup;
    _btn_rising[b] = false;
  }

  // Set up communication with pixel strip and make sure all LEDs are off to begin with
  _pixels.begin();
  startup_led_animation(4, 4);
  turn_off_leds();

  return;
}

// ...................................................................................................

void loop() {

  // Get shared loop time for all event timing calculations
  uint32_t t_loop = millis();

  // Read buttons, looking for rising edges
  bool have_rising_event = false;
  for (int b = 0; b < _num_btns; b++) {

    // Record all button press events
    // -> Do this even if the LEDs are on, since we need to check for 'cheating'
    //    (i.e. player holding down button as timer runs out)
    bool btn_is_rising = false;
    bool btn_is_pressed = digitalRead(BTN_PIN_NUMBERS[b]) == 0;
    if (btn_is_pressed) {
      uint32_t ms_since_last_press = (t_loop - _btn_last_pressed_ms[b]);
      btn_is_rising = ms_since_last_press > BTN_DEBOUNCE_MS;
      _btn_last_pressed_ms[b] = t_loop;
    }

    // Record rising events, in case the LEDs are off and need to be triggered
    // -> Note if a rising event occurs while LEDs are on, it will be ignored!
    _btn_rising[b] = btn_is_rising;
    have_rising_event = have_rising_event || btn_is_rising;
  }

  if (_leds_are_active) {

    // Update the LEDs if they're active and disable if needed
    bool disable_leds = update_led_state(t_loop);
    if (disable_leds) {
      
      // Reset led state
      _leds_are_active = false;
      _led_color = _pixels.Color(0, 0, 0);
      _led_start_ms = 0;
      _led_last_update_ms = 0;

      turn_off_leds();
    }

  } else if (have_rising_event) {

      // Set led state to trigger the LEDs to turn on
      int button_index = get_rising_button();
      _led_color = get_led_color(button_index);
      _led_start_ms = t_loop;
      _led_last_update_ms = 0;
      _leds_are_active = true;

  }

  return;
}


// ---------------------------------------------------------------------------------------------------
// Helper functions

void turn_off_leds() {

  _pixels.clear();
  _pixels.show();

  return;
}

// ...................................................................................................

uint32_t get_led_color(uint8_t button_index) {

  uint8_t btn_red = BTN_COLORS[button_index][0];
  uint8_t btn_green = BTN_COLORS[button_index][1];
  uint8_t btn_blue = BTN_COLORS[button_index][2];

  return _pixels.Color(btn_red, btn_green, btn_blue);
}

// ...................................................................................................

int get_rising_button() {
  
  /* Assumes a button HAS been pressed! Will give weird results if this isn't true */

  int button_select;

  // Randomize which button we check first
  // -> Only for (extremely) rare case where more than one button is pressed
  // -> Want to avoid always checking button 0 first, then 1, then 2, etc.
  //    since this would give an unfair advantage to player 0
  int random_offset = random(0, _num_btns);
  for (int k = 0; k < _num_btns; k++) {
    button_select = (k + random_offset) % _num_btns;
    if (_btn_rising[button_select]) {
      break;
    }
  }

  return button_select;
}

// ...................................................................................................

void startup_led_animation(int block_length, int gap_length) {

  /*
  Code for start-up animation
  Lights up 'block_length' number of LEDs for each button color
  and scrolls them through full LED strip length.
  Used to indicate player colors, but also confirms all LEDs are working!
  */

  // Set up the length of each block of single-colored LEDs + gap pixels
  int section_length = block_length + gap_length;
  int num_frames = NUM_LEDS + section_length * _num_btns;

  // Set up delay timing, so that pattern lasts a fixed amount of time, regardless of LED count
  int millis_per_frame = STARTUP_TIME_MS / num_frames;

  // Re-draw a pattern of colored LEDs, with increasing offset from start
  for (int offset = 0; offset < num_frames; offset++) {

    _pixels.clear();

    // Loop over each button, which sets a different LED color
    for (int b = 0; b < _num_btns; b++) {

      // Set LEDs that are lit up
      // - Don't need to explicitly set LEDs that are off, due to earlier clear call!
      uint32_t btn_color = get_led_color(b);
      int block_offset = section_length * b;
      for (int k = 0; k < block_length; k++) {
        int led_index = offset - (k + block_offset);
        if (led_index < 0 || led_index >= NUM_LEDS) continue;
        _pixels.setPixelColor(led_index, btn_color);
      }

    }

    // Show led sequence briefly 'i.e. one frame of animation'
    _pixels.show();
    delay(millis_per_frame);
  }

  return;
}

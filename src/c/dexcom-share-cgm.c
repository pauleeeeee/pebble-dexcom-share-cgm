#include <pebble.h>
#include <pebble-battery-bar/pebble-battery-bar.h>
#include <pebble-bluetooth-icon/pebble-bluetooth-icon.h>
#include "./dexcom-share-cgm.h"

static Window *s_window;
static TextLayer *s_text_layer;
static Window *s_window;
static BluetoothLayer *s_bluetooth_layer;
static BatteryBarLayer *s_battery_layer;
static Layer *s_person_one_holder_layer;
static TextLayer *s_time_layer, *s_background_layer, *s_full_date_layer;
static TextLayer *s_p_one_sgv_text_layer, *s_p_one_time_ago_text_layer;
static char s_p_one_sgv_text[8], s_p_one_time_ago_text[24];
static int s_p_one_ago_int;
static GBitmap *s_p_one_icon_bitmap = NULL;
static BitmapLayer *s_p_one_icon_layer;
static int s_respect_quiet_time = 0;

static void updateTimeAgo(){
  //format integer to string
  snprintf(s_p_one_time_ago_text, sizeof(s_p_one_time_ago_text), "%d", s_p_one_ago_int);
  //concatenate ' min ago' to the end of the formatted int
  strcat(s_p_one_time_ago_text, " min ago");
  text_layer_set_text(s_p_one_time_ago_text_layer, s_p_one_time_ago_text);
}


static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[16];
  strftime(s_buffer, sizeof(s_buffer), "%l:%M %p", tick_time);

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, s_buffer);

  //each minute that goes by... increase ago ints
  s_p_one_ago_int++;
  //update timeAgo
  updateTimeAgo();
  //write the int to storage so that accurate values are shown even after leaving and returning to watchface
  persist_write_int(7,s_p_one_ago_int);
}

static void update_date() {
    // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  
  static char full_date_text[] = "Xxx -----";
  
  strftime(full_date_text, sizeof(full_date_text), "%a %m/%d", tick_time);
  text_layer_set_text(s_full_date_layer, full_date_text);
}

static const uint32_t const tripple_segments[] = { 100, 100, 100, 100, 100, 100 };
VibePattern tripple = {
  .durations = tripple_segments,
  .num_segments = ARRAY_LENGTH(tripple_segments),
};

static const uint32_t const quad_segments[] = { 100, 100, 100, 100, 100, 100, 100, 100 };
VibePattern quad = {
  .durations = quad_segments,
  .num_segments = ARRAY_LENGTH(quad_segments),
};

static const uint32_t const sos_segments[] = { 100,30,100,30,100,200,200,30,200,30,200,200,100,30,100,30,100 };
VibePattern sos = {
  .durations = sos_segments,
  .num_segments = ARRAY_LENGTH(sos_segments),
};

static const uint32_t const bum_segments[] = { 150,150,150,150,75,75,150,150,150,150,450 };
VibePattern bum = {
  .durations = bum_segments,
  .num_segments = ARRAY_LENGTH(bum_segments),
};

static void sendAlert(int alert) {
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "switching alert %d", alert);
  switch(alert){
    case 0:
      // APP_LOG(APP_LOG_LEVEL_DEBUG, "no alert %d", alert);
      break;
    case 1:
      // APP_LOG(APP_LOG_LEVEL_DEBUG, "short vibes %d", alert);
      vibes_short_pulse();
      break;
    case 2:
      vibes_long_pulse();
      break;
    case 3:
      vibes_double_pulse();
      break;
    case 4:
      vibes_enqueue_custom_pattern(tripple);
      break;
    case 5:
      vibes_enqueue_custom_pattern(quad);
      break;
    case 6:
      vibes_enqueue_custom_pattern(sos);
      break;
    case 7:
      vibes_enqueue_custom_pattern(bum);
      break;
  }
}


//this function is called every time the pebble ticks a minute
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}


//function that receives AppMessages from the JS component
static void in_received_handler(DictionaryIterator *iter, void *context) {
  
  //declare tuples for all known keys.
  Tuple *sgv_tuple = dict_find (iter, SGV);
  Tuple *direction_tuple = dict_find (iter, Direction);
  Tuple *minutes_ago_tuple = dict_find (iter, MinutesAgo);
  Tuple *respect_quiet_time_tuple = dict_find (iter, RespectQuietTime);
  Tuple *send_alert_tuple = dict_find(iter, SendAlert);

  //update values of the watchface. 
  //APP_LOG(APP_LOG_LEVEL_DEBUG, "SGV value ios %d", 0);


  if (sgv_tuple) {
    strncpy(s_p_one_sgv_text, sgv_tuple->value->cstring, sizeof(s_p_one_sgv_text));
    persist_write_string(2, s_p_one_sgv_text);
    text_layer_set_text(s_p_one_sgv_text_layer, s_p_one_sgv_text);
  } 

  if (direction_tuple) {
    //write to persistent storage
    persist_write_int(6, direction_tuple->value->int32);
    //if the bitmap exists, destroy it (saving memory)
    if (s_p_one_icon_bitmap) {
      gbitmap_destroy(s_p_one_icon_bitmap);
    }
    //create new bitmap with the value
    s_p_one_icon_bitmap = gbitmap_create_with_resource(ICONS[direction_tuple->value->int32]);
    //set bitmap to layer
    bitmap_layer_set_bitmap(s_p_one_icon_layer, s_p_one_icon_bitmap);
  }

  if (minutes_ago_tuple) {
    //set value to outside variable
    s_p_one_ago_int = minutes_ago_tuple->value->int32;
    //write int to storage
    persist_write_int(7,s_p_one_ago_int);
    //update ago time
    updateTimeAgo();
  }

  //alerts
  if (respect_quiet_time_tuple) {
    s_respect_quiet_time = respect_quiet_time_tuple->value->int32;
    persist_write_int(52, s_respect_quiet_time);
  }

  if (send_alert_tuple) {

    int alert = send_alert_tuple->value->int32;
    // APP_LOG(APP_LOG_LEVEL_DEBUG, "alert received in tuple %d", alert);
    if (quiet_time_is_active() && s_respect_quiet_time) { 
          // APP_LOG(APP_LOG_LEVEL_DEBUG, "alert suppressed %d", alert);       
      return;
    } else {
      // APP_LOG(APP_LOG_LEVEL_DEBUG, "sending alert to function %d", alert);
      sendAlert(alert);
    }
  }

}

static void in_dropped_handler(AppMessageResult reason, void *context){
  //handle failed message - probably won't happen
}

//draws the two white rectangles for each holder layer and the graph
static void person_one_update_proc(Layer *layer, GContext *ctx) {
  //set fill color to white
  graphics_context_set_fill_color(ctx, GColorWhite);
  //define bounds of the rectangle which is passed through from the update_proc assignment
  GRect rectangle = layer_get_bounds(layer);
  //create filled rectangle, rounding all corners with 5px radius
  graphics_fill_rect(ctx, rectangle, 5, GCornersAll);

  // if (s_p_one_ago_int > 30){
  //   graphics_context_set_stroke_color(ctx, GColorBlack);
  //   graphics_context_set_stroke_width(ctx, 3);
  //   graphics_draw_line(ctx,GPoint(2,36), GPoint(65, 36));
  // }

}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  //create time text layer
  s_time_layer = text_layer_create(GRect(0,0,bounds.size.w,28));
  text_layer_set_background_color(s_time_layer, GColorBlack);
  text_layer_set_text(s_time_layer, "00:00 AM");
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  //create date layer
  s_full_date_layer = text_layer_create(GRect(0, bounds.size.h - 34, bounds.size.w, 40));
  text_layer_set_background_color(s_full_date_layer, GColorClear);
  // text_layer_set_text(s_full_date_layer, "--");
  text_layer_set_text_color(s_full_date_layer, GColorWhite);
  text_layer_set_font(s_full_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(s_full_date_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_full_date_layer));

  //create battery bar
  s_battery_layer = battery_bar_layer_create();
  battery_bar_set_position(GPoint(bounds.size.w-45,14));
  battery_bar_set_colors(GColorWhite, GColorDarkGray, GColorDarkGray, GColorWhite);
  battery_bar_set_percent_hidden(true);
  layer_add_child(window_layer, s_battery_layer);

  //create bluetooth indicator
  s_bluetooth_layer = bluetooth_layer_create();
  bluetooth_set_position(GPoint(12,14));
  bluetooth_vibe_disconnect(true);
  bluetooth_vibe_connect(false);
  //void bluetooth_set_colors(GColor connected_circle, GColor connected_icon, GColor disconnected_circle, GColor disconnected_icon);
  bluetooth_set_colors(GColorBlack, GColorWhite, GColorDarkGray, GColorClear);
  layer_add_child(window_layer, s_bluetooth_layer);

  //recall respect quiet time value; 0 or 1;
  s_respect_quiet_time = persist_read_int(52);


  //build holding layer
  GRect person_one_holder_bounds = GRect(10,35,bounds.size.w-20,bounds.size.h-70);
  s_person_one_holder_layer = layer_create(person_one_holder_bounds);
  layer_set_update_proc(s_person_one_holder_layer, person_one_update_proc);

  //add directional icon first
  //create layer which will hold the bitmap
  s_p_one_icon_layer = bitmap_layer_create(GRect((person_one_holder_bounds.size.w/2)+23, 27, 30, 30));
	//add bitmap holder to person holder
  layer_add_child(s_person_one_holder_layer, bitmap_layer_get_layer(s_p_one_icon_layer));
  //check to see if the int representing the bitmap exists in local storage
  if(persist_exists(6)){
    //if it exists, check to see if a bitmap already exists
    if (s_p_one_icon_bitmap) {
      //destory existing bitmap for memory
      gbitmap_destroy(s_p_one_icon_bitmap);
    }
    //create new bitmap arrow using int as index
    s_p_one_icon_bitmap = gbitmap_create_with_resource(ICONS[persist_read_int(6)]);
    //set bitmap to layer
    bitmap_layer_set_bitmap(s_p_one_icon_layer, s_p_one_icon_bitmap);
  } else {
    if (s_p_one_icon_bitmap) {
      gbitmap_destroy(s_p_one_icon_bitmap);
    }
    s_p_one_icon_bitmap = gbitmap_create_with_resource(ICONS[4]);
    bitmap_layer_set_bitmap(s_p_one_icon_layer, s_p_one_icon_bitmap);
  }

  //create sgv layer
	s_p_one_sgv_text_layer = text_layer_create(GRect(person_one_holder_bounds.origin.x, (person_one_holder_bounds.size.h/2)-35, person_one_holder_bounds.size.w, 50));
	text_layer_set_text_color(s_p_one_sgv_text_layer, GColorBlack);
	text_layer_set_background_color(s_p_one_sgv_text_layer, GColorClear);
	text_layer_set_font(s_p_one_sgv_text_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_MEDIUM_NUMBERS));
	text_layer_set_text_alignment(s_p_one_sgv_text_layer, GTextAlignmentLeft);
  //read sgv from memory
  if(persist_exists(2)){
    persist_read_string(2, s_p_one_sgv_text, sizeof(s_p_one_sgv_text));
  } else {
    strncpy(s_p_one_sgv_text, "000", sizeof(s_p_one_sgv_text));
  }
  text_layer_set_text(s_p_one_sgv_text_layer, s_p_one_sgv_text);
  //add sgv to holder
	layer_add_child(s_person_one_holder_layer, text_layer_get_layer(s_p_one_sgv_text_layer));

  //create time ago layer
	s_p_one_time_ago_text_layer = text_layer_create(GRect(0, (person_one_holder_bounds.size.h/2)+15, person_one_holder_bounds.size.w, 22));
	text_layer_set_text_color(s_p_one_time_ago_text_layer, GColorBlack);
	text_layer_set_background_color(s_p_one_time_ago_text_layer, GColorClear);
	text_layer_set_font(s_p_one_time_ago_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text_alignment(s_p_one_time_ago_text_layer, GTextAlignmentCenter);
  // read time ago from memory
  if(persist_exists(7)){
    s_p_one_ago_int = persist_read_int(7);
  } else {
    s_p_one_ago_int = 0;
  }
  updateTimeAgo(0);

  // add timeago to holder
	layer_add_child(s_person_one_holder_layer, text_layer_get_layer(s_p_one_time_ago_text_layer));

  //add person holding layer to root window
  layer_add_child(window_layer, s_person_one_holder_layer);

  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  //register with tap handler service
  //accel_tap_service_subscribe(accel_tap_handler);

  //update the time when the watchface loads
  update_time();
  update_date();
}

static void prv_window_unload(Window *window) {
  //destroy everything I guess
  text_layer_destroy(s_p_one_sgv_text_layer); 
  text_layer_destroy(s_p_one_time_ago_text_layer); 

  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_background_layer);
  s_p_one_icon_bitmap = NULL;
  bluetooth_layer_destroy(s_bluetooth_layer);
  battery_bar_layer_destroy(s_battery_layer);
  layer_destroy(s_person_one_holder_layer);

}

static void prv_init(void) {
  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  const bool animated = true;
  window_stack_push(s_window, animated);

  //instantiate appmessages
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_open(128, 128);

}

static void prv_deinit(void) {
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}

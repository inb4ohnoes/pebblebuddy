#include <pebble.h>
#include <stdlib.h>
  
static Window *window;
static TextLayer *s_time_layer;

static TextLayer *temperature_layer;
static char temperature[16];

static BitmapLayer *icon_layer;
static GBitmap *icon_bitmap = NULL;

static AppSync sync;
static uint8_t sync_buffer[32];

static GBitmap *cloud1_bitmap, *cloud2_bitmap;
static BitmapLayer *cloud1_layer, *cloud2_layer;
static AppTimer *timer;
static int count = 1;
static const int delta = 500;


enum WeatherKey {
  WEATHER_ICON_KEY = 0x0,         // TUPLE_INT
  WEATHER_TEMPERATURE_KEY = 0x1,  // TUPLE_CSTRING
};

static uint32_t WEATHER_ICONS[] = {
  RESOURCE_ID_sun1,
  RESOURCE_ID_cloud1,
  RESOURCE_ID_rain1,
  RESOURCE_ID_snow1
};
static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Create a long-lived buffer
  static char buffer[] = "00:00:00";

  // Write the current hours and minutes into the buffer
  if(clock_is_24h_style() == true) {
    // Use 24 hour format
    strftime(buffer, sizeof("00:00:00"), "%H:%M:%S", tick_time);
  } else {
    // Use 12 hour format
    strftime(buffer, sizeof("00:00:00"), "%I:%M:%S", tick_time);
  }

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, buffer);
}

static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
  switch (key) {
    case WEATHER_ICON_KEY:
      if (icon_bitmap) {
        gbitmap_destroy(icon_bitmap);
      }

      icon_bitmap = gbitmap_create_with_resource(WEATHER_ICONS[new_tuple->value->uint8]);
      bitmap_layer_set_bitmap(icon_layer, icon_bitmap);
      break;

    case WEATHER_TEMPERATURE_KEY:
      // App Sync keeps new_tuple in sync_buffer, so we may use it directly
      text_layer_set_text(temperature_layer, new_tuple->value->cstring);
      break;
  }
}

static void timer_callback(void *data) {
  if ( count == 1 ) {
    layer_remove_from_parent(bitmap_layer_get_layer(cloud2_layer));
    layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(cloud1_layer));
  }
  else {
    layer_remove_from_parent(bitmap_layer_get_layer(cloud1_layer));
    layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(cloud2_layer));
  }
  count *= -1;
  
  timer = app_timer_register(delta, (AppTimerCallback) timer_callback, NULL);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  
  icon_layer = bitmap_layer_create(GRect(32, 35, 80, 80));
  layer_add_child(window_layer, bitmap_layer_get_layer(icon_layer));
  
  temperature_layer = text_layer_create(GRect(0, 100, 144, 68));
  text_layer_set_text_color(temperature_layer, GColorWhite);
  text_layer_set_background_color(temperature_layer, GColorBlack);
  text_layer_set_font(temperature_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(temperature_layer, GTextAlignmentCenter);
  text_layer_set_text(temperature_layer, temperature);

  Tuplet initial_values[] = {
    TupletInteger(WEATHER_ICON_KEY, (uint8_t) 1),
    TupletCString(WEATHER_TEMPERATURE_KEY, "1234\u00B0C"),
  };
  app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
      sync_tuple_changed_callback, sync_error_callback, NULL);

  layer_add_child(window_layer, text_layer_get_layer(temperature_layer));
  
  s_time_layer = text_layer_create(GRect(0, 0, 150, 40));
  text_layer_set_background_color(s_time_layer, GColorBlack);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_text(s_time_layer, "00:00:00");
  
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));
  
  cloud1_bitmap = gbitmap_create_with_resource(RESOURCE_ID_cloud1);
  cloud2_bitmap = gbitmap_create_with_resource(RESOURCE_ID_cloud2);
  
  cloud1_layer = bitmap_layer_create(GRect(32, 35, 80, 80));
  cloud2_layer = bitmap_layer_create(GRect(32, 35, 80, 80));
  bitmap_layer_set_bitmap(cloud1_layer, cloud1_bitmap);
  bitmap_layer_set_bitmap(cloud2_layer, cloud2_bitmap);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(cloud1_layer));
  
  timer = app_timer_register(delta, (AppTimerCallback) timer_callback, NULL);
}

static void window_unload(Window *window) {
  app_sync_deinit(&sync);
  app_timer_cancel(timer);

  if (icon_bitmap) {
    gbitmap_destroy(icon_bitmap);
  }

  text_layer_destroy(temperature_layer);
  bitmap_layer_destroy(icon_layer);
}
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}
static void init() {
  window = window_create();
  window_set_background_color(window, GColorBlack);
  window_set_fullscreen(window, true);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });

  const int inbound_size = 64;
  const int outbound_size = 16;
  app_message_open(inbound_size, outbound_size);

  const bool animated = true;
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  window_stack_push(window, animated);
  
  //window_stack_push(s_main_window, true);
  update_time();
}


static void deinit() {
  window_destroy(window);
  gbitmap_destroy(cloud1_bitmap);
  bitmap_layer_destroy(cloud1_layer);
  gbitmap_destroy(cloud2_bitmap);
  bitmap_layer_destroy(cloud2_layer);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}

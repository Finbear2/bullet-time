#include </home/fin/.pebble-sdk/SDKs/4.9.148/sdk-core/pebble/basalt/include/pebble.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

// Globals

static DictationSession *dictationSession;

static Window *roomsWindow;
static MenuLayer *roomsLayer;

static Window *messagesWindow;
static MenuLayer *messagesLayer;

static Window *loadingWindow;
static TextLayer *loadingTextLayer;

char rooms[32][32];
int roomsCounter = 0;

char messages[12][128];
char senders[12][128];
int messagesCounter = 1;




// Message Functions

static void get_room_messages(const char *room) {

  DictionaryIterator *iter;
  AppMessageResult res = app_message_outbox_begin(&iter);
  if(res != APP_MSG_OK) return;

  dict_write_cstring(iter, MESSAGE_KEY_TYPE,  "ROOM_MESSAGES");
  dict_write_cstring(iter, MESSAGE_KEY_ROOM_NAME, room);

  app_message_outbox_send();

}

static void send_message(const char *text) {

  DictionaryIterator *iter;
  AppMessageResult res = app_message_outbox_begin(&iter);
  if(res != APP_MSG_OK) return;

  dict_write_cstring(iter, MESSAGE_KEY_TYPE, "SEND_MESSAGE");
  dict_write_cstring(iter, MESSAGE_KEY_TEXT, text);

  app_message_outbox_send();

}




// Dictation functions

static void dictation_callback(
  DictationSession *session,
  DictationSessionStatus status,
  char *transcription,
  void *context) {

  if (status == DictationSessionStatusSuccess) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Heard: %s", transcription);

    strncpy(messages[messagesCounter], transcription, 127);
    messages[messagesCounter][127] = '\0';

    strncpy(senders[messagesCounter], "You", 127);
    senders[messagesCounter][127] = '\0';

    messagesCounter++;

    send_message(transcription);

    menu_layer_reload_data(messagesLayer);
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Dictation failed");
    dictation_session_destroy(dictationSession);
    dictationSession = NULL;
  }
}




// Rooms Select Handlers

static void messages_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  char *text = messages[cell_index->row];

  APP_LOG(APP_LOG_LEVEL_INFO, "Clicking message : %s", text);

  if (strcmp(text, "Speech to Text") == 0) {
    APP_LOG(APP_LOG_LEVEL_INFO, "STARTING DICTATION");
    dictation_session_start(dictationSession);
  }

}

static void messages_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  menu_cell_basic_draw(ctx, cell_layer, senders[cell_index->row], messages[cell_index->row], NULL);
}

static uint16_t messages_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return messagesCounter;
}

// Rooms Select Handlers

static void rooms_select_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Opening room: %s", rooms[cell_index->row]);

  messagesCounter = 1;
  
  memset(messages, 0, sizeof(messages));
  memset(senders, 0, sizeof(senders));

  strncpy(messages[0], "Speech to Text", 127);
  messages[0][127] = '\0';

  strncpy(senders[0], "Send Message", 127);
  senders[0][127] = '\0';

  get_room_messages(rooms[cell_index->row]);

  window_stack_push(messagesWindow, true);
}

static void rooms_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *context) {
  menu_cell_basic_draw(ctx, cell_layer, rooms[cell_index->row], NULL, NULL);
}

static uint16_t rooms_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *context) {
  return roomsCounter;
}




// Inbox Handlers

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *type_tuple = dict_find(iterator, MESSAGE_KEY_TYPE);
  if (!type_tuple) return;

  const char *type = type_tuple->value->cstring;

  if (strcmp(type, "ROOMS") == 0) {

    if (roomsCounter >= 32) return;
    if (loadingWindow) {
      window_stack_remove(loadingWindow, true);
    }

    Tuple *room_tuple = dict_find(iterator, MESSAGE_KEY_ROOM_NAME);
    if (!room_tuple) return;

    const char *room = room_tuple->value->cstring;

    strncpy(rooms[roomsCounter], room, 31);
    rooms[roomsCounter][31] = '\0';
    roomsCounter++;

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Room ID: %s", room);

    if (roomsLayer) {
      menu_layer_reload_data(roomsLayer);
    }

  } else if (strcmp(type, "MESSAGE") == 0) {
    if (messagesCounter >= 12) return;

    Tuple *sender_tuple = dict_find(iterator, MESSAGE_KEY_SENDER);
    if (!sender_tuple) return;

    const char *sender = sender_tuple->value->cstring;

    Tuple *text_tuple = dict_find(iterator, MESSAGE_KEY_TEXT);
    if (!text_tuple) return;

    const char *text = text_tuple->value->cstring;

    strncpy(messages[messagesCounter], text, 127);
    messages[messagesCounter][127] = '\0';

    strncpy(senders[messagesCounter], sender, 127);
    senders[messagesCounter][127] = '\0';
    messagesCounter++;

    APP_LOG(APP_LOG_LEVEL_DEBUG, "Message Received: %s", text);

    if (messagesLayer) {
      menu_layer_reload_data(messagesLayer);
    }
  } else if (strcmp(type, "NOT_CONF") == 0) {
    
  }
  
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}




// Rooms Window Handlers

static void rooms_window_load(Window *window) {

  Layer *windowLayer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(windowLayer);

  roomsLayer = menu_layer_create(bounds);

  menu_layer_set_click_config_onto_window(roomsLayer, window);

  menu_layer_set_callbacks(roomsLayer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = rooms_get_num_rows_callback,
    .draw_row = rooms_draw_row_callback,
    .select_click = rooms_select_callback
  });

  layer_add_child(windowLayer, menu_layer_get_layer(roomsLayer));

}

static void rooms_window_unload(Window *window) {
  menu_layer_destroy(roomsLayer);
}


// Messages Window Handlers

static void messages_window_load(Window *window) {

  Layer *windowLayer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(windowLayer);

  messagesLayer = menu_layer_create(bounds);

  menu_layer_set_click_config_onto_window(messagesLayer, window);

  menu_layer_set_callbacks(messagesLayer, NULL, (MenuLayerCallbacks) {
    .get_num_rows = messages_get_num_rows_callback,
    .draw_row = messages_draw_row_callback,
    .select_click = messages_select_callback
  });

  layer_add_child(windowLayer, menu_layer_get_layer(messagesLayer));

}

static void messages_window_unload(Window *window) {
  menu_layer_destroy(messagesLayer);

  if (dictationSession) {
    dictation_session_destroy(dictationSession);
    dictationSession = NULL;
  }
}


// Loading Window Handlers

static void loading_window_load(Window *window) {

  Layer *windowLayer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(windowLayer);

  loadingTextLayer = text_layer_create(bounds);

  text_layer_set_background_color(loadingTextLayer, GColorWhite);
  text_layer_set_text_alignment(loadingTextLayer, GTextAlignmentCenter);
  text_layer_set_text_color(loadingTextLayer, GColorBlack);
  text_layer_set_font(loadingTextLayer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_text(loadingTextLayer, "Loading...");

  layer_add_child(windowLayer, text_layer_get_layer(loadingTextLayer));

}

static void loading_window_unload(Window *window) {
  text_layer_destroy(loadingTextLayer);
}




// Basic Handlers

static void init() {
  messagesWindow = window_create();

  window_set_window_handlers(messagesWindow, (WindowHandlers) {
    .load = messages_window_load,
    .unload = messages_window_unload
  });

  roomsWindow = window_create();

  window_set_window_handlers(roomsWindow, (WindowHandlers) {
    .load = rooms_window_load,
    .unload = rooms_window_unload
  });

  loadingWindow = window_create();

  window_set_window_handlers(loadingWindow, (WindowHandlers) {
    .load = loading_window_load,
    .unload = loading_window_unload
  });

  dictationSession = dictation_session_create(
      sizeof(messages[0]),
      dictation_callback,
      NULL
  );

  dictation_session_enable_confirmation(dictationSession, true);

  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);

  const int inbox_size = 1024;
  const int outbox_size = 128;
  app_message_open(inbox_size, outbox_size);
  window_stack_push(roomsWindow, true);
  window_stack_push(loadingWindow, false);
}

static void deinit() {
  window_destroy(roomsWindow);
  window_destroy(messagesWindow);
  window_destroy(loadingWindow);

  if (dictationSession) {
    dictation_session_destroy(dictationSession);
    dictationSession = NULL;
  }
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
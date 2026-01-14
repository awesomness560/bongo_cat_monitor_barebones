#include <lvgl.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <time.h>
#include "Free_Fonts.h"
#include "animations_sprites.h"

// WiFi credentials - CHANGE THESE TO YOUR NETWORK
const char* ssid = "Mavric1";
const char* password = "Sharad12";

// Time configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -6 * 3600;  // Change this for your timezone (PST = -8 hours)
const int daylightOffset_sec = 3600;   // 1 hour for daylight saving

// Display settings
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

// Configuration settings structure - simplified for time-only display
struct BongoCatSettings {
    bool time_format_24h = false;
    int sleep_timeout_minutes = 5;
    uint32_t checksum = 0;  // For validation
};

// EEPROM settings
#define SETTINGS_ADDRESS 0
#define SETTINGS_SIZE sizeof(BongoCatSettings)
#define EEPROM_SIZE 512  // ESP32 EEPROM size

// Global settings instance
BongoCatSettings settings;

TFT_eSPI tft = TFT_eSPI();

// LVGL display buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SCREEN_WIDTH * 10];

// Animation system with sprites
sprite_manager_t sprite_manager;
lv_obj_t * cat_canvas = NULL;
uint32_t last_frame_time = 0;

// Cat positioning
#define CAT_SIZE 64   // Base sprite size - will be zoomed 4x for display

// Display objects - only time
lv_obj_t * screen = NULL;
lv_obj_t * time_label = NULL;

// Time data
String current_time_str = "12:34";  // Default fallback time
bool wifi_time_synced = false;

// Function to flush the display buffer
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

// Update time display  
void updateTimeDisplay() {
    if (time_label) {
        String display_time;
        
        if (wifi_time_synced) {
            // Get current time from system
            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) {
                char timeStr[6];
                strftime(timeStr, sizeof(timeStr), "%H:%M", &timeinfo);
                display_time = String(timeStr);
                
                // Convert to 12-hour format if needed
                if (!settings.time_format_24h) {
                    int hour = timeinfo.tm_hour;
                    String minute = String(timeinfo.tm_min < 10 ? "0" : "") + String(timeinfo.tm_min);
                    String ampm = (hour >= 12) ? "PM" : "AM";
                    
                    if (hour == 0) hour = 12;      // 00:xx -> 12:xx AM
                    else if (hour > 12) hour -= 12; // 13:xx -> 1:xx PM
                    
                    display_time = String(hour) + ":" + minute + " " + ampm;
                }
            } else {
                display_time = current_time_str; // Fallback to static time
            }
        } else {
            // Use static fallback time
            display_time = current_time_str;
            
            // Convert static time to 12-hour format if needed
            if (!settings.time_format_24h && current_time_str.length() == 5) {
                int hour = current_time_str.substring(0, 2).toInt();
                String minute = current_time_str.substring(3, 5);
                String ampm = (hour >= 12) ? "PM" : "AM";
                
                if (hour == 0) hour = 12;      // 00:xx -> 12:xx AM
                else if (hour > 12) hour -= 12; // 13:xx -> 1:xx PM
                
                display_time = String(hour) + ":" + minute + " " + ampm;
            }
        }
        
        lv_label_set_text(time_label, display_time.c_str());
    }
}

// Simple WiFi time sync
void syncTimeFromWiFi() {
    Serial.println("📡 Connecting to WiFi...");
    WiFi.begin(ssid, password);
    
    // Try to connect for 10 seconds
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi connected!");
        Serial.print("📍 IP address: ");
        Serial.println(WiFi.localIP());
        
        // Configure time
        Serial.println("🕐 Syncing time from NTP server...");
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        
        // Wait for time sync (up to 5 seconds)
        struct tm timeinfo;
        int sync_attempts = 0;
        while (!getLocalTime(&timeinfo) && sync_attempts < 10) {
            delay(500);
            sync_attempts++;
        }
        
        if (getLocalTime(&timeinfo)) {
            wifi_time_synced = true;
            Serial.println("✅ Time synced successfully!");
            
            // Print current time for debugging
            char timeStr[20];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
            Serial.println("🕐 Current time: " + String(timeStr));
        } else {
            Serial.println("❌ Failed to sync time from NTP server");
            Serial.println("⚠️ Using fallback time instead");
        }
        
        // Disconnect WiFi to save power
        Serial.println("📴 Disconnecting WiFi to save power");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    } else {
        Serial.println("\n❌ WiFi connection failed!");
        Serial.println("⚠️ Check your WiFi credentials in the code");
        Serial.println("⚠️ Using fallback time (12:34)");
    }
}

// Settings management functions
uint32_t calculateChecksum(const BongoCatSettings* s) {
    // Simple checksum calculation (excluding checksum field itself)
    uint32_t sum = 0;
    const uint8_t* data = (const uint8_t*)s;
    size_t size = sizeof(BongoCatSettings) - sizeof(uint32_t); // Exclude checksum field
    
    for (size_t i = 0; i < size; i++) {
        sum += data[i];
    }
    return sum;
}

bool validateSettings(const BongoCatSettings* s) {
    // Check if settings are within valid ranges
    if (s->sleep_timeout_minutes < 1 || s->sleep_timeout_minutes > 60) return false;
    
    // Check if checksum matches
    uint32_t expected_checksum = calculateChecksum(s);
    return (s->checksum == expected_checksum);
}

void saveSettings() {
    settings.checksum = calculateChecksum(&settings);
    EEPROM.put(SETTINGS_ADDRESS, settings);
    EEPROM.commit();
}

void loadSettings() {
    BongoCatSettings temp_settings;
    EEPROM.get(SETTINGS_ADDRESS, temp_settings);
    
    if (validateSettings(&temp_settings)) {
        settings = temp_settings;
    } else {
        resetSettings();
    }
}

void resetSettings() {
    // Reset to default values
    settings.time_format_24h = true;
    settings.sleep_timeout_minutes = 5;
    settings.checksum = calculateChecksum(&settings);
}

// Sprite management functions
void sprite_manager_init(sprite_manager_t* manager) {
    // Initialize all layers to basic state
    manager->current_sprites[LAYER_BODY] = &standardbody1;
    manager->current_sprites[LAYER_FACE] = &stock_face;
    manager->current_sprites[LAYER_TABLE] = &table1;
    manager->current_sprites[LAYER_PAWS] = &twopawsup;  // Default paws up
    manager->current_sprites[LAYER_EFFECTS] = NULL;
    
    manager->current_state = ANIM_STATE_IDLE_STAGE1;
    manager->state_start_time = millis();
    manager->blink_timer = millis() + random(3000, 8000);
    manager->ear_twitch_timer = millis() + random(10000, 30000);
    manager->effect_timer = 0;
    manager->effect_frame = 0;
    manager->paw_animation_active = false;
    manager->paw_frame = 0;
    manager->paw_timer = 0;
    manager->animation_speed_ms = 200;  // Default speed
    manager->click_effect_left = false;
    
    // Enhanced animation control - standalone mode
    manager->idle_progression_enabled = true;  // Enable automatic progression
    manager->last_typing_time = 0;
    manager->is_streak_mode = false;
    
    // Initialize animation state variables
    manager->blink_start_time = 0;
    manager->blinking = false;
    manager->ear_twitch_start_time = 0;
    manager->ear_twitching = false;
}

// Calculate adaptive sleep stage timing based on user's timeout setting
void calculateSleepStageTiming(int timeout_minutes, unsigned long* stage1_ms, unsigned long* stage2_ms, unsigned long* stage3_ms) {
    unsigned long total_ms = (unsigned long)timeout_minutes * 60 * 1000;
    
    // Define minimums and maximums for each stage
    unsigned long min_stage2 = 5000;   // 5 seconds minimum
    unsigned long max_stage2 = 60000;  // 1 minute maximum
    unsigned long min_stage3 = 3000;   // 3 seconds minimum  
    unsigned long max_stage3 = 30000;  // 30 seconds maximum
    
    // Calculate based on timeout range for optimal user experience
    if (timeout_minutes <= 3) {
        // Short timeouts: quick but visible progression
        *stage2_ms = max(min_stage2, min(max_stage2, (unsigned long)(total_ms * 0.25)));
        *stage3_ms = max(min_stage3, min(max_stage3, (unsigned long)(total_ms * 0.15)));
    } else if (timeout_minutes <= 10) {
        // Medium timeouts: balanced progression
        *stage2_ms = max(min_stage2, min(max_stage2, (unsigned long)(total_ms * 0.20)));
        *stage3_ms = max(min_stage3, min(max_stage3, (unsigned long)(total_ms * 0.10)));
    } else {
        // Long timeouts: mostly normal, quick sleep transition
        *stage2_ms = max(min_stage2, min(max_stage2, (unsigned long)(total_ms * 0.15)));
        *stage3_ms = max(min_stage3, min(max_stage3, (unsigned long)(total_ms * 0.05)));
    }
    
    // Stage 1 gets the remainder to ensure total equals user setting
    *stage1_ms = total_ms - *stage2_ms - *stage3_ms;
}

void sprite_manager_update(sprite_manager_t* manager, uint32_t current_time) {
    // Standalone mode - automatic idle progression
    if (manager->idle_progression_enabled) {
        // Calculate adaptive timing based on current sleep timeout setting
        unsigned long stage1_duration, stage2_duration, stage3_duration;
        calculateSleepStageTiming(settings.sleep_timeout_minutes, &stage1_duration, &stage2_duration, &stage3_duration);
        
        if (manager->current_state == ANIM_STATE_IDLE_STAGE1) {
            if (current_time - manager->state_start_time > stage1_duration) {
                sprite_manager_set_state(manager, ANIM_STATE_IDLE_STAGE2, current_time);
            }
        } else if (manager->current_state == ANIM_STATE_IDLE_STAGE2) {
            if (current_time - manager->state_start_time > stage2_duration) {
                sprite_manager_set_state(manager, ANIM_STATE_IDLE_STAGE3, current_time);
            }
        } else if (manager->current_state == ANIM_STATE_IDLE_STAGE3) {
            if (current_time - manager->state_start_time > stage3_duration) {
                sprite_manager_set_state(manager, ANIM_STATE_IDLE_STAGE4, current_time);
            }
        }
    }
    
    // Handle paw positioning based on state
    if (manager->current_state == ANIM_STATE_IDLE_STAGE1) {
        manager->current_sprites[LAYER_PAWS] = &twopawsup;  // Visible paws for stage 1
    } else if (manager->current_state >= ANIM_STATE_IDLE_STAGE2 && 
               manager->current_state <= ANIM_STATE_IDLE_STAGE4) {
        manager->current_sprites[LAYER_PAWS] = NULL;  // Hidden paws for stages 2-4
    }
    
    // Handle sleepy effects animation (for IDLE_STAGE4)
    if (manager->current_state == ANIM_STATE_IDLE_STAGE4) {
        if (current_time - manager->effect_timer > 1000) { // Change effect every second
            manager->effect_frame = (manager->effect_frame + 1) % 3;
            switch (manager->effect_frame) {
                case 0: manager->current_sprites[LAYER_EFFECTS] = &sleepy1; break;
                case 1: manager->current_sprites[LAYER_EFFECTS] = &sleepy2; break;
                case 2: manager->current_sprites[LAYER_EFFECTS] = &sleepy3; break;
            }
            manager->effect_timer = current_time;
        }
    } else {
        manager->current_sprites[LAYER_EFFECTS] = NULL;  // Clear effects for other states
    }
    
    // Handle automatic blinking (only when awake, not during sleep)
    bool can_blink = (manager->current_state != ANIM_STATE_IDLE_STAGE3 && 
                      manager->current_state != ANIM_STATE_IDLE_STAGE4);
    
    if (!manager->blinking && current_time >= manager->blink_timer && can_blink) {
        // Start blink
        manager->blinking = true;
        manager->blink_start_time = current_time;
        manager->current_sprites[LAYER_FACE] = &blink_face;
    } else if (manager->blinking && current_time - manager->blink_start_time > 200) {
        // End blink after 200ms
        manager->blinking = false;
        // Restore normal face after blink based on current state
        if (manager->current_state == ANIM_STATE_IDLE_STAGE3 || 
            manager->current_state == ANIM_STATE_IDLE_STAGE4) {
            manager->current_sprites[LAYER_FACE] = &sleepy_face;
        } else {
            manager->current_sprites[LAYER_FACE] = &stock_face;  // Default face
        }
        // Set next blink time (only if can still blink)
        if (can_blink) {
            manager->blink_timer = current_time + random(3000, 8000);
        } else {
            // When entering sleep, set longer blink timer for when waking up
            manager->blink_timer = current_time + random(5000, 10000);
        }
    }
    
    // Handle ear twitch
    if (!manager->ear_twitching && current_time >= manager->ear_twitch_timer) {
        // Start ear twitch
        manager->ear_twitching = true;
        manager->ear_twitch_start_time = current_time;
        manager->current_sprites[LAYER_BODY] = &bodyeartwitch;
    } else if (manager->ear_twitching && current_time - manager->ear_twitch_start_time > 500) {
        // End ear twitch after 500ms
        manager->ear_twitching = false;
        manager->current_sprites[LAYER_BODY] = &standardbody1;
        // Set next ear twitch time
        manager->ear_twitch_timer = current_time + random(10000, 30000);
    }
}

void sprite_manager_set_state(sprite_manager_t* manager, animation_state_t new_state, uint32_t current_time) {
    manager->current_state = new_state;
    manager->state_start_time = current_time;
    
    // Set appropriate sprites based on state
    manager->current_sprites[LAYER_BODY] = &standardbody1;
    manager->current_sprites[LAYER_TABLE] = &table1;
    
    switch (new_state) {
        case ANIM_STATE_IDLE_STAGE1:
            // Stage 1: Stock face with paws up (hands visible above table)
            manager->current_sprites[LAYER_PAWS] = &twopawsup;
            manager->current_sprites[LAYER_EFFECTS] = NULL;
            manager->current_sprites[LAYER_FACE] = &stock_face;
            break;
            
        case ANIM_STATE_IDLE_STAGE2:
            // Stage 2: Stock face + hands gone (underneath table)
            manager->current_sprites[LAYER_PAWS] = NULL;
            manager->current_sprites[LAYER_EFFECTS] = NULL;
            manager->current_sprites[LAYER_FACE] = &stock_face;
            break;
            
        case ANIM_STATE_IDLE_STAGE3:
            // Stage 3: Sleepy face + hands gone
            manager->current_sprites[LAYER_PAWS] = NULL;
            manager->current_sprites[LAYER_EFFECTS] = NULL;
            manager->current_sprites[LAYER_FACE] = &sleepy_face;
            break;
            
        case ANIM_STATE_IDLE_STAGE4:
            // Stage 4: Sleepy face + hands gone + sleepy effects
            manager->current_sprites[LAYER_PAWS] = NULL;
            manager->current_sprites[LAYER_FACE] = &sleepy_face;
            manager->effect_timer = current_time;
            manager->effect_frame = 0;
            break;
            
        default:
            // Default to idle stage 1
            manager->current_sprites[LAYER_PAWS] = &twopawsup;
            manager->current_sprites[LAYER_EFFECTS] = NULL;
            manager->current_sprites[LAYER_FACE] = &stock_face;
            break;
    }
}

void sprite_render_layers(sprite_manager_t* manager, lv_obj_t* canvas, uint32_t current_time) {
    // Clear canvas with minimal operations
    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_TRANSP);
    
    // Render layers in order (back to front) with optimized drawing
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);  // Initialize once outside loop
    
    for (int layer = 0; layer < NUM_LAYERS; layer++) {
        const lv_img_dsc_t* sprite = manager->current_sprites[layer];
        if (sprite) {
            // Draw sprite at origin (0,0) - canvas will be zoomed to 4x size
            // Using single draw call per sprite to minimize operations
            lv_canvas_draw_img(canvas, 0, 0, sprite, &img_dsc);
        }
    }
}

// Helper function to get state name for debugging
const char* get_state_name(animation_state_t state) {
    switch (state) {
        case ANIM_STATE_IDLE_STAGE1: return "IDLE_STAGE1";
        case ANIM_STATE_IDLE_STAGE2: return "IDLE_STAGE2";
        case ANIM_STATE_IDLE_STAGE3: return "IDLE_STAGE3";
        case ANIM_STATE_IDLE_STAGE4: return "IDLE_STAGE4";
        case ANIM_STATE_TYPING_SLOW: return "TYPING_SLOW";
        case ANIM_STATE_TYPING_NORMAL: return "TYPING_NORMAL";
        case ANIM_STATE_TYPING_FAST: return "TYPING_FAST";
        case ANIM_STATE_BLINKING: return "BLINKING";
        case ANIM_STATE_EAR_TWITCH: return "EAR_TWITCH";
        default: return "UNKNOWN";
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("🐱 Bongo Cat ESP32 - Starting up...");
    
    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);
    loadSettings();
    
    // Try to sync time from WiFi
    syncTimeFromWiFi();
    
    // Initialize display
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_WHITE);
    
    // Initialize LVGL
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * 10);
    
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    
    // Initialize sprite manager
    sprite_manager_init(&sprite_manager);
    
    // Create UI
    createBongoCat();
    
    // Final status
    if (wifi_time_synced) {
        Serial.println("✅ Bongo Cat Ready - Real time synced!");
    } else {
        Serial.println("⚠️ Bongo Cat Ready - Using fallback time (12:34)");
    }
}

void createBongoCat() {
    // Create main screen
    screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    
    // Create cat canvas with proper sizing
    cat_canvas = lv_canvas_create(screen);
    
    // Use 64x64 base sprite size for the canvas buffer
    static lv_color_t canvas_buf[CAT_SIZE * CAT_SIZE];  // 64x64 buffer
    lv_canvas_set_buffer(cat_canvas, canvas_buf, CAT_SIZE, CAT_SIZE, LV_IMG_CF_TRUE_COLOR);
    
    // Apply 4x zoom to make 64x64 sprites appear as 256x256 on screen
    lv_img_set_zoom(cat_canvas, 1024);  // 4x zoom: 64x64 -> 256x256 pixels
    lv_img_set_antialias(cat_canvas, false);  // Keep pixels crisp and blocky
    
    // Position cat: original alignment method + 3 cat pixels right + a bit lower
    lv_obj_align(cat_canvas, LV_ALIGN_CENTER, 12, 50);  // 12px right (3 cat pixels), 50px lower
    
    // Create time label (top right) - pixelated font
    time_label = lv_label_create(screen);
    lv_label_set_text(time_label, "12:34");
    lv_obj_set_style_text_font(time_label, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(time_label, lv_color_black(), 0);
    lv_obj_align(time_label, LV_ALIGN_TOP_RIGHT, -5, 5);
    
    // Initial render
    sprite_render_layers(&sprite_manager, cat_canvas, millis());
}

void loop() {
    uint32_t current_time = millis();
    
    // Update sprite manager
    sprite_manager_update(&sprite_manager, current_time);
    
    // Render sprites at 60 FPS (every ~16ms)
    if (current_time - last_frame_time >= 16) {
        sprite_render_layers(&sprite_manager, cat_canvas, current_time);
        last_frame_time = current_time;
    }
    
    // Update time display periodically
    static uint32_t last_time_update = 0;
    if (current_time - last_time_update > 1000) {
        updateTimeDisplay();
        last_time_update = current_time;
    }
    
    // Update LVGL
    lv_timer_handler();
    
    delay(2);  // Small delay for stability
}
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_twai.h"
#include "esp_twai_onchip.h"

#define TWAI_SENDER_TX_GPIO     21
#define TWAI_SENDER_RX_GPIO     22
#define TWAI_QUEUE_DEPTH        10
#define TWAI_BITRATE            1000000 // 1 Mbps

// Message IDs
#define TWAI_DATA_ID            0x100
#define TWAI_HEARTBEAT_ID       0x101
#define TWAI_DATA_LEN           8

static const char* TAG = "TWAI_SENDER";

static twai_node_handle_t sender_node = NULL;
static TaskHandle_t recovery_task_handle = NULL;

// Transmission completion callback
static IRAM_ATTR bool on_tx_done_callback(twai_node_handle_t handle, const twai_tx_done_event_data_t* edata, void* user_ctx) {
    if (!edata->is_tx_success) {
        ESP_EARLY_LOGW(TAG, "Failed to transmit message, ID: 0x%" PRIX32, edata->done_tx_frame->header.id);
    } else {
        ESP_EARLY_LOGI(TAG, "Successfully transmitted message, ID: 0x%" PRIX32, edata->done_tx_frame->header.id);
    }
    return false;
}

// Bus error callback
static IRAM_ATTR bool on_error_callback(twai_node_handle_t handle, const twai_error_event_data_t* edata, void* user_ctx) {
    ESP_EARLY_LOGW(TAG, "TWAI node error: 0x%x", edata->err_flags.val);
    return false; // No task wake required
}

// Helper function to convert error state to string
static const char* twai_error_state_to_str(twai_error_state_t state)
{
    switch (state) {
        case TWAI_ERROR_ACTIVE:  return "ACTIVE";
        case TWAI_ERROR_WARNING: return "WARNING";
        case TWAI_ERROR_PASSIVE: return "PASSIVE";
        case TWAI_ERROR_BUS_OFF: return "BUS_OFF";
        default:                 return "UNKNOWN";
    }
}

// Bus state change callback
static IRAM_ATTR bool on_state_change_callback(twai_node_handle_t handle, const twai_state_change_event_data_t *edata, void *user_ctx) {
    ESP_EARLY_LOGW(TAG, "TWAI node state changed: %s -> %s",
                   twai_error_state_to_str(edata->old_sta),
                   twai_error_state_to_str(edata->new_sta));
    
    BaseType_t higher_priority_task_woken = pdFALSE;

    switch (edata->new_sta) {
        case TWAI_ERROR_ACTIVE:
            ESP_EARLY_LOGI(TAG, "TWAI node is active.");
            break;
        case TWAI_ERROR_WARNING:
            ESP_EARLY_LOGW(TAG, "TWAI node is in warning state.");
            break;
        case TWAI_ERROR_PASSIVE:
            ESP_EARLY_LOGW(TAG, "TWAI node is in passive state.");
            break;
        case TWAI_ERROR_BUS_OFF:
            ESP_EARLY_LOGE(TAG, "TWAI node is bus-off. Attempting recovery...");
            // Start a recovery task to handle bus-off state
            vTaskNotifyGiveFromISR(recovery_task_handle, &higher_priority_task_woken);
            break;
        default:
            ESP_EARLY_LOGW(TAG, "Unknown TWAI node state.");
            break;
    }

    return (higher_priority_task_woken == pdTRUE); // Return whether a higher priority task was woken
}

// Task to handle bus recovery
static void bus_recovery_task(void *arg) {
    while (1) {
        // Wait for notification from the state change callback
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        ESP_LOGW(TAG, "Bus-off detected. Recovering in 1 s...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_err_t err = twai_node_recover(sender_node);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initiate recovery: %s", esp_err_to_name(err));
        }
        // When the recovery is complete, on_state_change will trigger again
        // (TWAI_ERROR_BUS_OFF -> TWAI_ERROR_ACTIVE), so we can log the recovery success there.
    }
}

// Task to send data
static void data_tx_task(void *arg) {
    uint8_t counter = 0;
    uint8_t data[TWAI_DATA_LEN] = {0};

    twai_frame_t frame = {
        .header = {
            .id = TWAI_DATA_ID,
            .dlc = TWAI_DATA_LEN,
            .ide = 0, // Standard ID
            .rtr = 0, // Data frame
        },
        .buffer_len = TWAI_DATA_LEN,
        .tx_queue_priority = 1, // Normal priority
    };

    while (1) {
        // Prepare data
        for (int i = 0; i < TWAI_DATA_LEN; i++) {
            data[i] = counter + i;
        }
        frame.buffer = data;

        ESP_LOGI(TAG, "[#%u] Sending message 0x%X", counter, TWAI_DATA_ID);
        esp_err_t ret = twai_node_transmit(sender_node, &frame, 1000);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to queue message 0x%X: %s", TWAI_DATA_ID, esp_err_to_name(ret));
        } else {
            counter++;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

// Heartbeat task
static void heartbeat_tx_task(void *arg) {
    uint8_t sequence = 0;
    uint8_t heartbeat_data[1] = {0};

    twai_frame_t heartbeat_frame = {
        .header = {
            .id = TWAI_HEARTBEAT_ID,
            .dlc = 1, // Heartbeat data length
            .ide = 0, // Standard ID
            .rtr = 0, // Data frame
        },
        .buffer_len = 1,
        .tx_queue_priority = 1, // Normal priority
    };

    while (1) {
        heartbeat_data[0] = sequence;
        heartbeat_frame.buffer = heartbeat_data;

        ESP_LOGI(TAG, "[#%u] Sending heartbeat 0x%X", sequence, TWAI_HEARTBEAT_ID);
        esp_err_t ret = twai_node_transmit(sender_node, &heartbeat_frame, 1000);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to queue heartbeat: %s", esp_err_to_name(ret));
        } else {
            sequence++;
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void) {
    twai_onchip_node_config_t node_config = {
        .io_cfg = {
            .tx = TWAI_SENDER_TX_GPIO,
            .rx = TWAI_SENDER_RX_GPIO,
            .quanta_clk_out = GPIO_NUM_NC,
            .bus_off_indicator = GPIO_NUM_NC,
        },
        .bit_timing = {
            .bitrate = TWAI_BITRATE,
        },
        .flags = {
            .enable_self_test = 1, // Enable self-test mode for testing without bus
        },
        .fail_retry_cnt = 3, // Retry 3 times on failure
        .tx_queue_depth = TWAI_QUEUE_DEPTH,
    };

    // Create TWAI node
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &sender_node));

    // Register callbacks
    twai_event_callbacks_t callbacks = {
        .on_tx_done = on_tx_done_callback,
        .on_error = on_error_callback,
        .on_state_change = on_state_change_callback,
    };
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(sender_node, &callbacks, NULL));

    // Create recovery task before enabling the TWAI node to ensure it can handle bus-off events
    xTaskCreate(bus_recovery_task, "bus_recovery_task", 4096, NULL, 5, &recovery_task_handle);
    
    // Enable the TWAI node
    ESP_ERROR_CHECK(twai_node_enable(sender_node));
    ESP_LOGI(TAG, "TWAI sender started, sending messages with ID 0x%X and 0x%X", TWAI_DATA_ID, TWAI_HEARTBEAT_ID);

    // Create data transmission task
    xTaskCreate(data_tx_task, "data_tx_task", 4096, NULL, 5, NULL);

    // Create heartbeat transmission task
    xTaskCreate(heartbeat_tx_task, "heartbeat_tx_task", 4096, NULL, 5, NULL);
}

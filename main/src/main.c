#include "driver/i2c.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"


#include "display.h"
#include "main.h"
#include "measurment.h"

#define I2C_MASTER_SCL_IO           22
#define I2C_MASTER_SDA_IO           21
#define I2C_MASTER_FREQ_HZ          400000
#define I2C_MASTER_TX_BUF_DISABLE   0       
#define I2C_MASTER_RX_BUF_DISABLE   0

#define DISP_STR_LENGTH 256

static const char *TAG_APP = "APP";

static SemaphoreHandle_t i2c_smphr;
static QueueHandle_t sensors_queue;
static QueueHandle_t display_queue;
static QueueHandle_t logging_queue;
static QueueHandle_t buzzer_queue;

static inline esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(I2C_MASTER_NUM, &conf);

    return i2c_driver_install(
        I2C_MASTER_NUM, conf.mode, 
        I2C_MASTER_RX_BUF_DISABLE, 
        I2C_MASTER_TX_BUF_DISABLE, 
        0
    );
}

/**
 * @brief Task for sending measurment data to uart
*/
static void uart_log_task(void *arg)
{
    const QueueHandle_t queue = (QueueHandle_t) arg;
    sensors_data_t sensors_data;
    char buf[DISP_STR_LENGTH];

    // disable bufferization for acceleration
    setvbuf(stdout, NULL, _IONBF, 0);   

    while(1)
    {
        xQueueReceive(queue, &sensors_data, portMAX_DELAY);

        snprintf(buf, sizeof(buf), "%.2f,%.2f,%.2f,%.2f,%d,%d,%d;", 
            sensors_data.aht21.temperature,
            sensors_data.aht21.humidity,
            sensors_data.bmp280.temperature,
            sensors_data.bmp280.pressure,
            sensors_data.ens160.aqi,
            sensors_data.ens160.tvoc,
            sensors_data.ens160.eco2
        );
        fputs(buf, stdout);
    }
}

static void buzzer_task(void *arg)
{
    const QueueHandle_t queue = (QueueHandle_t) arg;
    TickType_t duration;

    ledc_timer_config_t tcfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 2000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&tcfg));

    ledc_channel_config_t chcfg = {
        .gpio_num = 32,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&chcfg));

    while (1)
    {
        xQueueReceive(queue, &duration, portMAX_DELAY);

        ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 128));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));

        vTaskDelay(duration);

        ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
    }
}


void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(100));

    i2c_smphr = xSemaphoreCreateMutex();
    sensors_queue = xQueueCreate(1,  sizeof(sensors_data_t));
    display_queue = xQueueCreate(1,  sizeof(sensors_data_t));
    logging_queue = xQueueCreate(32, sizeof(sensors_data_t));
    buzzer_queue  = xQueueCreate(8,  sizeof(TickType_t));

    assert(i2c_smphr != NULL);
    assert(sensors_queue != 0);
    assert(display_queue != 0);
    assert(logging_queue != 0);

    ESP_LOGI(TAG_APP, "initializing I2C...");
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG_APP, "...done");
    

    display_task_config_t display_task_config = {
        .i2c_smphr = i2c_smphr,
        .queue = display_queue
    };
    xTaskCreatePinnedToCore(display_task, "disp", 
        4096, (void*) &display_task_config, 
        ESP_TASK_PRIO_MIN + 2, NULL, tskNO_AFFINITY
    );

    xTaskCreatePinnedToCore(uart_log_task, "ulog", 
        4096, (void*) logging_queue, 
        ESP_TASK_PRIO_MIN + 1, NULL, tskNO_AFFINITY
    );

    xTaskCreatePinnedToCore(buzzer_task, "buzz", 
        2048, (void*) buzzer_queue, 
        ESP_TASK_PRIO_MIN + 1, NULL, tskNO_AFFINITY
    );

    measurment_task_config_t measurment_task_config = {
        .i2c_smphr = i2c_smphr,
        .sensors_queue = sensors_queue
    };
    xTaskCreatePinnedToCore(measurment_task, "meas", 
        4096, (void*) &measurment_task_config, 
        ESP_TASK_PRIO_MIN + 3, NULL, tskNO_AFFINITY
    );

    sensors_data_t sensors_data;

    const TickType_t buzzer_duration = pdMS_TO_TICKS(500);

    while(1)
    {
        xQueueReceive(sensors_queue, &sensors_data, portMAX_DELAY);

        xQueueSend(logging_queue, &sensors_data, pdMS_TO_TICKS(50));
        xQueueSend(display_queue, &sensors_data, pdMS_TO_TICKS(50));

        if(sensors_data.ens160.aqi > 3 && sensors_data.ens160.eco2 > 1000)
            xQueueSend(buzzer_queue, &buzzer_duration, pdMS_TO_TICKS(50));
    }
}

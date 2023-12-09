#include <stdio.h>
#include "esp_crt_bundle.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "s3_auth_header.h"
#include "protocol_examples_common.h"
#include "esp_https_ota.h"

static char *TAG = "UPSERT S3";

#define AWS_ACCESS_KEY "MY_IAM_ACCESS_KEY"
#define AWS_ACCESS_SECRET "MY_IAM_ACCESS_SECRET"

esp_err_t on_client_data(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA)
        ESP_LOGI(TAG, "Length=%d data %.*s\n", evt->data_len, evt->data_len, (char *)evt->data);
    return ESP_OK;
}

/************************ UPLOAD UPDATE A FILE ***************************/
void upsert_file()
{
    esp_http_client_config_t esp_http_client_config = {
        .url = "https://esp32-demo-bucket.s3.amazonaws.com/test.txt",
        .method = HTTP_METHOD_PUT,
        .event_handler = on_client_data,
        .crt_bundle_attach = esp_crt_bundle_attach};
    esp_http_client_handle_t client = esp_http_client_init(&esp_http_client_config);

    char *content = "Hello AWS!";
    s3_params_t s3_params = {
        .access_key = AWS_ACCESS_KEY,
        .secret_key = AWS_ACCESS_SECRET,
        .host = "esp32-demo-bucket.s3.amazonaws.com",
        .region = "us-east-1",
        .canonical_uri = "/test.txt",
        .content = content,
        .method = "PUT",
        .should_get_time = true};

    http_client_set_aws_header(client, &s3_params);

    esp_http_client_set_post_field(client, content, strlen(content));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP GET status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}
/******************************************/

/**************** DOWNLOAD A FILE **************************/
void download_file()
{
    esp_http_client_config_t esp_http_client_config = {
        .url = "https://esp32-demo-bucket.s3.amazonaws.com/test.txt",
        .method = HTTP_METHOD_GET,
        .event_handler = on_client_data,
        .crt_bundle_attach = esp_crt_bundle_attach};
    esp_http_client_handle_t client = esp_http_client_init(&esp_http_client_config);

    char *content = "";
    s3_params_t s3_params = {
        .access_key = AWS_ACCESS_KEY,
        .secret_key = AWS_ACCESS_SECRET,
        .host = "esp32-demo-bucket.s3.amazonaws.com",
        .region = "us-east-1",
        .canonical_uri = "/test.txt",
        .content = content,
        .method = "GET",
        .should_get_time = false};

    http_client_set_aws_header(client, &s3_params);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP GET status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}
/******************************************/

/**************** Perform an OTA **************************/

static esp_err_t http_client_init(esp_http_client_handle_t esp_http_client)
{

    s3_params_t s3_params = {
        .access_key = AWS_ACCESS_KEY,
        .secret_key = AWS_ACCESS_SECRET,
        .host = "esp32-demo-bucket.s3.amazonaws.com",
        .region = "us-east-1",
        .canonical_uri = "/aws-s3-auth-headers.bin",
        .method = "GET",
        .should_get_time = false};

    http_client_set_aws_header(esp_http_client, &s3_params);

    return ESP_OK;
}

static void run_ota_task()
{
    while (true)
    {
        esp_http_client_config_t esp_http_client_config = {
            .url = "https://esp32-demo-bucket.s3.amazonaws.com/aws-s3-auth-headers.bin",
            .crt_bundle_attach = esp_crt_bundle_attach};

        esp_https_ota_config_t esp_https_ota_config = {
            .http_config = &esp_http_client_config,
            .http_client_init_cb = http_client_init};

        if (esp_https_ota(&esp_https_ota_config) == ESP_OK)
        {
            ESP_LOGI(TAG, "OTA successful");
            printf("restarting in 3 seconds\n");
            vTaskDelay(pdMS_TO_TICKS(3000));
            esp_restart();
        }
        ESP_LOGE(TAG, "Failed to update firmware");
    }
}

/******************************************/

void app_main(void)
{

    nvs_flash_init();
    esp_netif_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    printf("upload / update file\n");
    upsert_file();

    printf("downloading file\n");
    download_file();

    printf("Perform an OTA\n");
    download_file();

    const esp_partition_t *ota_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
    if (ota_partition == NULL)
    {
        printf("OTA partition not found\n");
        return;
    }
    else
    {
        run_ota_task();
    }
}
